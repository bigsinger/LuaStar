#include "star_common.h"

#include <Shlwapi.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace {

struct AlgorithmHandle final {
    BCRYPT_ALG_HANDLE value = nullptr;
    ~AlgorithmHandle() {
        if (value != nullptr) {
            BCryptCloseAlgorithmProvider(value, 0);
        }
    }
};

struct HashHandle final {
    BCRYPT_HASH_HANDLE value = nullptr;
    ~HashHandle() {
        if (value != nullptr) {
            BCryptDestroyHash(value);
        }
    }
};

void check_nt(const NTSTATUS status, const char* operation) {
    if (status < 0) {
        throw std::runtime_error(
            std::string(operation) + " 失败，NTSTATUS=" +
            std::to_string(status) + "。");
    }
}

std::vector<std::wstring> patterns_from_options(
    lua_State* state,
    const int options_index) {
    if (!lua_istable(state, options_index)) {
        return {L"*"};
    }

    const auto absolute_index = lua_absindex(state, options_index);
    lua_getfield(state, absolute_index, "patterns");
    if (lua_istable(state, -1)) {
        const auto utf8_patterns = star::table_strings(state, -1);
        lua_pop(state, 1);
        std::vector<std::wstring> result;
        result.reserve(utf8_patterns.size());
        for (const auto& pattern : utf8_patterns) {
            result.push_back(star::utf8_to_wide(pattern));
        }
        return result.empty() ? std::vector<std::wstring>{L"*"} : result;
    }
    lua_pop(state, 1);

    const auto pattern = star::option_string(
        state, absolute_index, "pattern");
    return {pattern.has_value() ? star::utf8_to_wide(*pattern) : L"*"};
}

std::vector<std::wstring> patterns_from_argument(
    lua_State* state,
    const int index) {
    if (lua_istable(state, index)) {
        const auto utf8_patterns = star::table_strings(state, index);
        std::vector<std::wstring> result;
        result.reserve(utf8_patterns.size());
        for (const auto& pattern : utf8_patterns) {
            result.push_back(star::utf8_to_wide(pattern));
        }
        return result;
    }
    return {star::utf8_to_wide(star::check_string(state, index))};
}

bool matches(
    const std::filesystem::path& path,
    const std::vector<std::wstring>& patterns) {
    const auto name = path.filename().wstring();
    return std::ranges::any_of(patterns, [&](const std::wstring& pattern) {
        return PathMatchSpecW(name.c_str(), pattern.c_str()) == TRUE;
    });
}

std::vector<std::filesystem::path> collect_files(
    const std::filesystem::path& root,
    const std::vector<std::wstring>& patterns,
    const bool recursive) {
    if (!std::filesystem::is_directory(root)) {
        throw std::runtime_error(
            "目录不存在：" + star::wide_to_utf8(root.wstring()));
    }

    std::vector<std::filesystem::path> result;
    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && matches(entry.path(), patterns)) {
                result.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (entry.is_regular_file() && matches(entry.path(), patterns)) {
                result.push_back(entry.path());
            }
        }
    }
    std::ranges::sort(result, [](const auto& left, const auto& right) {
        auto left_text = left.wstring();
        auto right_text = right.wstring();
        std::ranges::transform(left_text, left_text.begin(), ::towlower);
        std::ranges::transform(right_text, right_text.begin(), ::towlower);
        return left_text < right_text;
    });
    return result;
}

void ensure_parent(const std::filesystem::path& file) {
    const auto parent = file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

bool is_inside(
    const std::filesystem::path& possible_child,
    const std::filesystem::path& parent) {
    auto child_text = star::safe_absolute(possible_child).wstring();
    auto parent_text = star::safe_absolute(parent).wstring();
    std::ranges::transform(child_text, child_text.begin(), ::towlower);
    std::ranges::transform(parent_text, parent_text.begin(), ::towlower);
    if (!parent_text.ends_with(L'\\')) {
        parent_text.push_back(L'\\');
    }
    return child_text.starts_with(parent_text);
}

std::filesystem::path effective_destination(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    if (std::filesystem::is_directory(destination)) {
        return destination / source.filename();
    }
    return destination;
}

void copy_one(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const bool overwrite) {
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error(
            "源文件不存在：" + star::wide_to_utf8(source.wstring()));
    }
    const auto target = effective_destination(source, destination);
    ensure_parent(target);
    const auto option = overwrite
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;
    if (!std::filesystem::copy_file(source, target, option) &&
        !overwrite) {
        throw std::runtime_error(
            "目标已存在：" + star::wide_to_utf8(target.wstring()));
    }
}

