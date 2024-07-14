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

#define LUA_NUMBER double
#define LUA_INTEGER long long

typedef LUA_NUMBER lua_Number;
typedef LUA_INTEGER lua_Integer;

#define LUA_TNONE (-1)

#define LUA_TNIL 0
#define LUA_TBOOLEAN 1
#define LUA_TLIGHTUSERDATA 2
#define LUA_TNUMBER 3
#define LUA_TSTRING 4
#define LUA_TTABLE 5
#define LUA_TFUNCTION 6
#define LUA_TUSERDATA 7
#define LUA_TTHREAD 8

#define LUA_NUMTAGS 9

#define lua_call(L, n, r) lua_callk(L, (n), (r), 0, NULL)

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef LUA_KCONTEXT lua_KContext;
typedef int (*lua_KFunction)(lua_State *L, int status, lua_KContext ctx);
typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

struct vec_mat {
  float m11, m12, m13, m14;
  float m21, m22, m23, m24;
  float m31, m32, m33, m34;
  float m41, m42, m43, m44;
};

struct LobbyChatMsg_t {
  uint64_t m_ulSteamIDLobby;
  uint64_t m_ulSteamIDUser;
  uint8_t m_eChatEntryType;
  uint32_t m_iChatID;
};

struct LobbyMember_t {
  uint64_t steamId;
  float posX;
  float posY;
  float posZ;
};

SIGSCAN_FUNC(
    SteamFriends,
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 48 8B 1D ?? "
    "?? ?? ?? 33 C0 89 44 24 ?? 48 85 DB 74 ?? F0 FF 43 ?? 48 8B 1D ?? ?? ?? "
    "?? 48 8B 35 ?? ?? ?? ?? 48 8B FB 48 85 F6 74 ?? 48 85 DB 74 ?? F0 FF 43 "
    "?? 48 8B 1D ?? ?? ?? ?? 48 8B 35 ?? ?? ?? ?? 48 8B 76 ?? B8 ?? ?? ?? ?? "
    "EB ?? 48 8B 5C 24 ?? 48 8B F0 BD ?? ?? ?? ?? A8 ?? 74 ?? 48 85 DB 74 ?? "
    "8B C5 F0 0F C1 43 ?? 83 F8 ?? 75 ?? 48 8B 03 48 8B CB FF 10 8B C5 F0 0F "
    "C1 43 ?? 83 F8 ?? 75 ?? 48 8B 03 48 8B CB FF 50 ?? 48 85 FF 74 ?? 8B C5 "
    "F0 0F C1 47 ?? 83 F8 ?? 75 ?? 48 8B 17 48 8B CF FF 12 F0 0F C1 6F ?? 83 "
    "FD ?? 75 ?? 48 8B 17 48 8B CF FF 52 ?? 48 8B 5C 24 ?? 48 8B C6 48 8B 74 "
    "24 ?? 48 8B 6C 24 ?? 48 83 C4 ?? 5F C3 CC CC CC CC CC CC CC CC CC CC CC "
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 33 C0",
    __fastcall, __int64)

SIGSCAN_FUNC(lua_settop, "85 D2 78 34 48", __fastcall, void, lua_State *L,
             int idx)

#define lua_pop(L, n) lua_settop(L, -(n)-1)

SIGSCAN_FUNC(luaL_loadstring, "48 83 EC 48 48 89 54 24 30 48", __fastcall, int,
             lua_State *L, const char *s)

SIGSCAN_FUNC(luaL_loadbufferx, "48 83 EC ?? 48 8B 44 24 ?? 48 89 54 24",
             __fastcall, int, lua_State *L, const char *buff, size_t size,
             const char *name, const char *mode)

SIGSCAN_FUNC(lua_pushcclosure,
             "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 49 63 F8",
             __fastcall, void, lua_State *L, lua_CFunction fn, int n)

#define lua_pushcfunction(L, f) lua_pushcclosure(L, (f), 0)

SIGSCAN_FUNC(lua_pushinteger,
             "48 8B 41 ?? 48 89 10 C7 40 ?? ?? ?? ?? ?? 48 83 41 ?? ?? C3 CC "
             "CC CC CC CC CC CC CC CC CC CC CC 48 89 5C 24",
             __fastcall, void, lua_State *L, lua_Integer n)

