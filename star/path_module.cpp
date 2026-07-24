#include "star_common.h"

namespace {

int join(lua_State* state) {
    return star::guarded(state, [&] {
        const auto count = lua_gettop(state);
        if (count < 1) {
            return luaL_error(state, "path.join 至少需要一个路径参数。");
        }
        auto result = star::check_path(state, 1);
        for (int index = 2; index <= count; ++index) {
            result /= star::check_path(state, index);
        }
        star::push_path(state, result.lexically_normal());
        return 1;
    });
}

int normalize(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::check_path(state, 1).lexically_normal());
        return 1;
    });
}

int absolute(lua_State* state) {
    return star::guarded(state, [&] {
        auto value = star::check_path(state, 1);
        const auto base = lua_gettop(state) >= 2 && !lua_isnil(state, 2)
            ? star::check_path(state, 2)
            : std::filesystem::current_path();
        if (value.is_relative()) {
            value = base / value;
        }
        star::push_path(state, std::filesystem::absolute(value).lexically_normal());
        return 1;
    });
}

int relative(lua_State* state) {
    return star::guarded(state, [&] {
        const auto value = star::safe_absolute(star::check_path(state, 1));
        const auto base = lua_gettop(state) >= 2 && !lua_isnil(state, 2)
            ? star::safe_absolute(star::check_path(state, 2))
            : std::filesystem::current_path();
        star::push_path(state, value.lexically_relative(base));
        return 1;
    });
}

int parent(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::check_path(state, 1).parent_path());
        return 1;
    });
}

int filename(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::check_path(state, 1).filename());
        return 1;
    });
}

int stem(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::check_path(state, 1).stem());
        return 1;
    });
}

int extension(lua_State* state) {
    return star::guarded(state, [&] {
        star::push_path(state, star::check_path(state, 1).extension());
        return 1;
    });
}

int is_absolute(lua_State* state) {
    return star::guarded(state, [&] {
        lua_pushboolean(state, star::check_path(state, 1).is_absolute());
        return 1;
    });
}

const luaL_Reg functions[] = {
    {"join", join},
    {"normalize", normalize},
    {"absolute", absolute},
    {"relative", relative},
    {"parent", parent},
    {"filename", filename},
    {"stem", stem},
    {"extension", extension},
    {"is_absolute", is_absolute},
    {nullptr, nullptr},
};

} // namespace

namespace star {

void register_path_module(lua_State* state) {
    luaL_newlib(state, functions);
    lua_pushliteral(state, "\\");
    lua_setfield(state, -2, "separator");
}

} // namespace star