std::string read_bytes(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(
            "无法读取文件：" + star::wide_to_utf8(file.wstring()));
    }
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

int exists(lua_State* state) {
    return star::guarded(state, [&] {
        lua_pushboolean(state, std::filesystem::exists(star::check_path(state, 1)));
        return 1;
    });
}

int is_file(lua_State* state) {
    return star::guarded(state, [&] {
        lua_pushboolean(
            state, std::filesystem::is_regular_file(star::check_path(state, 1)));
        return 1;
    });
}

int is_dir(lua_State* state) {
    return star::guarded(state, [&] {
        lua_pushboolean(
            state, std::filesystem::is_directory(star::check_path(state, 1)));
        return 1;
    });
}

int mkdir(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::check_path(state, 1);
        if (!path.empty()) {
            std::filesystem::create_directories(path);
        }
        lua_pushboolean(state, true);
        return 1;
    });
}

int reset_dir(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::check_path(state, 1);
        star::ensure_safe_destructive_target(path);
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        lua_pushboolean(state, true);
        return 1;
    });
}

int remove(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::check_path(state, 1);
        const bool recursive = lua_gettop(state) < 2 ||
            lua_isnil(state, 2) || lua_toboolean(state, 2) != 0;
        const bool missing_ok = lua_gettop(state) < 3 ||
            lua_isnil(state, 3) || lua_toboolean(state, 3) != 0;
        if (!std::filesystem::exists(path)) {
            if (!missing_ok) {
                throw std::runtime_error(
                    "路径不存在：" +
                    star::wide_to_utf8(path.wstring()));
            }
            lua_pushinteger(state, 0);
            return 1;
        }

        std::uintmax_t count = 0;
        if (std::filesystem::is_directory(path)) {
            if (!recursive) {
                count = std::filesystem::remove(path) ? 1 : 0;
            } else {
                star::ensure_safe_destructive_target(path);
                count = std::filesystem::remove_all(path);
            }
        } else {
            count = std::filesystem::remove(path) ? 1 : 0;
        }
        lua_pushinteger(state, static_cast<lua_Integer>(count));
        return 1;
    });
}

int copy_file(lua_State* state) {
    return star::guarded(state, [&] {
        const bool overwrite = lua_gettop(state) < 3 ||
            lua_isnil(state, 3) || lua_toboolean(state, 3) != 0;
        copy_one(
            star::check_path(state, 1),
            star::check_path(state, 2),
            overwrite);
        lua_pushboolean(state, true);
        return 1;
    });
}

int copy_tree(lua_State* state) {
    return star::guarded(state, [&] {
        const auto source = star::safe_absolute(star::check_path(state, 1));
        const auto destination =
            star::safe_absolute(star::check_path(state, 2));
        const bool overwrite = lua_gettop(state) < 3 ||
            lua_isnil(state, 3) || lua_toboolean(state, 3) != 0;
        if (!std::filesystem::is_directory(source)) {
            throw std::runtime_error(
                "源目录不存在：" +
                star::wide_to_utf8(source.wstring()));
        }
        if (source == destination || is_inside(destination, source)) {
            throw std::runtime_error("目标目录不能位于源目录内部。");
        }
        std::filesystem::create_directories(destination);
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(source)) {
            const auto relative = entry.path().lexically_relative(source);
            const auto target = destination / relative;
            if (entry.is_directory()) {
                std::filesystem::create_directories(target);
            } else if (entry.is_regular_file()) {
                copy_one(entry.path(), target, overwrite);
            }
        }
        lua_pushboolean(state, true);
        return 1;
    });
}

int copy_files(lua_State* state) {
    return star::guarded(state, [&] {
        const auto source = star::check_path(state, 1);
        const auto destination = star::check_path(state, 2);
        const auto names = star::table_strings(state, 3);
        const bool reset = lua_gettop(state) >= 4 &&
            !lua_isnil(state, 4) && lua_toboolean(state, 4) != 0;
        if (reset) {
            star::ensure_safe_destructive_target(destination);
            std::filesystem::remove_all(destination);
        }
        std::filesystem::create_directories(destination);
        for (const auto& name : names) {
            const auto relative =
                std::filesystem::path(star::utf8_to_wide(name));
            copy_one(source / relative, destination / relative, true);
        }
        lua_pushinteger(state, static_cast<lua_Integer>(names.size()));
        return 1;
    });
}

