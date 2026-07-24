#include "star_common.h"

#include <conio.h>

#include <stdexcept>

namespace {

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
        const auto wide = star::utf8_to_wide(text);
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

int version(lua_State* state) {
    lua_pushliteral(state, "1.0.0");
    return 1;
}

int lua_version(lua_State* state) {
    lua_pushliteral(state, LUA_RELEASE);
    return 1;
}

int exe_path(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::executable_path());
        return 1;
    });
}

int exe_dir(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::executable_path().parent_path());
        return 1;
    });
}

int script_path(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::script_path(state);
        if (path.empty()) {
            lua_pushnil(state);
        } else {
            star::push_path(state, path);
        }
        return 1;
    });
}

int script_dir(lua_State* state) {
    return star::guarded(state, [&] {
        auto path = star::script_path(state);
        star::push_path(
            state,
            path.empty() ? std::filesystem::current_path() : path.parent_path());
        return 1;
    });
}

int cwd(lua_State* state) {
    return star::guarded(state, [&] {
        if (lua_gettop(state) >= 1 && !lua_isnil(state, 1)) {
            std::filesystem::current_path(star::check_path(state, 1));
        }
        star::push_path(state, std::filesystem::current_path());
        return 1;
    });
}

int pause(lua_State* state) {
    return star::guarded(state, [&] {
        const auto message = lua_gettop(state) >= 1 && !lua_isnil(state, 1)
            ? star::check_string(state, 1)
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

int require_file(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::check_path(state, 1);
        if (!std::filesystem::is_regular_file(path)) {
            const auto label = lua_gettop(state) >= 2 && !lua_isnil(state, 2)
                ? star::check_string(state, 2)
                : std::string("必需文件");
            throw std::runtime_error(
                "缺少" + label + "：" +
                star::wide_to_utf8(path.wstring()));
        }
        star::push_path(state, std::filesystem::absolute(path).lexically_normal());
        return 1;
    });
}

int require_dir(lua_State* state) {
    return star::guarded(state, [&] {
        const auto path = star::check_path(state, 1);
        if (!std::filesystem::is_directory(path)) {
            const auto label = lua_gettop(state) >= 2 && !lua_isnil(state, 2)
                ? star::check_string(state, 2)
                : std::string("必需目录");
            throw std::runtime_error(
                "缺少" + label + "：" +
                star::wide_to_utf8(path.wstring()));
        }
        star::push_path(state, std::filesystem::absolute(path).lexically_normal());
        return 1;
    });
}

int help(lua_State* state) {
    lua_pushliteral(
        state,
        "star 1.0.0\n"
        "模块：star.path、star.fs、star.process、star.release\n"
        "完整接口见 docs/api.md");
    return 1;
}

const luaL_Reg functions[] = {
    {"version", version},
    {"lua_version", lua_version},
    {"exe_path", exe_path},
    {"exe_dir", exe_dir},
    {"script_path", script_path},
    {"script_dir", script_dir},
    {"cwd", cwd},
    {"pause", pause},
    {"require_file", require_file},
    {"require_dir", require_dir},
    {"help", help},
    {nullptr, nullptr},
};

} // namespace

extern "C" int luaopen_star(lua_State* state) {
    luaL_checkversion(state);
    luaL_newlib(state, functions);

    star::register_path_module(state);
    lua_setfield(state, -2, "path");
    star::register_fs_module(state);
    lua_setfield(state, -2, "fs");
    star::register_process_module(state);
    lua_setfield(state, -2, "process");
    star::register_release_module(state);
    lua_setfield(state, -2, "release");

    return 1;
}
