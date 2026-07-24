#include "map_config.h"

#include <Windows.h>
#include <conio.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Deliberately do not include Lua headers here. runLua binds the small,
// version-stable Lua C API surface at runtime.
struct lua_State;
using lua_KContext = std::intptr_t;
using lua_KFunction = int(__cdecl*)(lua_State*, int, lua_KContext);

namespace {

constexpr int lua_multret = -1;

class LuaApiError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::string to_utf8(std::wstring_view text);

std::wstring format_windows_error(const DWORD code) {
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
    return message;
}

void write_standard_handle(
    const DWORD handle_id,
    const std::wstring_view text) {
    const auto handle = GetStdHandle(handle_id);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || text.empty()) {
        return;
    }
    DWORD console_mode = 0;
    if (GetConsoleMode(handle, &console_mode) != FALSE) {
        DWORD written = 0;
        WriteConsoleW(
            handle,
            text.data(),
            static_cast<DWORD>(text.size()),
            &written,
            nullptr);
        return;
    }

    const auto utf8 = to_utf8(text);
    DWORD written = 0;
    WriteFile(
        handle,
        utf8.data(),
        static_cast<DWORD>(utf8.size()),
        &written,
        nullptr);
}

void write_stderr(const std::wstring_view text) {
    write_standard_handle(STD_ERROR_HANDLE, text);
}

void write_error(const std::wstring_view text) {
    const auto handle = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE &&
        GetConsoleMode(handle, &mode) != FALSE) {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(handle, &info) != FALSE) {
            SetConsoleTextAttribute(
                handle,
                FOREGROUND_RED | FOREGROUND_INTENSITY);
            write_stderr(text);
            SetConsoleTextAttribute(handle, info.wAttributes);
            return;
        }
    }
    write_stderr(text);
}

void write_stdout(const std::wstring_view text) {
    write_standard_handle(STD_OUTPUT_HANDLE, text);
}

void pause_after_script_error() {
    const auto input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (input == nullptr || input == INVALID_HANDLE_VALUE ||
        GetConsoleMode(input, &mode) == FALSE) {
        return;
    }
    write_stderr(L"脚本执行失败，按任意键继续……\n");
    _getch();
}

std::string to_utf8(const std::wstring_view text) {
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

std::wstring to_wide(const std::string_view text) {
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
        return L"<错误文本不是有效的 UTF-8>";
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

std::filesystem::path absolute_from(
    const std::filesystem::path& value,
    const std::filesystem::path& base) {
    if (value.is_absolute()) {
        return value.lexically_normal();
    }
    return std::filesystem::absolute(base / value).lexically_normal();
}

std::vector<char> read_script(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("无法打开 Lua 脚本。");
    }
    std::vector<char> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(bytes.begin(), bytes.begin() + 3);
    }
    if (!bytes.empty() && bytes.front() == '#') {
        const auto line_end = std::find(bytes.begin(), bytes.end(), '\n');
        if (line_end == bytes.end()) {
            bytes.assign(1, '\n');
        } else {
            bytes.erase(bytes.begin(), line_end);
        }
    }
    return bytes;
}

