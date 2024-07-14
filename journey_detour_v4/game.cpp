#include "game.h"
#include "igig/console.h"
#include "igig/hud.h"
#include "igig/igig.h"
#include "lualibs.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <winrt/base.h>

#include <mimalloc.h>

namespace fs = std::filesystem;
std::random_device rd;
std::mt19937 gen(rd());


std::vector<LobbyMember_t> LobbyMembers;

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

void LuaManager::registerGlobal(const std::string &name,
                                const std::string &code) {
  doNextFrame([name, code](lua_State *L) {
    if (luaL_loadbufferx(L, code.c_str(), code.size(), "code", 0) != LUA_OK) {
      spdlog::error("load failed: {}", lua_tostring(L, -1));
      lua_pop(L, 1);
      return;
    }
    if (lua_pcallk(L, 0, 1, 0, 0, nullptr) != LUA_OK) {
      spdlog::error("call failed: {}", lua_tostring(L, -1));
      lua_pop(L, 1);
      return;
    }
    lua_setglobal(L, name.c_str());
  });
}

void LuaManager::registerGlobal(const std::string &name, lua_CFunction func) {
  doNextFrame([name, func](lua_State *L) {
    lua_pushcfunction(L, func);
    lua_setglobal(L, name.c_str());
  });
}

void LuaManager::update(lua_State *L) {
  std::unique_lock lk(pendingOperationsMutex);
  for (auto &operation : pendingOperations) {
    operation(L);
  }
  pendingOperations.clear();
}

static void *lua_mimalloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  if (nsize == 0) {
    mi_free(ptr);
    return nullptr;
  } else {
    return mi_realloc(ptr, nsize);
  }
}

static int luaC_test(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  spdlog::info("[LuaC] message: {}", msg);
  return 0;
}

LuaManager::LuaManager() {
  spdlog::info("Loading additional Lua libraries...");
  registerGlobal("inspect", lib_inspect);
  doNextFrame(lib_journeydetour);
  registerGlobal("LuaTest", luaC_test);
}

void doImmediate(lua_State *L, std::string str) {
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
}

SIGSCAN_HOOK(PlayerShout,
             "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 "
             "54 41 56 41 57 48 81 EC ?? ?? ?? ?? 4C 8B BC 24",
             __fastcall, uint64_t, float a1, float a2, __int64 a3, __int64 a4,
             __int64 a5, __int64 a6, __int64 a7, __int64 a8, float pressure,
             __int64 a10) 
{
  auto p = ShoutBarn::instance().GetPressure();
  spdlog::info("PlayerShout: {} {} {}", a1, a2, p);
  return PlayerShout(a1, a2, a3, a4, a5, a6, a7, a8,
                     p, a10);
}

SIGSCAN_HOOK(DoShoutImpl, "40 56 57 48 81 EC ?? ?? ?? ?? 33 C0", __fastcall,
             void, __int64 a1, __int64 *a2, __int64 a3, char a4) {
  spdlog::info("DoShoutImpl: {} {} {}", *((float *)a2 + 4), *((float *)a2 + 8),
               *(__int32 *)(a3 + 48));
  return DoShoutImpl(a1, a2, a3, a4);
}

SIGSCAN_HOOK(luaC_AddDecoration,
             "40 53 48 83 EC ?? 44 8B 91 ?? ?? ?? ?? 49 69 DA", __fastcall,
             __int64, uintptr_t decorationbarn, uintptr_t resources,
             const char *meshname, const char *shadername, vec_mat *mat) {
  /*spdlog::info(
      "ptr:{},size:{},mesh:{},shader:{}",
      DecorationBarn + (*(unsigned int *)(DecorationBarn + 4407312) * 4304),
      *(unsigned int *)(DecorationBarn + 4407312), meshname, shadername);*/
  return luaC_AddDecoration(DecorationBarn::instance().base(), resources,
                            meshname,
                            shadername,
                            mat);
}

SIGSCAN_HOOK(LoadDecorations, "48 89 4C 24 ?? 55 53 57 41 54 41 56", __fastcall,
             int, __int64 decorationBarn, __int64 resourceManager,
             lua_State *L) {
  std::string filePathString;
  uintptr_t v3 = *(uintptr_t *)(decorationBarn + 4407376);
  if (v3)
    v3 = *(uintptr_t *)(v3 + 16);
  char *fileName = (char *)(v3 + 144);
  if (*((uintptr_t *)fileName + 3) >= 0x10ui64)
    filePathString = *(char **)fileName;

  spdlog::info("LoadDecorationCalled for {}!", filePathString);
  fs::path filePath = strstr(filePathString.c_str(), "Data/");

  if (filePath.extension() == ".bin") {
    filePath.replace_extension("");

    if (std::ifstream fileStream{filePath, std::ios::binary | std::ios::ate}) {
      spdlog::info("[\033[32mExternalLua\033[m] Loading {}",
                   winrt::to_string(filePath.wstring()));
      size_t fileSize = fileStream.tellg();
      std::vector<char> buf(fileSize, '\0');
      fileStream.seekg(0);
      fileStream.read(buf.data(), buf.size());
      doImmediate(L, buf.data());
      doImmediate(L, lib_loaddecorations);
    } else {
      spdlog::warn("Error while loading {}",
                   winrt::to_string(filePath.wstring()));
      return LoadDecorations(decorationBarn, resourceManager, L);
    }
  }
  return 0;
}

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
  DecorationBarn::instance().update(a1);

  doImmediate(L, "UpdateHudCameraInfo()");
  doImmediate(L, "UpdateHudLocalDudeInfo()");

  return GameUpdate(a1, a2);
}