SIGSCAN_FUNC(luaL_checknumber, "48 89 5C 24 ?? 57 48 83 EC ?? 4C 8D 44 24",
             __fastcall, lua_Number, lua_State *L, int arg)

SIGSCAN_FUNC(lua_type,
             "48 83 EC ?? E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 48 3B C1",
             __fastcall, int, lua_State *L, int idx)
SIGSCAN_FUNC(
    lua_gettable,
    "48 89 5C 24 ?? 57 48 83 EC ?? 48 8B D9 E8 ?? ?? ?? ?? 48 8B F8 83 78",
    __fastcall, int, lua_State *L, int idx)

SIGSCAN_FUNC(lua_tolstring,
             "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 49 8B D8 8B F2",
             __fastcall, const char *, lua_State *L, int idx, size_t *len)

#define lua_tostring(L, i) lua_tolstring(L, (i), NULL)

SIGSCAN_FUNC(lua_rawlen, "48 83 EC ?? E8 ?? ?? ?? ?? 8B 50", __fastcall, size_t,
             lua_State *L, int idx)

SIGSCAN_FUNC(
    lua_tonumberx,
    "40 53 48 83 EC ?? 49 8B D8 E8 ?? ?? ?? ?? 83 78 ?? ?? 75 ?? F2 0F 10 00",
    __fastcall, lua_Number, lua_State *L, int idx, int *pisnum)

SIGSCAN_FUNC(lua_callk, "48 89 5C 24 ?? 57 48 83 EC ?? 8D 42", __fastcall, void,
             lua_State *L, int nargs, int nresults, lua_KContext ctx,
             lua_KFunction k)

SIGSCAN_FUNC(lua_pcallk,
             "48 89 74 24 ?? 57 48 83 EC ?? 33 F6 48 89 6C 24 ?? 41 8B E8",
             __fastcall, int, lua_State *L, int nargs, int nresults,
             int errfunc, lua_KContext ctx, lua_KFunction k)

SIGSCAN_FUNC(lua_gettop, "48 8B 41 ?? 48 8B 51 ?? 48 2B 10", __fastcall, int,
             lua_State *L)
SIGSCAN_FUNC(lua_pushvalue, "48 83 EC ?? 4C 8B D1 E8 ?? ?? ?? ?? 49 8B 52",
             __fastcall, void, lua_State *L, int idx)

SIGSCAN_FUNC(
    lua_getglobal,
    "48 89 5C 24 ?? 57 48 83 EC ?? 48 8B 41 ?? 48 8B DA 48 8B F9 BA ?? ?? ?? "
    "?? 48 8B 48 ?? E8 ?? ?? ?? ?? 48 8B D0 4C 8B C3 48 8B CF 48 8B 5C 24 ?? "
    "48 83 C4 ?? 5F E9 ?? ?? ?? ?? CC CC CC CC CC CC 48 89 5C 24 ?? 57",
    __fastcall, int, lua_State *L, const char *name)

SIGSCAN_FUNC(lua_setglobal,
             "48 89 5C 24 ?? 57 48 83 EC ?? 48 8B 41 ?? 48 8B DA 48 8B F9 BA "
             "?? ?? ?? ?? 48 8B 48 ?? E8 ?? ?? ?? ?? 48 8B D0 4C 8B C3 48 8B "
             "CF 48 8B 5C 24 ?? 48 83 C4 ?? 5F E9 ?? ?? ?? ?? CC CC CC CC CC "
             "CC 48 89 5C 24 ?? 48 89 74 24",
             __fastcall, void, lua_State *L, const char *name)

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




class DecorationBarn {
public:
  static DecorationBarn &instance();
  uintptr_t base();
  void update(uintptr_t game);
  int getDecorationCount();

private:
  DecorationBarn();
  uintptr_t decobarn = 0;
};

class ShoutBarn {
public:
  static ShoutBarn &instance();
  inline float GetPressure() const { return pressure; }
  inline void SetPressure(float p) { pressure = p; }

private:
  ShoutBarn();
  float pressure = 0.0;
};