int move(lua_State* state) {
    return star::guarded(state, [&] {
        const auto source = star::check_path(state, 1);
        const auto destination = star::check_path(state, 2);
        const bool overwrite = lua_gettop(state) < 3 ||
            lua_isnil(state, 3) || lua_toboolean(state, 3) != 0;
        ensure_parent(destination);
        DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
        if (overwrite) {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }
        if (MoveFileExW(source.c_str(), destination.c_str(), flags) == FALSE) {
            throw std::runtime_error(
                "移动文件失败：" +
                star::wide_to_utf8(star::windows_error()));
        }
        lua_pushboolean(state, true);
        return 1;
    });
}

int list(lua_State* state) {
    return star::guarded(state, [&] {
        const auto root = star::check_path(state, 1);
        const int options_index = 2;
        const bool recursive =
            star::option_bool(state, options_index, "recursive", false);
        const bool relative =
            star::option_bool(state, options_index, "relative", false);
        const auto patterns = patterns_from_options(state, options_index);
        const auto files = collect_files(root, patterns, recursive);
        lua_createtable(state, static_cast<int>(files.size()), 0);
        for (std::size_t index = 0; index < files.size(); ++index) {
            star::push_path(
                state,
                relative ? files[index].lexically_relative(root) : files[index]);
            lua_seti(state, -2, static_cast<lua_Integer>(index + 1));
        }
        return 1;
    });
}

int count(lua_State* state) {
    return star::guarded(state, [&] {
        const auto root = star::check_path(state, 1);
        const auto patterns = patterns_from_argument(state, 2);
        const bool recursive = lua_gettop(state) >= 3 &&
            !lua_isnil(state, 3) && lua_toboolean(state, 3) != 0;
        const auto files = collect_files(root, patterns, recursive);
        lua_pushinteger(state, static_cast<lua_Integer>(files.size()));
        return 1;
    });
}

int remove_matching(lua_State* state) {
    return star::guarded(state, [&] {
        const auto root = star::check_path(state, 1);
        star::ensure_safe_destructive_target(root);
        const auto patterns = patterns_from_argument(state, 2);
        const bool recursive = lua_gettop(state) >= 3 &&
            !lua_isnil(state, 3) && lua_toboolean(state, 3) != 0;
        const auto files = collect_files(root, patterns, recursive);
        for (const auto& file : files) {
            std::filesystem::remove(file);
        }
        lua_pushinteger(state, static_cast<lua_Integer>(files.size()));
        return 1;
    });
}

int assert_no_match(lua_State* state) {
    return star::guarded(state, [&] {
        const auto root = star::check_path(state, 1);
        const auto patterns = patterns_from_argument(state, 2);
        const bool recursive = lua_gettop(state) < 3 ||
            lua_isnil(state, 3) || lua_toboolean(state, 3) != 0;
        const auto files = collect_files(root, patterns, recursive);
        if (!files.empty()) {
            std::string message = "发现禁止发布的文件：";
            const auto display_count = (std::min)(files.size(), std::size_t(10));
            for (std::size_t index = 0; index < display_count; ++index) {
                message.append("\n  ");
                message.append(star::wide_to_utf8(files[index].wstring()));
            }
            if (display_count < files.size()) {
                message.append("\n  ...");
            }
            throw std::runtime_error(message);
        }
        lua_pushboolean(state, true);
        return 1;
    });
}

int read(lua_State* state) {
    return star::guarded(state, [&] {
        const auto text = read_bytes(star::check_path(state, 1));
        lua_pushlstring(state, text.data(), text.size());
        return 1;
    });
}

int write(lua_State* state) {
    return star::guarded(state, [&] {
        const auto file = star::check_path(state, 1);
        std::size_t length = 0;
        const auto* data = luaL_checklstring(state, 2, &length);
        ensure_parent(file);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error(
                "无法写入文件：" + star::wide_to_utf8(file.wstring()));
        }
        stream.write(data, static_cast<std::streamsize>(length));
        if (!stream) {
            throw std::runtime_error(
                "写入文件失败：" + star::wide_to_utf8(file.wstring()));
        }
        lua_pushboolean(state, true);
        return 1;
    });
}

