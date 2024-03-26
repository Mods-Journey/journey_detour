#include "game.h"

SIGSCAN_HOOK(
    PrintfHook,
    "48 89 4C 24 08 48 89 54 24 10 4C 89 44 24 18 4C 89 4C 24 20 53 57 B8",
    __cdecl, const char *, const char *fmt, ...) {

  va_list args;
  char buffer[1024];
  va_start(args, fmt);
  vsprintf_s(buffer, fmt, args);
  va_end(args);
  spdlog::info(buffer);
  return fmt;
}

SIGSCAN_HOOK(GameUpdate, "40 55 53 56 57 41 55 41 56 48 8D AC 24", __fastcall,
             __int64, __int64 a1, float a2) {
  lua_State *L = *(lua_State **)(a1 + 32);

  {
    std::unique_lock lk(pendingOperationsMutex);
    for (auto &func : pendingOperations) {
      func(L);
    }
    pendingOperations.clear();
  }

  return GameUpdate(a1, a2);
}

std::function<void(lua_State *L)> luaJ_loadstring(const std::string &buf) {
  return [buf](lua_State *L) {
    int loadResult =
        luaL_loadbufferx(L, buf.c_str(), buf.size(), "JourneyDetourPayload", 0);
    if (loadResult != LUA_OK) {
      spdlog::error("luaL_loadbufferx failed: {}", loadResult);
      return;
    }
    int callResult = lua_pcallk(L, 0, 0, 0, 0, nullptr);
    if (callResult == LUA_OK) {
      spdlog::info("{}", lua_tostring(L, -1));

    } else {
      spdlog::error("lua_pcallk failed: [{}] {}", callResult,
                    lua_tostring(L, -1));
      return;
    }
  };
}
