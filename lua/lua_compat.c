/*
** Compatibility export for embedders that dynamically bind luaL_openlibs.
** Lua 5.5 changed luaL_openlibs into a macro over luaL_openselectedlibs,
** so no luaL_openlibs symbol is emitted by the unmodified upstream sources.
*/

#define LUA_LIB

#include "lua.h"
#include "lualib.h"

#undef luaL_openlibs

LUALIB_API void luaL_openlibs(lua_State *L) {
  luaL_openselectedlibs(L, ~0, 0);
}