int replace_text(lua_State* state) {
    return star::guarded(state, [&] {
        const auto file = star::check_path(state, 1);
        const auto search = star::check_string(state, 2);
        const auto replacement = star::check_string(state, 3);
        const bool required = lua_gettop(state) < 4 ||
            lua_isnil(state, 4) || lua_toboolean(state, 4) != 0;
        if (search.empty()) {
            throw std::runtime_error("查找文本不能为空。");
        }
        auto text = read_bytes(file);
        std::size_t position = 0;
        std::size_t replacements = 0;
        while ((position = text.find(search, position)) != std::string::npos) {
            text.replace(position, search.size(), replacement);
            position += replacement.size();
            ++replacements;
        }
        if (required && replacements == 0) {
            throw std::runtime_error("没有找到要求替换的文本。");
        }
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) {
            throw std::runtime_error("无法写入替换后的文本。");
        }
        lua_pushinteger(state, static_cast<lua_Integer>(replacements));
        return 1;
    });
}

int size(lua_State* state) {
    return star::guarded(state, [&] {
        const auto bytes = std::filesystem::file_size(star::check_path(state, 1));
        lua_pushinteger(state, static_cast<lua_Integer>(bytes));
        return 1;
    });
}

int sha256(lua_State* state) {
    return star::guarded(state, [&] {
        const auto file = star::check_path(state, 1);
        AlgorithmHandle algorithm;
        check_nt(
            BCryptOpenAlgorithmProvider(
                &algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
            "BCryptOpenAlgorithmProvider");

        DWORD object_size = 0;
        DWORD result_size = 0;
        check_nt(
            BCryptGetProperty(
                algorithm.value,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&object_size),
                sizeof(object_size),
                &result_size,
                0),
            "BCryptGetProperty(BCRYPT_OBJECT_LENGTH)");
        DWORD digest_size = 0;
        check_nt(
            BCryptGetProperty(
                algorithm.value,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&digest_size),
                sizeof(digest_size),
                &result_size,
                0),
            "BCryptGetProperty(BCRYPT_HASH_LENGTH)");

        std::vector<UCHAR> object(object_size);
        std::vector<UCHAR> digest(digest_size);
        HashHandle hash;
        check_nt(
            BCryptCreateHash(
                algorithm.value,
                &hash.value,
                object.data(),
                static_cast<ULONG>(object.size()),
                nullptr,
                0,
                0),
            "BCryptCreateHash");

        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            throw std::runtime_error(
                "无法计算文件哈希：" + star::wide_to_utf8(file.wstring()));
        }
        std::array<char, 64 * 1024> buffer{};
        while (stream) {
            stream.read(buffer.data(), buffer.size());
            const auto read_count = stream.gcount();
            if (read_count > 0) {
                check_nt(
                    BCryptHashData(
                        hash.value,
                        reinterpret_cast<PUCHAR>(buffer.data()),
                        static_cast<ULONG>(read_count),
                        0),
                    "BCryptHashData");
            }
        }
        check_nt(
            BCryptFinishHash(
                hash.value,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0),
            "BCryptFinishHash");

        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (const auto byte : digest) {
            output << std::setw(2) << static_cast<unsigned int>(byte);
        }
        star::push_utf8(state, output.str());
        return 1;
    });
}

const luaL_Reg functions[] = {
    {"exists", exists},
    {"is_file", is_file},
    {"is_dir", is_dir},
    {"mkdir", mkdir},
    {"reset_dir", reset_dir},
    {"remove", remove},
    {"copy_file", copy_file},
    {"copy_tree", copy_tree},
    {"copy_files", copy_files},
    {"move", move},
    {"list", list},
    {"count", count},
    {"remove_matching", remove_matching},
    {"assert_no_match", assert_no_match},
    {"read", read},
    {"write", write},
    {"replace_text", replace_text},
    {"size", size},
    {"sha256", sha256},
    {nullptr, nullptr},
};

} // namespace

namespace star {

void register_fs_module(lua_State* state) {
    luaL_newlib(state, functions);
}

} // namespace star