SIGSCAN_HOOK(lua_newstate, "40 55 56 41 56 48 83 EC ?? 48 8B EA", __fastcall,
             lua_State *, lua_Alloc f, void *ud) {
  return lua_newstate(lua_mimalloc, nullptr);
}

SIGSCAN_HOOK(DebugLog, "48 89 5C 24 ?? 57 48 83 EC ?? 48 8D 59 ?? 48 8B FA",
             __fastcall, void, __int64 a1, const char *buf) {
  if (*((__int64 *)buf + 3) >= 0x10)
    IgIgPageConsole::instance().log("{}", *(const char **)buf);
  return DebugLog(a1, buf);
}

SIGSCAN_HOOK(InputRelated, "48 8B C4 55 56 57 41 55", __fastcall, bool,
             __int64 a1) {
  if (IgIg::instance().inited && IgIgGui::instance().show) {
    return true;
  }
  return InputRelated(a1);
}

SIGSCAN_HOOK(
    luaB_print,
    "48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 56 48 83 EC ?? 48 8B F9 E8",
    __fastcall, int, lua_State *L) 
{
  
  int n = lua_gettop(L); /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  std::string buffer;
  //spdlog::info("luabprint called, n:{}",n);
  for (i = 1; i <= n; i++) {
    const char *s;
    size_t l;
    lua_pushvalue(L, -1); /* function to be called */
    lua_pushvalue(L, i);  /* value to print */
    lua_call(L, 1, 1);
    s = lua_tolstring(L, -1, &l); /* get result */
    if (s == NULL) {
      spdlog::error("'tostring' must return a string to 'print'");
      return 0;
    }
      
    if (i > 1)
      buffer.push_back('\t');
    buffer += std::string(s, l);
    lua_pop(L, 1); /* pop result */
  }
  spdlog::info("Lua: {}", buffer);
  return 0;
}

SIGSCAN_HOOK(
    OnLobbyChat,
    "48 89 54 24 ?? 55 53 56 57 41 54 41 55 41 56 48 8D AC 24 ?? ?? ?? ?? B8",
    __fastcall, void, __int64 *Matchmaking, LobbyChatMsg_t *LobbyChatMsg) {

  int fromuserid;
  char buffer[4097];

  __int64 v27 = Matchmaking[2];

  // int GetLobbyChatEntry( CSteamID steamIDLobby, int iChatID, CSteamID
  // *pSteamIDUser, void *pvData, int cubData, EChatEntryType *peChatEntryType
  // );
  size_t result = (*(__int64(__fastcall **)(__int64, __int64, __int64, int *,
                                            char *, int, int *))(
      *(__int64 *)v27 + 216))(v27, Matchmaking[3], LobbyChatMsg->m_iChatID,
                              &fromuserid, buffer, 4096, NULL);

  float remoteX = *(float *)buffer;
  float remoteY = *(float *)(buffer + 4);
  float remoteZ = *(float *)(buffer + 8);

  bool exists = false;
  for (auto &member : IgIgHud::instance().LobbyMembersRenderList) {
    if (member.steamId == std::to_string(LobbyChatMsg->m_ulSteamIDUser)) {
      member.pos[0] = remoteX;
      member.pos[1] = remoteY;
      member.pos[2] = remoteZ;
      member.lastUpdate = std::chrono::steady_clock::now();

      exists = true;
    } else {
      continue;
    }
  }
  if (!exists) {
    LobbyMemberRenderContext ctx;
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    ctx.pos[0] = remoteX;
    ctx.pos[1] = remoteY;
    ctx.pos[2] = remoteZ;
    ctx.color = ImColor(dis(gen), dis(gen), dis(gen));
    ctx.steamId = std::to_string(LobbyChatMsg->m_ulSteamIDUser);
    ctx.lastUpdate = std::chrono::steady_clock::now();

    std::string persona_name;
    if (SteamFriends()) {
      auto ISteamFriends = SteamFriends();
      persona_name = (const char *)(*(__int64(__fastcall **)(__int64, __int64))(
          *(__int64 *)ISteamFriends + 56))(ISteamFriends,
                                           LobbyChatMsg->m_ulSteamIDUser);
    } else {
      persona_name = "???";
    }
    ctx.steamUsername = persona_name;

    IgIgHud::instance().LobbyMembersRenderList.push_back(ctx);
  }

  return OnLobbyChat(Matchmaking, LobbyChatMsg);
}

DecorationBarn::DecorationBarn() {}

DecorationBarn &DecorationBarn::instance() {
  static DecorationBarn DECORATION_BARN;
  return DECORATION_BARN;
}

void DecorationBarn::update(uintptr_t game) {
  decobarn = *(__int64 *)(game + 0x138);
}

uintptr_t DecorationBarn::base() { 
    return decobarn; 
}

int DecorationBarn::getDecorationCount() { 
  if (decobarn == 0)
    return 0; 
  return *(unsigned int *)(decobarn + 4407312);
}



ShoutBarn &ShoutBarn::instance() {
  static ShoutBarn SHOUT_BARN;
  return SHOUT_BARN;
}
ShoutBarn::ShoutBarn() {  }