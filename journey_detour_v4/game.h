#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "sigscan.h"

#define LUA_OK 0
#define LUA_YIELD 1
#define LUA_ERRRUN 2
#define LUA_ERRSYNTAX 3
#define LUA_ERRMEM 4
#define LUA_ERRGCMM 5
#define LUA_ERRERR 6

#define LUA_KCONTEXT ptrdiff_t

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef LUA_KCONTEXT lua_KContext;
typedef int (*lua_KFunction)(lua_State *L, int status, lua_KContext ctx);
typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

SIGSCAN_FUNC(lua_settop, "85 D2 78 34 48", __fastcall, void, lua_State *L,
             int idx);

#define lua_pop(L, n) lua_settop(L, -(n)-1)

SIGSCAN_FUNC(luaL_loadstring, "48 83 EC 48 48 89 54 24 30 48", __fastcall, int,
             lua_State *L, const char *s);

SIGSCAN_FUNC(luaL_loadbufferx, "48 83 EC ?? 48 8B 44 24 ?? 48 89 54 24",
             __fastcall, int, lua_State *L, const char *buff, size_t size,
             const char *name, const char *mode);

SIGSCAN_FUNC(lua_pushcclosure,
             "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 49 63 F8",
             __fastcall, void, lua_State *L, lua_CFunction fn, int n);

#define lua_pushcfunction(L, f) lua_pushcclosure(L, (f), 0)

SIGSCAN_FUNC(lua_tolstring,
             "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 49 8B D8 8B F2",
             __fastcall, const char *, lua_State *L, int idx, size_t *len);

#define lua_tostring(L, i) lua_tolstring(L, (i), NULL)

SIGSCAN_FUNC(lua_callk, "48 89 5C 24 ?? 57 48 83 EC ?? 8D 42", __fastcall, void,
             lua_State *L, int nargs, int nresults, lua_KContext ctx,
             lua_KFunction k);

SIGSCAN_FUNC(lua_pcallk,
             "48 89 74 24 ?? 57 48 83 EC ?? 33 F6 48 89 6C 24 ?? 41 8B E8",
             __fastcall, int, lua_State *L, int nargs, int nresults,
             int errfunc, lua_KContext ctx, lua_KFunction k);

SIGSCAN_FUNC(lua_gettop, "48 8B 41 ?? 48 8B 51 ?? 48 2B 10", __fastcall, int,
             lua_State *L);

SIGSCAN_FUNC(lua_setglobal,
             "48 89 5C 24 ?? 57 48 83 EC ?? 48 8B 41 ?? 48 8B DA 48 8B F9 BA "
             "?? ?? ?? ?? 48 8B 48 ?? E8 ?? ?? ?? ?? 48 8B D0 4C 8B C3 48 8B "
             "CF 48 8B 5C 24 ?? 48 83 C4 ?? 5F E9 ?? ?? ?? ?? CC CC CC CC CC "
             "CC 48 89 5C 24 ?? 48 89 74 24",
             __fastcall, void, lua_State *L, const char *name);

class LuaManager {
public:
  static LuaManager &instance();

  void doNextFrame(std::function<void(lua_State *L)> &&operation);
  void doNextFrame(const std::string &str);
  template <typename... T>
  inline void doNextFrame(fmt::format_string<T...> fmt, T &&...args) {
    return doNextFrame(fmt::vformat(fmt, fmt::make_format_args(args...)));
  }

  void registerGlobal(const std::string &name, const std::string &code);
  void registerGlobal(const std::string &name, lua_CFunction func);

  void update(lua_State *L);

private:
  std::mutex pendingOperationsMutex;
  std::vector<std::function<void(lua_State *L)>> pendingOperations;

  LuaManager();
};
