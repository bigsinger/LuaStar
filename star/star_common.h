#pragma once

#include <Windows.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace star {

std::wstring utf8_to_wide(std::string_view text);
std::string wide_to_utf8(std::wstring_view text);
std::wstring windows_error(DWORD code = GetLastError());

std::string check_string(lua_State* state, int index);
std::filesystem::path check_path(lua_State* state, int index);
void push_utf8(lua_State* state, std::string_view text);
void push_wide(lua_State* state, std::wstring_view text);
void push_path(lua_State* state, const std::filesystem::path& path);

bool option_bool(
    lua_State* state,
    int table_index,
    const char* name,
    bool fallback);
lua_Integer option_integer(
    lua_State* state,
    int table_index,
    const char* name,
    lua_Integer fallback);
std::optional<std::string> option_string(
    lua_State* state,
    int table_index,
    const char* name);
std::optional<std::filesystem::path> option_path(
    lua_State* state,
    int table_index,
    const char* name);
std::vector<std::string> table_strings(lua_State* state, int index);

std::filesystem::path executable_path();
std::filesystem::path script_path(lua_State* state);
std::filesystem::path safe_absolute(const std::filesystem::path& path);
void ensure_safe_destructive_target(const std::filesystem::path& path);

template <typename Function>
int guarded(lua_State* state, Function&& function) {
    try {
        return std::invoke(std::forward<Function>(function));
    } catch (const std::exception& exception) {
        return luaL_error(state, "%s", exception.what());
    } catch (...) {
        return luaL_error(state, "Unknown native error.");
    }
}

struct ProcessOptions final {
    std::optional<std::filesystem::path> cwd;
    DWORD timeout_ms = INFINITE;
    bool capture = false;
    bool check = true;
    bool hide = false;
};

struct ProcessResult final {
    bool ok = false;
    DWORD exit_code = 0;
    bool timed_out = false;
    std::string output;
    std::wstring command_line;
};

ProcessOptions read_process_options(lua_State* state, int table_index);
ProcessResult run_process(
    const std::filesystem::path& program,
    const std::vector<std::wstring>& arguments,
    const ProcessOptions& options);
ProcessResult run_shell(
    std::wstring_view command,
    const ProcessOptions& options);
std::optional<std::filesystem::path> find_program(
    std::wstring_view name,
    const std::vector<std::filesystem::path>& candidates = {});
std::wstring quote_argument(std::wstring_view argument);
void push_process_result(lua_State* state, const ProcessResult& result);
void enforce_process_success(const ProcessResult& result);

void register_path_module(lua_State* state);
void register_fs_module(lua_State* state);
void register_process_module(lua_State* state);
void register_release_module(lua_State* state);

int lua_process_batch(lua_State* state);

} // namespace star
