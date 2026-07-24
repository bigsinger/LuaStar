#include <Windows.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <conio.h>

#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

std::atomic_bool debug_enabled = false;

class Handle final {
public:
    Handle() = default;
    explicit Handle(const HANDLE value) : value_(value) {}
    ~Handle() {
        reset();
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return value_;
    }

    void reset(const HANDLE value = nullptr) noexcept {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
        value_ = value;
    }

private:
    HANDLE value_ = nullptr;
};

class TemporaryFile final {
public:
    TemporaryFile() {
        std::wstring directory(MAX_PATH + 1, L'\0');
        const auto length =
            GetTempPathW(static_cast<DWORD>(directory.size()), directory.data());
        if (length == 0 || length >= directory.size()) {
            throw std::runtime_error("读取临时目录失败。");
        }
        directory.resize(length);

        std::wstring file(MAX_PATH + 1, L'\0');
        if (GetTempFileNameW(
                directory.c_str(), L"LST", 0, file.data()) == 0) {
            throw std::runtime_error("创建临时文件失败。");
        }
        file.resize(std::wcslen(file.c_str()));
        path_ = std::move(file);
    }

    ~TemporaryFile() {
        if (!path_.empty()) {
            DeleteFileW(path_.c_str());
        }
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

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

std::string windows_error(const DWORD code = GetLastError()) {
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
    std::wstring message =
        length != 0 && buffer != nullptr ? std::wstring(buffer, length) : L"";
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return wide_to_utf8(message);
}

std::string check_string(lua_State* state, const int index) {
    std::size_t length = 0;
    const auto* text = luaL_checklstring(state, index, &length);
    return std::string(text, length);
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

template <typename Function>
int guarded(lua_State* state, Function&& function) {
    try {
        return function();
    } catch (const std::exception& exception) {
        return luaL_error(state, "%s", exception.what());
    } catch (...) {
        return luaL_error(state, "未知原生错误。");
    }
}

void write_stdout(const std::string_view text) {
    if (text.empty()) {
        return;
    }
    const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("标准输出不可用。");
    }

    DWORD mode = 0;
    DWORD written = 0;
    if (GetConsoleMode(handle, &mode) != FALSE) {
        const auto wide = utf8_to_wide(text);
        if (WriteConsoleW(
                handle,
                wide.data(),
                static_cast<DWORD>(wide.size()),
                &written,
                nullptr) == FALSE) {
            throw std::runtime_error("写入控制台失败。");
        }
    } else if (WriteFile(
                   handle,
                   text.data(),
                   static_cast<DWORD>(text.size()),
                   &written,
                   nullptr) == FALSE) {
        throw std::runtime_error("写入标准输出失败。");
    }
}

void debug_write(const std::string_view text) noexcept {
    if (!debug_enabled.load()) {
        return;
    }
    try {
        write_stdout("[star] ");
        write_stdout(text);
        write_stdout("\r\n");
    } catch (...) {
    }
}

void debug_write_error(const std::string_view text) noexcept {
    if (!debug_enabled.load()) {
        return;
    }
    try {
        const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        const bool console = handle != nullptr &&
            handle != INVALID_HANDLE_VALUE &&
            GetConsoleMode(handle, &mode) != FALSE;
        if (console) {
            CONSOLE_SCREEN_BUFFER_INFO info{};
            if (GetConsoleScreenBufferInfo(handle, &info) != FALSE) {
                SetConsoleTextAttribute(
                    handle,
                    FOREGROUND_RED | FOREGROUND_INTENSITY);
                write_stdout("[star] " + std::string(text) + "\r\n");
                SetConsoleTextAttribute(handle, info.wAttributes);
                return;
            }
        }
        debug_write(text);
    } catch (...) {
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

std::wstring quote(const std::wstring_view value) {
    if (!value.empty() &&
        value.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(value);
    }

    std::wstring result(1, L'"');
    std::size_t backslashes = 0;
    for (const auto character : value) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(character);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring command_shell() {
    std::wstring value(32768, L'\0');
    const auto length = GetEnvironmentVariableW(
        L"COMSPEC", value.data(), static_cast<DWORD>(value.size()));
    if (length != 0 && length < value.size()) {
        value.resize(length);
        return value;
    }
    return L"cmd.exe";
}

std::string read_file(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("无法读取命令输出。");
    }
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

struct RunResult final {
    lua_Integer exit_code = -1;
    std::string output;
};

RunResult execute(const std::string_view command) {
    if (command.empty()) {
        return {-1, "命令不能为空。"};
    }

    TemporaryFile output_file;
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    Handle output(CreateFileW(
        output_file.path().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr));
    if (output.get() == INVALID_HANDLE_VALUE) {
        return {-1, "无法创建命令输出文件：" + windows_error()};
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output.get();
    startup.hStdError = output.get();

    const auto shell = command_shell();
    auto command_line = quote(shell);
    command_line.append(L" /D /S /C \"");
    command_line.append(utf8_to_wide(command));
    command_line.push_back(L'"');
    std::vector<wchar_t> mutable_command(
        command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    PROCESS_INFORMATION process{};
    const auto created = CreateProcessW(
        shell.c_str(),
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startup,
        &process);
    const auto create_error = GetLastError();
    output.reset();
    if (created == FALSE) {
        return {-1, "创建进程失败：" + windows_error(create_error)};
    }

    Handle thread(process.hThread);
    Handle process_handle(process.hProcess);
    if (WaitForSingleObject(process_handle.get(), INFINITE) != WAIT_OBJECT_0) {
        return {-1, "等待进程失败：" + windows_error()};
    }

    DWORD exit_code = 0;
    if (GetExitCodeProcess(process_handle.get(), &exit_code) == FALSE) {
        return {-1, "读取进程退出码失败：" + windows_error()};
    }
    process_handle.reset();
    thread.reset();

    return {
        static_cast<lua_Integer>(exit_code),
        read_file(output_file.path()),
    };
}

bool path_starts_with(
    const std::filesystem::path& value,
    const std::filesystem::path& prefix) {
    const auto absolute_value =
        std::filesystem::absolute(value).lexically_normal();
    const auto absolute_prefix =
        std::filesystem::absolute(prefix).lexically_normal();
    auto value_part = absolute_value.begin();
    for (auto prefix_part = absolute_prefix.begin();
         prefix_part != absolute_prefix.end();
         ++prefix_part, ++value_part) {
        if (value_part == absolute_value.end() ||
            _wcsicmp(
                value_part->c_str(),
                prefix_part->c_str()) != 0) {
            return false;
        }
    }
    return true;
}

void ensure_parent(const std::filesystem::path& path) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

void ensure_safe_remove(const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::runtime_error("删除路径不能为空。");
    }
    const auto absolute =
        std::filesystem::absolute(path).lexically_normal();
    if (absolute == absolute.root_path() || absolute.filename().empty()) {
        throw std::runtime_error("拒绝删除文件系统根目录。");
    }
}

void copy_item(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    if (!std::filesystem::exists(source)) {
        throw std::runtime_error(
            "源路径不存在：" + wide_to_utf8(source.wstring()));
    }
    if (std::filesystem::is_symlink(source)) {
        throw std::runtime_error("不支持复制符号链接。");
    }

    if (std::filesystem::is_regular_file(source)) {
        ensure_parent(destination);
        std::filesystem::copy_file(
            source,
            destination,
            std::filesystem::copy_options::overwrite_existing);
        return;
    }

    if (!std::filesystem::is_directory(source)) {
        throw std::runtime_error("仅支持复制普通文件或目录。");
    }
    if (path_starts_with(destination, source)) {
        throw std::runtime_error("目标目录不能位于源目录内部。");
    }
    if (std::filesystem::exists(destination) &&
        !std::filesystem::is_directory(destination)) {
        throw std::runtime_error("目录复制目标已经是一个文件。");
    }

    std::filesystem::create_directories(destination);
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(source)) {
        const auto relative = entry.path().lexically_relative(source);
        const auto target = destination / relative;
        if (entry.is_symlink()) {
            throw std::runtime_error(
                "不支持复制符号链接：" +
                wide_to_utf8(entry.path().wstring()));
        } else if (entry.is_directory()) {
            std::filesystem::create_directories(target);
        } else if (entry.is_regular_file()) {
            ensure_parent(target);
            std::filesystem::copy_file(
                entry.path(),
                target,
                std::filesystem::copy_options::overwrite_existing);
        } else {
            throw std::runtime_error(
                "目录中包含不支持的文件类型：" +
                wide_to_utf8(entry.path().wstring()));
        }
    }
}

void move_item(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    if (!std::filesystem::exists(source)) {
        throw std::runtime_error(
            "源路径不存在：" + wide_to_utf8(source.wstring()));
    }
    if (std::filesystem::is_symlink(source)) {
        throw std::runtime_error("不支持移动符号链接。");
    }
    if (std::filesystem::exists(destination)) {
        throw std::runtime_error(
            "目标路径已存在：" + wide_to_utf8(destination.wstring()));
    }
    if (std::filesystem::is_directory(source) &&
        path_starts_with(destination, source)) {
        throw std::runtime_error("目标目录不能位于源目录内部。");
    }

    ensure_parent(destination);
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    if (!error) {
        return;
    }

    copy_item(source, destination);
    std::filesystem::remove_all(source, error);
    if (error) {
        throw std::runtime_error(
            "目标已复制，但删除源路径失败，错误码：" +
            std::to_string(error.value()));
    }
}

template <typename Function>
int file_operation(
    lua_State* state,
    const std::string_view name,
    Function&& function) {
    try {
        function();
        debug_write(std::string(name) + " 成功");
        lua_pushboolean(state, true);
        lua_pushnil(state);
    } catch (const std::filesystem::filesystem_error& exception) {
        const auto message =
            std::string("文件系统操作失败，错误码：") +
            std::to_string(exception.code().value());
        debug_write(std::string(name) + " 失败：" + message);
        lua_pushboolean(state, false);
        push_utf8(state, message);
    } catch (const std::exception& exception) {
        debug_write(std::string(name) + " 失败：" + exception.what());
        lua_pushboolean(state, false);
        push_utf8(state, exception.what());
    }
    return 2;
}

int version(lua_State* state) {
    lua_pushliteral(state, "1.0.0");
    lua_pushliteral(state, LUA_RELEASE);
    return 2;
}

int debug(lua_State* state) {
    if (lua_gettop(state) >= 1 && !lua_isnil(state, 1)) {
        luaL_checktype(state, 1, LUA_TBOOLEAN);
        debug_enabled.store(lua_toboolean(state, 1) != 0);
        debug_write("调试模式已开启");
    }
    lua_pushboolean(state, debug_enabled.load());
    return 1;
}

int path(lua_State* state) {
    return guarded(state, [&] {
        const auto script = script_path(state);
        auto directory = script.empty()
            ? std::filesystem::current_path()
            : script.parent_path();
        auto directory_text = directory.wstring();
        if (!directory_text.empty() &&
            directory_text.back() != L'\\' &&
            directory_text.back() != L'/') {
            directory_text.push_back(L'\\');
        }
        push_wide(state, directory_text);
        push_wide(state, script.empty() ? L"" : script.filename().wstring());
        return 2;
    });
}

int join(lua_State* state) {
    return guarded(state, [&] {
        const auto count = lua_gettop(state);
        if (count < 1) {
            return luaL_error(state, "path.join 至少需要一个路径参数。");
        }
        auto result = check_path(state, 1);
        for (int index = 2; index <= count; ++index) {
            result /= check_path(state, index);
        }
        push_wide(state, result.lexically_normal().wstring());
        return 1;
    });
}

int dir(lua_State* state) {
    return guarded(state, [&] {
        auto text = check_path(state, 1).parent_path().wstring();
        if (!text.empty() && text.back() != L'\\' && text.back() != L'/') {
            text.push_back(L'\\');
        }
        push_wide(state, text);
        return 1;
    });
}

int name(lua_State* state) {
    return guarded(state, [&] {
        push_wide(state, check_path(state, 1).filename().wstring());
        return 1;
    });
}

int ext(lua_State* state) {
    return guarded(state, [&] {
        push_wide(state, check_path(state, 1).extension().wstring());
        return 1;
    });
}

int run(lua_State* state) {
    return guarded(state, [&] {
        std::string command;
        const auto count = lua_gettop(state);
        for (int index = 1; index <= count; ++index) {
            std::size_t length = 0;
            const auto* value = luaL_tolstring(state, index, &length);
            if (index > 1) {
                command.push_back(' ');
            }
            command.append(value, length);
            lua_pop(state, 1);
        }

        debug_write("执行：" + command);
        const auto result = execute(command);
        const auto exit_text = "退出码：" + std::to_string(result.exit_code);
        if (result.exit_code != 0) {
            debug_write_error(exit_text);
        } else {
            debug_write(exit_text);
        }
        if (!result.output.empty() && debug_enabled.load()) {
            if (result.exit_code != 0) {
                debug_write_error("输出：");
            } else {
                debug_write("输出：");
            }
            try {
                if (result.exit_code != 0) {
                    debug_write_error(result.output);
                } else {
                    write_stdout(result.output);
                }
                if (!result.output.ends_with('\n')) {
                    if (result.exit_code == 0) {
                        write_stdout("\r\n");
                    }
                }
            } catch (...) {
            }
        }
        lua_pushinteger(state, result.exit_code);
        push_utf8(state, result.output);
        return 2;
    });
}

int copy(lua_State* state) {
    return guarded(state, [&] {
        const auto source = check_path(state, 1);
        const auto destination = check_path(state, 2);
        debug_write(
            "复制：" + wide_to_utf8(source.wstring()) +
            " -> " + wide_to_utf8(destination.wstring()));
        return file_operation(state, "复制", [&] {
            copy_item(source, destination);
        });
    });
}

int move(lua_State* state) {
    return guarded(state, [&] {
        const auto source = check_path(state, 1);
        const auto destination = check_path(state, 2);
        debug_write(
            "移动：" + wide_to_utf8(source.wstring()) +
            " -> " + wide_to_utf8(destination.wstring()));
        return file_operation(state, "移动", [&] {
            move_item(source, destination);
        });
    });
}

int env(lua_State* state) {
    return guarded(state, [&] {
        const auto directory =
            std::filesystem::absolute(check_path(state, 1)).lexically_normal();
        debug_write("加入 PATH：" + wide_to_utf8(directory.wstring()));
        return file_operation(state, "环境变量", [&] {
            if (!std::filesystem::is_directory(directory)) {
                throw std::runtime_error(
                    "目录不存在：" + wide_to_utf8(directory.wstring()));
            }

            const auto required =
                GetEnvironmentVariableW(L"PATH", nullptr, 0);
            std::wstring current;
            if (required != 0) {
                current.resize(required, L'\0');
                const auto written = GetEnvironmentVariableW(
                    L"PATH",
                    current.data(),
                    static_cast<DWORD>(current.size()));
                if (written == 0 || written >= current.size()) {
                    throw std::runtime_error("读取 PATH 环境变量失败。");
                }
                current.resize(written);
            }

            auto normalized = directory.wstring();
            while (normalized.size() > directory.root_path().wstring().size() &&
                   (normalized.back() == L'\\' ||
                    normalized.back() == L'/')) {
                normalized.pop_back();
            }

            std::size_t start = 0;
            while (start <= current.size()) {
                const auto end = current.find(L';', start);
                auto item = current.substr(
                    start,
                    end == std::wstring::npos
                        ? std::wstring::npos
                        : end - start);
                if (item.size() >= 2 && item.front() == L'"' &&
                    item.back() == L'"') {
                    item = item.substr(1, item.size() - 2);
                }
                while (!item.empty() &&
                       (item.back() == L'\\' || item.back() == L'/')) {
                    item.pop_back();
                }
                if (_wcsicmp(item.c_str(), normalized.c_str()) == 0) {
                    return;
                }
                if (end == std::wstring::npos) {
                    break;
                }
                start = end + 1;
            }

            if (!current.empty() && current.back() != L';') {
                current.push_back(L';');
            }
            current.append(normalized);
            const auto result = _wputenv_s(L"PATH", current.c_str());
            if (result != 0) {
                throw std::runtime_error(
                    "更新 PATH 环境变量失败，错误码：" +
                    std::to_string(result));
            }
        });
    });
}

int mkdir(lua_State* state) {
    return guarded(state, [&] {
        const auto count = lua_gettop(state);
        if (count < 1) {
            return luaL_error(state, "mkdir 至少需要一个目录参数。");
        }
        return file_operation(state, "创建目录", [&] {
            for (int index = 1; index <= count; ++index) {
                const auto directory = check_path(state, index);
                debug_write("检查目录：" + wide_to_utf8(directory.wstring()));
                if (directory.empty()) {
                    throw std::runtime_error("目录路径不能为空。");
                }
                if (std::filesystem::exists(directory)) {
                    if (!std::filesystem::is_directory(directory)) {
                        throw std::runtime_error("目标路径已经是一个文件。");
                    }
                    continue;
                }
                std::filesystem::create_directories(directory);
            }
        });
    });
}

int remove(lua_State* state) {
    return guarded(state, [&] {
        const auto target = check_path(state, 1);
        debug_write("删除：" + wide_to_utf8(target.wstring()));
        return file_operation(state, "删除", [&] {
            ensure_safe_remove(target);
            if (std::filesystem::exists(target) ||
                std::filesystem::is_symlink(target)) {
                std::filesystem::remove_all(target);
            }
        });
    });
}

int exists(lua_State* state) {
    return guarded(state, [&] {
        const auto target = check_path(state, 1);
        const auto status = std::filesystem::symlink_status(target);
        if (!std::filesystem::exists(status)) {
            lua_pushboolean(state, false);
            lua_pushnil(state);
        } else {
            lua_pushboolean(state, true);
            if (std::filesystem::is_regular_file(status)) {
                lua_pushliteral(state, "file");
            } else if (std::filesystem::is_directory(status)) {
                lua_pushliteral(state, "dir");
            } else {
                lua_pushliteral(state, "other");
            }
        }
        return 2;
    });
}

int pause(lua_State* state) {
    return guarded(state, [&] {
        const auto message = lua_gettop(state) >= 1 && !lua_isnil(state, 1)
            ? check_string(state, 1)
            : std::string("按任意键继续 . . .");
        write_stdout(message);
        const auto key = _getwch();
        if (key == 0 || key == 0xE0) {
            static_cast<void>(_getwch());
        }
        write_stdout("\r\n");
        return 0;
    });
}

const luaL_Reg functions[] = {
    {"version", version},
    {"debug", debug},
    {"run", run},
    {"copy", copy},
    {"move", move},
    {"env", env},
    {"mkdir", mkdir},
    {"remove", remove},
    {"exists", exists},
    {"pause", pause},
    {nullptr, nullptr},
};

const luaL_Reg path_functions[] = {
    {"join", join},
    {"dir", dir},
    {"name", name},
    {"ext", ext},
    {nullptr, nullptr},
};

const luaL_Reg fs_functions[] = {
    {"copy", copy},
    {"move", move},
    {"mkdir", mkdir},
    {"remove", remove},
    {"exists", exists},
    {nullptr, nullptr},
};

} // namespace

extern "C" int luaopen_star(lua_State* state) {
    luaL_checkversion(state);
    luaL_newlib(state, functions);

    luaL_newlib(state, path_functions);
    lua_createtable(state, 0, 1);
    lua_pushcfunction(state, path);
    lua_setfield(state, -2, "__call");
    lua_setmetatable(state, -2);
    lua_setfield(state, -2, "path");

    luaL_newlib(state, fs_functions);
    lua_setfield(state, -2, "fs");

    // Keep the short form `require "star"` useful in release scripts while
    // still returning the module for the conventional local assignment.
    lua_pushvalue(state, -1);
    lua_setglobal(state, "star");
    return 1;
}
