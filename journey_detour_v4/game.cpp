#include "game.h"
#include "lualibs.h"

#include <filesystem>
#include <fstream>

#include <winrt/base.h>

#include <mimalloc.h>

namespace fs = std::filesystem;

SIGSCAN_HOOK(
    LoadEmbeddedLuaFile,
    "40 55 41 56 41 57 48 83 EC ?? 48 8D 6C 24 ?? 48 89 5D ?? 48 8B DA",
    __fastcall, int, lua_State *a1, const char *a2) {
  fs::path filePath = strstr(a2, "Data/");
  if (std::ifstream fileStream{filePath, std::ios::binary | std::ios::ate}) {
    spdlog::info("[\033[32mExternalLua\033[m] Loading {}",
                 winrt::to_string(filePath.wstring()));
    size_t fileSize = fileStream.tellg();
    std::vector<char> buf(fileSize, '\0');
    fileStream.seekg(0);
    fileStream.read(buf.data(), buf.size());
    return luaL_loadbufferx(a1, buf.data(), buf.size(),
                            filePath.string().c_str(), "bt");
  } else {
    spdlog::warn("[\033[32mExternalLua\033[m] Missing {}",
                 winrt::to_string(filePath.wstring()));
    return LoadEmbeddedLuaFile(a1, a2);
  }
}

SIGSCAN_HOOK(GameUpdate, "40 55 53 56 57 41 55 41 56 48 8D AC 24", __fastcall,
             __int64, __int64 a1, float a2) {
  lua_State *L = *(lua_State **)(a1 + 32);
  LuaManager::instance().update(L);
  return GameUpdate(a1, a2);
}

LuaManager &LuaManager::instance() {
  static LuaManager LUA_MANAGER;
  return LUA_MANAGER;
}

void LuaManager::doNextFrame(std::function<void(lua_State *L)> &&operation) {
  std::unique_lock lk(pendingOperationsMutex);
  pendingOperations.push_back(std::move(operation));
}

void LuaManager::doNextFrame(const std::string &str) {
  doNextFrame([str](lua_State *L) {
    if (luaL_loadbufferx(L, str.c_str(), str.size(), "cmd", 0) != LUA_OK) {
      spdlog::error("load failed: {}", lua_tostring(L, -1));
      lua_pop(L, 1);
      return;
    }
    if (lua_pcallk(L, 0, 0, 0, 0, nullptr) != LUA_OK) {
      spdlog::error("call failed: {}", lua_tostring(L, -1));
      lua_pop(L, 1);
      return;
    }
  });
}

void LuaManager::update(lua_State *L) {
  std::unique_lock lk(pendingOperationsMutex);
  for (auto &operation : pendingOperations) {
    operation(L);
  }
  pendingOperations.clear();
}

LuaManager::LuaManager() {
  spdlog::info("Loading additional Lua libraries...");
  doNextFrame(lib_inspect);
  doNextFrame(lib_journeydetour);
}

static void* lua_mimalloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  if (nsize == 0) {
    mi_free(ptr);
    return nullptr;
  } else {
    return mi_realloc(ptr, nsize);
  }
}

SIGSCAN_HOOK(lua_newstate, "40 55 56 41 56 48 83 EC ?? 48 8B EA", __fastcall,
             lua_State *, lua_Alloc f, void *ud) {
  return lua_newstate(lua_mimalloc, nullptr);
}