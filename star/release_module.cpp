#include "star_common.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace {

std::vector<std::string> string_or_table_field(
    lua_State* state,
    const int table_index,
    const char* name) {
    if (!lua_istable(state, table_index)) {
        return {};
    }
    const auto absolute_index = lua_absindex(state, table_index);
    lua_getfield(state, absolute_index, name);
    std::vector<std::string> result;
    if (lua_istable(state, -1)) {
        result = star::table_strings(state, -1);
    } else if (lua_isstring(state, -1)) {
        result.push_back(star::check_string(state, -1));
    }
    lua_pop(state, 1);
    return result;
}

std::optional<std::filesystem::path> environment_path(
    const std::wstring& name,
    const std::wstring& executable_name) {
    const auto required =
        GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (required == 0) {
        return std::nullopt;
    }
    std::wstring value(required, L'\0');
    const auto written = GetEnvironmentVariableW(
        name.c_str(), value.data(), static_cast<DWORD>(value.size()));
    if (written == 0 || written >= value.size()) {
        return std::nullopt;
    }
    value.resize(written);
    auto path = std::filesystem::path(value);
    if (std::filesystem::is_directory(path)) {
        path /= executable_name;
    }
    if (std::filesystem::is_regular_file(path)) {
        return std::filesystem::absolute(path).lexically_normal();
    }
    return std::nullopt;
}

std::filesystem::path resolve_tool(
    lua_State* state,
    const int options_index,
    const std::wstring& executable_name,
    const std::vector<std::wstring>& default_environment_names,
    const std::vector<std::filesystem::path>& default_candidates = {}) {
    if (const auto configured =
            star::option_path(state, options_index, "exe");
        configured.has_value()) {
        if (!std::filesystem::is_regular_file(*configured)) {
            throw std::runtime_error(
                "配置的工具不存在：" +
                star::wide_to_utf8(configured->wstring()));
        }
        return std::filesystem::absolute(*configured).lexically_normal();
    }

    auto environment_names = default_environment_names;
    for (const auto& value :
         string_or_table_field(state, options_index, "env")) {
        environment_names.push_back(star::utf8_to_wide(value));
    }
    for (const auto& name : environment_names) {
        if (const auto value = environment_path(name, executable_name);
            value.has_value()) {
            return *value;
        }
    }

    auto candidates = default_candidates;
    for (const auto& value :
         string_or_table_field(state, options_index, "paths")) {
        candidates.emplace_back(star::utf8_to_wide(value));
    }
    if (const auto found =
            star::find_program(executable_name, candidates);
        found.has_value()) {
        return *found;
    }
    throw std::runtime_error(
        "没有找到必需工具：" +
        star::wide_to_utf8(executable_name));
}

