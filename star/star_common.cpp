#include "star_common.h"

#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <system_error>

namespace star {

std::wstring utf8_to_wide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (count <= 0) {
        throw std::runtime_error("文本不是有效的 UTF-8。");
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count);
    return result;
}

std::string wide_to_utf8(const std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const auto count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (count <= 0) {
        throw std::runtime_error("UTF-16 转换为 UTF-8 失败。");
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count,
        nullptr,
        nullptr);
    return result;
}

std::wstring windows_error(const DWORD code) {
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        0,
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    std::wstring result =
        length != 0 && buffer != nullptr ? std::wstring(buffer, length) : L"";
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    while (!result.empty() &&
           (result.back() == L'\r' || result.back() == L'\n')) {
        result.pop_back();
    }
    return result;
}

std::string check_string(lua_State* state, const int index) {
    std::size_t length = 0;
    const auto* value = luaL_checklstring(state, index, &length);
    return std::string(value, length);
}

std::filesystem::path check_path(lua_State* state, const int index) {
    return std::filesystem::path(utf8_to_wide(check_string(state, index)));
}

void push_utf8(lua_State* state, const std::string_view text) {
    lua_pushlstring(state, text.data(), text.size());
}

void push_wide(lua_State* state, const std::wstring_view text) {
    push_utf8(state, wide_to_utf8(text));
}

void push_path(lua_State* state, const std::filesystem::path& path) {
    push_wide(state, path.wstring());
}

bool option_bool(
    lua_State* state,
    const int table_index,
    const char* name,
    const bool fallback) {
    if (!lua_istable(state, table_index)) {
        return fallback;
    }
    lua_getfield(state, table_index, name);
    const bool result =
        lua_isnil(state, -1) ? fallback : lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return result;
}

lua_Integer option_integer(
    lua_State* state,
    const int table_index,
    const char* name,
    const lua_Integer fallback) {
    if (!lua_istable(state, table_index)) {
        return fallback;
    }
    lua_getfield(state, table_index, name);
    const auto result =
        lua_isnil(state, -1) ? fallback : luaL_checkinteger(state, -1);
    lua_pop(state, 1);
    return result;
}

std::optional<std::string> option_string(
    lua_State* state,
    const int table_index,
    const char* name) {
    if (!lua_istable(state, table_index)) {
        return std::nullopt;
    }
    lua_getfield(state, table_index, name);
    std::optional<std::string> result;
    if (!lua_isnil(state, -1)) {
        result = check_string(state, -1);
    }
    lua_pop(state, 1);
    return result;
}

std::optional<std::filesystem::path> option_path(
    lua_State* state,
    const int table_index,
    const char* name) {
    const auto value = option_string(state, table_index, name);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return std::filesystem::path(utf8_to_wide(*value));
}

std::vector<std::string> table_strings(lua_State* state, const int index) {
    luaL_checktype(state, index, LUA_TTABLE);
    const auto absolute_index = lua_absindex(state, index);
    const auto count = lua_rawlen(state, absolute_index);
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    for (lua_Unsigned item = 1; item <= count; ++item) {
        lua_geti(state, absolute_index, static_cast<lua_Integer>(item));
        result.push_back(check_string(state, -1));
        lua_pop(state, 1);
    }
    return result;
}

std::filesystem::path executable_path() {
    std::wstring buffer(512, L'\0');
    while (true) {
        const auto length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("读取程序路径失败。");
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path script_path(lua_State* state) {
    lua_Debug debug{};
    for (int level = 1; lua_getstack(state, level, &debug) != 0; ++level) {
        if (lua_getinfo(state, "S", &debug) == 0 || debug.source == nullptr ||
            debug.source[0] != '@') {
            continue;
        }
        auto result = std::filesystem::path(utf8_to_wide(debug.source + 1));
        if (result.is_relative()) {
            result = std::filesystem::absolute(result);
        }
        return result.lexically_normal();
    }
    return {};
}

std::filesystem::path safe_absolute(const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::runtime_error("路径不能为空。");
    }
    return std::filesystem::absolute(path).lexically_normal();
}

void ensure_safe_destructive_target(const std::filesystem::path& path) {
    const auto absolute = safe_absolute(path);
    if (absolute == absolute.root_path() || absolute.filename().empty()) {
        throw std::runtime_error("拒绝对文件系统根目录执行破坏性操作。");
    }
}

} // namespace star
