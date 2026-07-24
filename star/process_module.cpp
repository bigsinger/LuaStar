#include "star_common.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {

class Handle final {
public:
    Handle() = default;
    explicit Handle(const HANDLE value) : value_(value) {}
    ~Handle() {
        reset();
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(other.release()) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept {
        return value_;
    }
    [[nodiscard]] HANDLE release() noexcept {
        const auto result = value_;
        value_ = nullptr;
        return result;
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
    explicit TemporaryFile(const wchar_t* extension = nullptr) {
        std::wstring directory(MAX_PATH + 1, L'\0');
        const auto directory_length =
            GetTempPathW(static_cast<DWORD>(directory.size()), directory.data());
        if (directory_length == 0 || directory_length >= directory.size()) {
            throw std::runtime_error("读取临时目录失败。");
        }
        directory.resize(directory_length);

        std::wstring buffer(MAX_PATH + 1, L'\0');
        if (GetTempFileNameW(
                directory.c_str(), L"LST", 0, buffer.data()) == 0) {
            throw std::runtime_error("创建临时文件名失败。");
        }
        buffer.resize(std::wcslen(buffer.c_str()));
        path_ = buffer;
        if (extension != nullptr) {
            const auto renamed = path_.replace_extension(extension);
            DeleteFileW(renamed.c_str());
            if (MoveFileExW(
                    path_.c_str(), renamed.c_str(),
                    MOVEFILE_REPLACE_EXISTING) == FALSE) {
                DeleteFileW(path_.c_str());
                throw std::runtime_error("无法准备临时文件。");
            }
            path_ = renamed;
        }
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

std::string read_output(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

star::ProcessResult run_command_line(
    const std::filesystem::path& application,
    std::wstring command_line,
    const star::ProcessOptions& options) {
    star::ProcessResult result;
    result.command_line = command_line;

    std::optional<TemporaryFile> capture_file;
    Handle capture_handle;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (options.capture) {
        capture_file.emplace();
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        capture_handle.reset(CreateFileW(
            capture_file->path().c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY,
            nullptr));
        if (capture_handle.get() == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("无法创建输出捕获文件。");
        }
        startup.dwFlags |= STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = capture_handle.get();
        startup.hStdError = capture_handle.get();
    }
    if (options.hide) {
        startup.dwFlags |= STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
    }

    std::vector<wchar_t> mutable_command(
        command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');
    PROCESS_INFORMATION process{};
    const auto cwd = options.cwd.has_value()
        ? options.cwd->wstring()
        : std::wstring();
    DWORD creation_flags = options.hide ? CREATE_NO_WINDOW : 0;
    const auto created = CreateProcessW(
        application.empty() ? nullptr : application.c_str(),
        mutable_command.data(),
        nullptr,
        nullptr,
        options.capture ? TRUE : FALSE,
        creation_flags,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &startup,
        &process);
    const auto create_error = GetLastError();
    capture_handle.reset();
    if (created == FALSE) {
        throw std::runtime_error(
            "创建进程失败：" +
            star::wide_to_utf8(star::windows_error(create_error)));
    }

    Handle thread(process.hThread);
    Handle process_handle(process.hProcess);
    const auto wait = WaitForSingleObject(
        process_handle.get(), options.timeout_ms);
    if (wait == WAIT_TIMEOUT) {
        result.timed_out = true;
        TerminateProcess(process_handle.get(), 124);
        WaitForSingleObject(process_handle.get(), INFINITE);
    } else if (wait != WAIT_OBJECT_0) {
        throw std::runtime_error(
            "等待进程失败：" +
            star::wide_to_utf8(star::windows_error()));
    }
    if (GetExitCodeProcess(process_handle.get(), &result.exit_code) == FALSE) {
        throw std::runtime_error("读取进程退出码失败。");
    }
    process_handle.reset();
    thread.reset();

    if (capture_file.has_value()) {
        result.output = read_output(capture_file->path());
    }
    result.ok = !result.timed_out && result.exit_code == 0;
    return result;
}

std::vector<std::wstring> lua_arguments(lua_State* state, const int index) {
    if (lua_gettop(state) < index || lua_isnil(state, index)) {
        return {};
    }
    const auto values = star::table_strings(state, index);
    std::vector<std::wstring> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(star::utf8_to_wide(value));
    }
    return result;
}

int run(lua_State* state) {
    return star::guarded(state, [&] {
        const auto program = star::check_path(state, 1);
        const auto arguments = lua_arguments(state, 2);
        const auto options = star::read_process_options(state, 3);
        const auto result = star::run_process(program, arguments, options);
        if (options.check) {
            star::enforce_process_success(result);
        }
        star::push_process_result(state, result);
        return 1;
    });
}

int shell(lua_State* state) {
    return star::guarded(state, [&] {
        const auto command = star::utf8_to_wide(star::check_string(state, 1));
        const auto options = star::read_process_options(state, 2);
        const auto result = star::run_shell(command, options);
        if (options.check) {
            star::enforce_process_success(result);
        }
        star::push_process_result(state, result);
        return 1;
    });
}

int which(lua_State* state) {
    return star::guarded(state, [&] {
        const auto name = star::utf8_to_wide(star::check_string(state, 1));
        std::vector<std::filesystem::path> candidates;
        if (lua_gettop(state) >= 2 && lua_istable(state, 2)) {
            for (const auto& candidate : star::table_strings(state, 2)) {
                candidates.emplace_back(star::utf8_to_wide(candidate));
            }
        }
        const auto result = star::find_program(name, candidates);
        if (!result.has_value()) {
            lua_pushnil(state);
        } else {
            star::push_path(state, *result);
        }
        return 1;
    });
}

int quote(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_wide(
            state,
            star::quote_argument(
                star::utf8_to_wide(star::check_string(state, 1))));
        return 1;
    });
}

const luaL_Reg functions[] = {
    {"run", run},
    {"shell", shell},
    {"batch", star::lua_process_batch},
    {"which", which},
    {"quote", quote},
    {nullptr, nullptr},
};

} // namespace

namespace star {

std::wstring quote_argument(const std::wstring_view argument) {
    if (!argument.empty() &&
        argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }

    std::wstring result;
    result.push_back(L'"');
    std::size_t backslashes = 0;
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(character);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

ProcessOptions read_process_options(
    lua_State* state,
    const int table_index) {
    ProcessOptions result;
    if (!lua_istable(state, table_index)) {
        return result;
    }
    result.cwd = option_path(state, table_index, "cwd");
    const auto timeout =
        option_integer(state, table_index, "timeout_ms", -1);
    if (timeout < -1 || timeout > MAXDWORD) {
        throw std::runtime_error("timeout_ms 超出有效范围。");
    }
    result.timeout_ms =
        timeout < 0 ? INFINITE : static_cast<DWORD>(timeout);
    result.capture = option_bool(state, table_index, "capture", false);
    result.check = option_bool(state, table_index, "check", true);
    result.hide = option_bool(state, table_index, "hide", false);
    return result;
}

ProcessResult run_process(
    const std::filesystem::path& program,
    const std::vector<std::wstring>& arguments,
    const ProcessOptions& options) {
    if (program.empty()) {
        throw std::runtime_error("程序路径不能为空。");
    }
    auto application = program;
    if (!program.has_parent_path()) {
        if (const auto resolved = find_program(program.wstring());
            resolved.has_value()) {
            application = *resolved;
        }
    }
    auto command_line = quote_argument(application.wstring());
    for (const auto& argument : arguments) {
        command_line.push_back(L' ');
        command_line.append(quote_argument(argument));
    }
    return run_command_line(application, std::move(command_line), options);
}

ProcessResult run_shell(
    const std::wstring_view command,
    const ProcessOptions& options) {
    if (command.empty()) {
        throw std::runtime_error("Shell 命令不能为空。");
    }
    std::wstring comspec(MAX_PATH + 1, L'\0');
    const auto count =
        GetEnvironmentVariableW(
            L"COMSPEC",
            comspec.data(),
            static_cast<DWORD>(comspec.size()));
    std::filesystem::path shell_path;
    if (count != 0 && count < comspec.size()) {
        comspec.resize(count);
        shell_path = comspec;
    } else {
        shell_path = L"cmd.exe";
    }
    auto command_line = quote_argument(shell_path.wstring());
    command_line.append(L" /D /S /C \"");
    command_line.append(command);
    command_line.push_back(L'"');
    return run_command_line(shell_path, std::move(command_line), options);
}

std::optional<std::filesystem::path> find_program(
    const std::wstring_view name,
    const std::vector<std::filesystem::path>& candidates) {
    const auto name_path = std::filesystem::path(name);
    if ((name_path.is_absolute() || name_path.has_parent_path()) &&
        std::filesystem::is_regular_file(name_path)) {
        return std::filesystem::absolute(name_path).lexically_normal();
    }
    for (const auto& candidate : candidates) {
        const auto value = std::filesystem::is_directory(candidate)
            ? candidate / name_path
            : candidate;
        if (std::filesystem::is_regular_file(value)) {
            return std::filesystem::absolute(value).lexically_normal();
        }
    }

    std::vector<std::wstring> names;
    if (name_path.has_extension()) {
        names.emplace_back(name);
    } else {
        names = {
            std::wstring(name) + L".exe",
            std::wstring(name) + L".com",
            std::wstring(name) + L".cmd",
            std::wstring(name) + L".bat",
            std::wstring(name),
        };
    }
    for (const auto& current : names) {
        const auto required =
            SearchPathW(nullptr, current.c_str(), nullptr, 0, nullptr, nullptr);
        if (required == 0) {
            continue;
        }
        std::wstring buffer(required + 1, L'\0');
        const auto written = SearchPathW(
            nullptr,
            current.c_str(),
            nullptr,
            static_cast<DWORD>(buffer.size()),
            buffer.data(),
            nullptr);
        if (written != 0 && written < buffer.size()) {
            buffer.resize(written);
            return std::filesystem::path(buffer);
        }
    }
    return std::nullopt;
}

void push_process_result(lua_State* state, const ProcessResult& result) {
    lua_createtable(state, 0, 5);
    lua_pushboolean(state, result.ok);
    lua_setfield(state, -2, "ok");
    lua_pushinteger(state, static_cast<lua_Integer>(result.exit_code));
    lua_setfield(state, -2, "exit_code");
    lua_pushboolean(state, result.timed_out);
    lua_setfield(state, -2, "timed_out");
    push_utf8(state, result.output);
    lua_setfield(state, -2, "output");
    push_wide(state, result.command_line);
    lua_setfield(state, -2, "command");
}

void enforce_process_success(const ProcessResult& result) {
    if (result.ok) {
        return;
    }
    std::string message = result.timed_out
        ? "进程执行超时。"
        : "进程执行失败，退出码为 " +
            std::to_string(result.exit_code) + "。";
    if (!result.output.empty()) {
        message.append("\n");
        message.append(result.output);
    }
    throw std::runtime_error(message);
}

int lua_process_batch(lua_State* state) {
    return guarded(state, [&] {
        auto content = check_string(state, 1);
        const auto options = read_process_options(state, 2);

        TemporaryFile file(L".cmd");
        std::ofstream stream(file.path(), std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("无法创建临时批处理文件。");
        }
        if (content.find("chcp") == std::string::npos &&
            content.find("CHCP") == std::string::npos) {
            stream << "@chcp 65001 >nul\r\n";
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        stream.close();
        const auto result = run_shell(
            L"call " + quote_argument(file.path().wstring()), options);
        if (options.check) {
            enforce_process_success(result);
        }
        lua_pushboolean(state, result.ok);
        lua_pushinteger(state, static_cast<lua_Integer>(result.exit_code));
        push_utf8(state, result.output);
        return 3;
    });
}

void register_process_module(lua_State* state) {
    luaL_newlib(state, functions);
}

} // namespace star