std::vector<std::wstring> option_arguments(
    lua_State* state,
    const int options_index,
    const char* field) {
    std::vector<std::wstring> result;
    for (const auto& value :
         string_or_table_field(state, options_index, field)) {
        result.push_back(star::utf8_to_wide(value));
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> table_pairs(
    lua_State* state,
    const int table_index,
    const char* field) {
    std::vector<std::pair<std::string, std::string>> result;
    if (!lua_istable(state, table_index)) {
        return result;
    }
    const auto absolute_index = lua_absindex(state, table_index);
    lua_getfield(state, absolute_index, field);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return result;
    }

    const auto values_index = lua_absindex(state, -1);
    lua_pushnil(state);
    while (lua_next(state, values_index) != 0) {
        const auto key = star::check_string(state, -2);
        std::size_t length = 0;
        const auto* value = luaL_tolstring(state, -1, &length);
        result.emplace_back(key, std::string(value, length));
        lua_pop(state, 2);
    }
    lua_pop(state, 1);
    std::ranges::sort(result, {}, &std::pair<std::string, std::string>::first);
    return result;
}

int find_tool(lua_State* state) {
    return star::guarded(state, [&] {
        const auto name = star::utf8_to_wide(star::check_string(state, 1));
        const int options_index = 2;
        try {
            star::push_path(
                state,
                resolve_tool(state, options_index, name, {}));
        } catch (const std::exception&) {
            if (star::option_bool(state, options_index, "required", true)) {
                throw;
            }
            lua_pushnil(state);
        }
        return 1;
    });
}

int upx(lua_State* state) {
    return star::guarded(state, [&] {
        const auto source = star::check_path(state, 1);
        const auto destination = star::check_path(state, 2);
        const int options_index = 3;
        if (!std::filesystem::is_regular_file(source)) {
            throw std::runtime_error(
                "待压缩文件不存在：" +
                star::wide_to_utf8(source.wstring()));
        }
        const auto executable = resolve_tool(
            state,
            options_index,
            L"upx.exe",
            {L"UPX_EXE"});
        auto arguments = option_arguments(
            state, options_index, "arguments");
        if (arguments.empty()) {
            arguments.push_back(L"-9kf");
        }
        arguments.push_back(L"-o");
        arguments.push_back(destination.wstring());
        arguments.push_back(source.wstring());

        if (star::option_bool(state, options_index, "force", true) &&
            std::filesystem::exists(destination)) {
            std::filesystem::remove(destination);
        }
        if (!destination.parent_path().empty()) {
            std::filesystem::create_directories(destination.parent_path());
        }
        auto process_options =
            star::read_process_options(state, options_index);
        const auto result =
            star::run_process(executable, arguments, process_options);
        if (process_options.check) {
            star::enforce_process_success(result);
        }
        star::push_process_result(state, result);
        return 1;
    });
}

int nsis(lua_State* state) {
    return star::guarded(state, [&] {
        const auto script = star::check_path(state, 1);
        const int options_index = 2;
        if (!std::filesystem::is_regular_file(script)) {
            throw std::runtime_error(
                "安装脚本不存在：" +
                star::wide_to_utf8(script.wstring()));
        }
        const auto executable = resolve_tool(
            state,
            options_index,
            L"makensis.exe",
            {L"MAKENSIS_EXE", L"NSIS"});
        std::vector<std::wstring> arguments;
        if (star::option_bool(
                state, options_index, "warnings_as_errors", false)) {
            arguments.push_back(L"/WX");
        }
        if (const auto charset =
                star::option_string(state, options_index, "charset");
            charset.has_value()) {
            arguments.push_back(
                L"/INPUTCHARSET=" + star::utf8_to_wide(*charset));
        }
        for (const auto& [name, value] :
             table_pairs(state, options_index, "defines")) {
            arguments.push_back(
                L"/D" + star::utf8_to_wide(name) + L"=" +
                star::utf8_to_wide(value));
        }
        auto additional =
            option_arguments(state, options_index, "arguments");
        arguments.insert(
            arguments.end(), additional.begin(), additional.end());
        arguments.push_back(std::filesystem::absolute(script).wstring());

        auto process_options =
            star::read_process_options(state, options_index);
        if (!process_options.cwd.has_value()) {
            process_options.cwd =
                std::filesystem::absolute(script).parent_path();
        }
        const auto result =
            star::run_process(executable, arguments, process_options);
        if (process_options.check) {
            star::enforce_process_success(result);
        }
        star::push_process_result(state, result);
        return 1;
    });
}

int msbuild(lua_State* state) {
    return star::guarded(state, [&] {
        const auto project = star::check_path(state, 1);
        const int options_index = 2;
        if (!std::filesystem::is_regular_file(project)) {
            throw std::runtime_error(
                "构建输入不存在：" +
                star::wide_to_utf8(project.wstring()));
        }
        const auto executable = resolve_tool(
            state,
            options_index,
            L"MSBuild.exe",
            {L"MSBUILD_EXE"});
        std::vector<std::wstring> arguments{
            std::filesystem::absolute(project).wstring()};
        if (star::option_bool(state, options_index, "restore", false)) {
            arguments.push_back(L"/restore");
        }
        if (star::option_bool(state, options_index, "max_cpu", true)) {
            arguments.push_back(L"/m");
        }
        if (const auto target =
                star::option_string(state, options_index, "target");
            target.has_value()) {
            arguments.push_back(L"/t:" + star::utf8_to_wide(*target));
        }
        if (const auto configuration =
                star::option_string(state, options_index, "configuration");
            configuration.has_value()) {
            arguments.push_back(
                L"/p:Configuration=" + star::utf8_to_wide(*configuration));
        }
        if (const auto platform =
                star::option_string(state, options_index, "platform");
            platform.has_value()) {
            arguments.push_back(
                L"/p:Platform=" + star::utf8_to_wide(*platform));
        }
        for (const auto& [name, value] :
             table_pairs(state, options_index, "properties")) {
            arguments.push_back(
                L"/p:" + star::utf8_to_wide(name) + L"=" +
                star::utf8_to_wide(value));
        }
        auto additional =
            option_arguments(state, options_index, "arguments");
        arguments.insert(
            arguments.end(), additional.begin(), additional.end());

        auto process_options =
            star::read_process_options(state, options_index);
        if (!process_options.cwd.has_value()) {
            process_options.cwd =
                std::filesystem::absolute(project).parent_path();
        }
        const auto result =
            star::run_process(executable, arguments, process_options);
        if (process_options.check) {
            star::enforce_process_success(result);
        }
        star::push_process_result(state, result);
        return 1;
    });
}

const luaL_Reg functions[] = {
    {"find_tool", find_tool},
    {"upx", upx},
    {"nsis", nsis},
    {"msbuild", msbuild},
    {nullptr, nullptr},
};

} // namespace

namespace star {

void register_release_module(lua_State* state) {
    luaL_newlib(state, functions);
}

} // namespace star