template <typename Function>
Function bind(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

class LuaApi final {
public:
    using NewState = lua_State*(__cdecl*)();
    using OpenLibs = void(__cdecl*)(lua_State*);
    using LoadBuffer = int(__cdecl*)(
        lua_State*, const char*, std::size_t, const char*, const char*);
    using PCall = int(__cdecl*)(
        lua_State*, int, int, int, lua_KContext, lua_KFunction);
    using Close = void(__cdecl*)(lua_State*);
    using ToString = const char*(__cdecl*)(
        lua_State*, int, std::size_t*);
    using SetTop = void(__cdecl*)(lua_State*, int);
    using PushString = const char*(__cdecl*)(
        lua_State*, const char*, std::size_t);
    using GetGlobal = int(__cdecl*)(lua_State*, const char*);
    using GetField = int(__cdecl*)(lua_State*, int, const char*);
    using SetField = void(__cdecl*)(lua_State*, int, const char*);

    explicit LuaApi(const HMODULE module)
        : new_state(bind<NewState>(module, "luaL_newstate")),
          open_libs(bind<OpenLibs>(module, "luaL_openlibs")),
          load_buffer(bind<LoadBuffer>(module, "luaL_loadbufferx")),
          pcall(bind<PCall>(module, "lua_pcallk")),
          close(bind<Close>(module, "lua_close")),
          to_string(bind<ToString>(module, "lua_tolstring")),
          set_top(bind<SetTop>(module, "lua_settop")),
          push_string(bind<PushString>(module, "lua_pushlstring")),
          get_global(bind<GetGlobal>(module, "lua_getglobal")),
          get_field(bind<GetField>(module, "lua_getfield")),
          set_field(bind<SetField>(module, "lua_setfield")) {
        if (new_state == nullptr || open_libs == nullptr ||
            load_buffer == nullptr || pcall == nullptr || close == nullptr ||
            to_string == nullptr || set_top == nullptr ||
            push_string == nullptr) {
            throw LuaApiError("配置的 DLL 未导出必需的 Lua C API。");
        }
    }

    NewState new_state;
    OpenLibs open_libs;
    LoadBuffer load_buffer;
    PCall pcall;
    Close close;
    ToString to_string;
    SetTop set_top;
    PushString push_string;
    GetGlobal get_global;
    GetField get_field;
    SetField set_field;
};

class LuaState final {
public:
    explicit LuaState(const LuaApi& api) : api_(api), state_(api.new_state()) {
        if (state_ == nullptr) {
            throw std::runtime_error("Lua 状态创建失败。");
        }
    }
    ~LuaState() {
        api_.close(state_);
    }
    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;

    [[nodiscard]] lua_State* get() const noexcept {
        return state_;
    }

private:
    const LuaApi& api_;
    lua_State* state_;
};

std::wstring lua_error(const LuaApi& api, lua_State* state) {
    std::size_t length = 0;
    const auto* text = api.to_string(state, -1, &length);
    if (text == nullptr) {
        return L"未知 Lua 错误。";
    }
    return to_wide(std::string_view(text, length));
}

void add_executable_cpath(
    const LuaApi& api,
    lua_State* state,
    const std::filesystem::path& directory) {
    if (api.get_global == nullptr || api.get_field == nullptr ||
        api.set_field == nullptr) {
        return;
    }
    api.get_global(state, "package");
    api.get_field(state, -1, "cpath");
    std::size_t old_length = 0;
    const auto* old_value = api.to_string(state, -1, &old_length);
    const std::string old_path =
        old_value == nullptr ? std::string() : std::string(old_value, old_length);
    api.set_top(state, -2);

    auto prefix = to_utf8((directory / L"?.dll").wstring());
    prefix.push_back(';');
    prefix.append(old_path);
    api.push_string(state, prefix.data(), prefix.size());
    api.set_field(state, -2, "cpath");
    api.set_top(state, 0);
}

struct Options final {
    std::optional<std::filesystem::path> config;
    std::optional<std::filesystem::path> dll;
    std::optional<std::filesystem::path> script;
    std::vector<std::wstring> lua_arguments;
    bool help = false;
};

Options parse_options(const int argc, wchar_t* argv[]) {
    Options result;
    bool arguments_only = false;
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view current(argv[index]);
        if (arguments_only) {
            result.lua_arguments.emplace_back(argv[index]);
        } else if (current == L"--") {
            arguments_only = true;
        } else if (current == L"--help" || current == L"-h" ||
                   current == L"/?") {
            result.help = true;
        } else if (current == L"--config") {
            if (++index >= argc) {
                throw std::runtime_error("--config 后必须指定文件路径。");
            }
            result.config = argv[index];
        } else if (current == L"--dll") {
            if (++index >= argc) {
                throw std::runtime_error("--dll 后必须指定文件路径。");
            }
            result.dll = argv[index];
        } else if (current == L"--script") {
            if (++index >= argc) {
                throw std::runtime_error("--script 后必须指定文件路径。");
            }
            result.script = argv[index];
        } else if (!result.script.has_value() && !result.config.has_value()) {
            result.script = argv[index];
        } else {
            result.lua_arguments.emplace_back(argv[index]);
        }
    }
    return result;
}

std::optional<std::filesystem::path> find_automatic_config(
    const std::filesystem::path& exe_directory) {
    const auto current = std::filesystem::current_path() / L"runLua.ini";
    if (std::filesystem::is_regular_file(current)) {
        return current;
    }
    const auto beside_exe = exe_directory / L"runLua.ini";
    if (std::filesystem::is_regular_file(beside_exe)) {
        return beside_exe;
    }
    return std::nullopt;
}

