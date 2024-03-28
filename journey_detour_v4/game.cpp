#include "game.h"

#include <filesystem>
#include <fstream>

#include <winrt/base.h>

namespace fs = std::filesystem;

SIGSCAN_HOOK(
    LoadEmbeddedLuaFile,
    "40 55 41 56 41 57 48 83 EC ?? 48 8D 6C 24 ?? 48 89 5D ?? 48 8B DA",
    __fastcall, int, lua_State* a1, const char *a2) {
  fs::path filePath = strstr(a2, "Data/");
  if (std::ifstream fileStream{filePath, std::ios::binary | std::ios::ate}) {
    spdlog::info("[ExternalLua] Loading {}",
                 winrt::to_string(filePath.wstring()));
    size_t fileSize = fileStream.tellg();
    std::vector<char> buf(fileSize, '\0');
    fileStream.seekg(0);
    fileStream.read(buf.data(), buf.size());
    return luaL_loadbufferx(a1, buf.data(), buf.size(),
                            filePath.string().c_str(), "bt");
  } else {
    spdlog::warn("[ExternalLua] Missing {}",
                 winrt::to_string(filePath.wstring()));
    return LoadEmbeddedLuaFile(a1, a2);
  }
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