void print_usage() {
    write_stdout(
        L"runLua - 与 Lua 版本解耦的脚本启动器\n\n"
        L"用法：\n"
        L"  runLua.exe [script.lua] [args...]\n"
        L"  runLua.exe --config file.ini [--script file.lua] [args...]\n"
        L"  runLua.exe --dll lua-runtime.dll --script file.lua [args...]\n\n"
        L"扁平配置项：\n"
        L"  LuaDll=star.dll\n"
        L"  LuaFile=main.lua\n");
}

int run(const int argc, wchar_t* argv[]) {
    const auto options = parse_options(argc, argv);
    if (options.help) {
        print_usage();
        return 0;
    }

    const auto exe = executable_path();
    const auto exe_directory = exe.parent_path();
    auto config_path = options.config;
    if (!config_path.has_value()) {
        config_path = find_automatic_config(exe_directory);
    } else {
        config_path = absolute_from(*config_path, std::filesystem::current_path());
        if (!std::filesystem::is_regular_file(*config_path)) {
            throw std::runtime_error("指定的配置文件不存在。");
        }
    }

    std::optional<MapConfig> config;
    if (config_path.has_value()) {
        config_path = std::filesystem::absolute(*config_path).lexically_normal();
        config = MapConfig::load(*config_path);
    }

    const auto config_base =
        config_path.has_value() ? config_path->parent_path() : exe_directory;
    std::filesystem::path dll_value =
        options.dll.value_or(config.has_value()
            ? std::filesystem::path(config->get_or(L"LuaDll", L"star.dll"))
            : std::filesystem::path(L"star.dll"));
    const auto dll_path = absolute_from(dll_value, config_base);

    const bool command_line_script = options.script.has_value();
    std::filesystem::path script_value =
        options.script.value_or(config.has_value()
            ? std::filesystem::path(config->get_or(L"LuaFile", L"main.lua"))
            : std::filesystem::path(L"main.lua"));
    const auto script_base = command_line_script
        ? std::filesystem::current_path()
        : (config_path.has_value()
            ? config_path->parent_path()
            : std::filesystem::current_path());
    const auto script_path = absolute_from(script_value, script_base);

    const auto module = LoadLibraryExW(
        dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr) {
        const auto code = GetLastError();
        std::wostringstream message;
        message << L"无法加载 Lua 运行库：\n  " << dll_path
                << L"\n  " << format_windows_error(code) << L" (" << code
                << L")\n";
        write_stderr(message.str());
        return 3;
    }

    try {
        const LuaApi api(module);
        LuaState state(api);
        api.open_libs(state.get());
        add_executable_cpath(api, state.get(), exe_directory);

        const auto script = read_script(script_path);
        const auto chunk_name = "@" + to_utf8(script_path.wstring());
        if (api.load_buffer(
                state.get(),
                script.data(),
                script.size(),
                chunk_name.c_str(),
                nullptr) != 0) {
            std::wostringstream message;
            message << L"Lua 脚本编译失败：" << script_path << L"\n"
                    << lua_error(api, state.get()) << L"\n";
            write_error(message.str());
            pause_after_script_error();
            return 5;
        }

        for (const auto& argument : options.lua_arguments) {
            const auto utf8 = to_utf8(argument);
            api.push_string(state.get(), utf8.data(), utf8.size());
        }
        if (api.pcall(
                state.get(),
                static_cast<int>(options.lua_arguments.size()),
                lua_multret,
                0,
                0,
                nullptr) != 0) {
            std::wostringstream message;
            message << L"Lua 脚本运行失败：" << script_path << L"\n"
                    << lua_error(api, state.get()) << L"\n";
            write_error(message.str());
            pause_after_script_error();
            return 5;
        }
    } catch (...) {
        FreeLibrary(module);
        throw;
    }
    FreeLibrary(module);
    return 0;
}

} // namespace

int wmain(const int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    try {
        return run(argc, argv);
    } catch (const LuaApiError& exception) {
        write_stderr(
            L"Lua API 绑定失败：" + to_wide(exception.what()) + L"\n");
        return 4;
    } catch (const std::exception& exception) {
        write_stderr(L"runLua 启动失败：" + to_wide(exception.what()) + L"\n");
        return 2;
    }
}
