#include "hud.h"
#include "game.h"
#include "igig.h"
#include <chrono>

#define VECTORMATH_FORCE_SCALAR_MODE 1

#define DEG_TO_RAD(x) ((x) * (3.14159265358979323846f / 180.0f))

Matrix4 Float4ToMatrix4(float floats[4][4]) {
  return Matrix4(
      Vector4(floats[0][0], floats[0][1], floats[0][2], floats[0][3]),
      Vector4(floats[1][0], floats[1][1], floats[1][2], floats[1][3]),
      Vector4(floats[2][0], floats[2][1], floats[2][2], floats[2][3]),
      Vector4(floats[3][0], floats[3][1], floats[3][2], floats[3][3]));
}

Matrix4 CameraParamToProj(float fov, float aspect, float nearPlane,
    float farPlane, float screenX, float screenY) {
    float coeff = 1.0f - 0.5f * (fov / 180.f) * (fov / 180.f);
  Matrix4 projMat = Matrix4::perspective(DEG_TO_RAD(fov) * coeff, aspect,
                                         nearPlane, farPlane);

  return projMat;
}

bool WorldToScreen(Point3 objectPos, Matrix4 worldMat, Matrix4 projMat,
                   float width, float height, float *outx, float *outy) {
  auto viewMat = inverse(worldMat);
  Vector4 objPos =
      Vector4(objectPos.getX(), objectPos.getY(), objectPos.getZ(), 1);
  auto viewPos = viewMat * objPos;
  /* ImGui::Text("viewPos: %f %f %f %f", viewPos.getX(), viewPos.getY(),
               viewPos.getZ(), viewPos.getW());*/
  float coeff = 1.0f + 0.1f * (IgIgHud::instance().fov / 180.f) *
                           (IgIgHud::instance().fov / 180.f);

  auto clipPos = projMat * viewPos;
  clipPos.setX(clipPos.getX() * IgIgHud::instance().screencrop);
  clipPos.setY(clipPos.getY() * IgIgHud::instance().screencrop);

  //ImGui::Text("clipPos: %f %f %f", clipPos.getX() / clipPos.getW(),
    //          clipPos.getY() / clipPos.getW(), clipPos.getZ() / clipPos.getW());
  *outx = (clipPos.getX() / clipPos.getW() * width) / 2.0f +
          width / 2.0f; // Removed negation from X
  *outy = height - ((clipPos.getY() / clipPos.getW() * height) / 2.0f +
                    height / 2.0f); // Invert Y-axis properly

  return clipPos.getZ() > 0 && *outx >= 0 && *outy >= 0 && *outx < width &&
         *outy < height;
}

static int luaC_addHullToHudRender(lua_State *L) {
  auto rendercontext = new HullRenderContext;
  size_t edgelen = lua_rawlen(L, -1);
  for (int idx = 0; idx <= edgelen; idx++) {
    lua_pushinteger(L, idx + 1);
    lua_gettable(L, -2);
    if (lua_type(L, -1) == LUA_TNIL)
      break;
    float num = luaL_checknumber(L, -1);
    rendercontext->edges.push_back(num);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  lua_pop(L, 1);
  size_t vertlen = lua_rawlen(L, -1);
  for (int idx = 0; idx <= vertlen; idx++) {
    lua_pushinteger(L, idx + 1);
    lua_gettable(L, -2);
    if (lua_type(L, -1) == LUA_TNIL)
      break;
    float num = luaL_checknumber(L, -1);
    rendercontext->verts.push_back(num);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  std::unique_lock lk(IgIgHud::instance().hullRenderListMutex);
  IgIgHud::instance().hullRenderList.push_back(rendercontext);

  return 0;
}

static int luaC_updateLocalDudeInfo(lua_State *L) {
  IgIgHud::instance().localDudePos[2] = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  IgIgHud::instance().localDudePos[1] = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  IgIgHud::instance().localDudePos[0] = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return 0;
}

// fov aspect near far
static int luaC_updateCameraInfo(lua_State *L) {
  IgIgHud::instance().farPlane = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  IgIgHud::instance().nearPlane = luaL_checknumber(L, -1);
  lua_pop(L, 1);
    IgIgHud::instance().aspect = luaL_checknumber(L, -1);
  lua_pop(L, 1);
    IgIgHud::instance().fov = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  

  for (int index = 0; index <= 3; index++) {
    lua_pushinteger(L, index + 1);
    lua_gettable(L, -2);
    for (int index1 = 0; index1 <= 3; index1++) {
      lua_pushinteger(L, index1 + 1);
      lua_gettable(L, -2);
      IgIgHud::instance().worldMatrix[index][index1] = luaL_checknumber(L, -1);

      lua_pop(L, 1);
    }
    // Pop it off again.
    lua_pop(L, 1);
  }

  return 0;
}

IgIgHud &IgIgHud::instance() {
  static IgIgHud IGIG_HUD;
  return IGIG_HUD;
}

void IgIgHud::draw() {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(100000, 100000));
  if (ImGui::Begin("Overlay", nullptr,
                   ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoInputs)) {

    // ImGui::Text("worldMat: %f %f %f %f", worldMatrix[0][0],
    // worldMatrix[0][1],
    //             worldMatrix[0][2], worldMatrix[0][3]);
    // ImGui::Text("     %f %f %f %f", worldMatrix[1][0], worldMatrix[1][1],
    //             worldMatrix[1][2], worldMatrix[1][3]);
    // ImGui::Text("     %f %f %f %f", worldMatrix[2][0], worldMatrix[2][1],
    //             worldMatrix[2][2], worldMatrix[2][3]);
    // ImGui::Text("     %f %f %f %f", worldMatrix[3][0], worldMatrix[3][1],
    //             worldMatrix[3][2], worldMatrix[3][3]);

    // ImGui::Text("projMat: %f %f %f %f", projMatrix[0][0], projMatrix[0][1],
    //             projMatrix[0][2], projMatrix[0][3]);
    // ImGui::Text("     %f %f %f %f", projMatrix[1][0], projMatrix[1][1],
    //             projMatrix[1][2], projMatrix[1][3]);
    // ImGui::Text("     %f %f %f %f", projMatrix[2][0], projMatrix[2][1],
    //             projMatrix[2][2], projMatrix[2][3]);
    // ImGui::Text("     %f %f %f %f", projMatrix[3][0], projMatrix[3][1],
    //             projMatrix[3][2], projMatrix[3][3]);

    // auto rotQuat = Quat(cameraRot[0], cameraRot[1], cameraRot[2],
    // cameraRot[3]);

    std::string formatted =
        fmt::format("Pos: {:.2f}, {:.2f}, {:.2f}", localDudePos[0], localDudePos[1],
                    localDudePos[2]);
    
    ImGui::Text(formatted.c_str());

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    Matrix4 worldMat = Float4ToMatrix4(worldMatrix);
    Matrix4 projMat = CameraParamToProj(fov, aspect, nearPlane, farPlane,
                                        displaySize.x, displaySize.y);

    for (const auto &member : LobbyMembersRenderList) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - member.lastUpdate)
                         .count();
      if (elapsed > 30000LL)
        continue;

      float minPos[3] = {member.pos[0] - 1.0f, member.pos[1] - 1.0f,
                         member.pos[2] - 1.0f};
      float maxPos[3] = {member.pos[0] + 1.0f, member.pos[1] + 1.0f,
                         member.pos[2] + 1.0f};

      float lines[][6] = {
          // lower rectangle
          {minPos[0], minPos[1], minPos[2], maxPos[0], minPos[1], minPos[2]},
          {minPos[0], minPos[1], minPos[2], minPos[0], minPos[1], maxPos[2]},
          {maxPos[0], minPos[1], minPos[2], maxPos[0], minPos[1], maxPos[2]},
          {minPos[0], minPos[1], maxPos[2], maxPos[0], minPos[1], maxPos[2]},

          // upper rectangle
          {minPos[0], maxPos[1], minPos[2], maxPos[0], maxPos[1], minPos[2]},
          {minPos[0], maxPos[1], minPos[2], minPos[0], maxPos[1], maxPos[2]},
          {maxPos[0], maxPos[1], minPos[2], maxPos[0], maxPos[1], maxPos[2]},
          {minPos[0], maxPos[1], maxPos[2], maxPos[0], maxPos[1], maxPos[2]},

          // joining lines
          {minPos[0], minPos[1], minPos[2], minPos[0], maxPos[1], minPos[2]},
          {maxPos[0], minPos[1], minPos[2], maxPos[0], maxPos[1], minPos[2]},
          {minPos[0], minPos[1], maxPos[2], minPos[0], maxPos[1], maxPos[2]},
          {maxPos[0], minPos[1], maxPos[2], maxPos[0], maxPos[1], maxPos[2]},
      };

      for (int i = 0; i < _countof(lines); ++i) {
        float fromScreenX, fromScreenY, toScreenX, toScreenY;

        bool ws1 = WorldToScreen(Point3(lines[i][0], lines[i][1], lines[i][2]),
                                 worldMat, projMat, displaySize.x,
                                 displaySize.y, &fromScreenX, &fromScreenY);

        bool ws2 = WorldToScreen(Point3(lines[i][3], lines[i][4], lines[i][5]),
                                 worldMat, projMat, displaySize.x,
                                 displaySize.y, &toScreenX, &toScreenY);

        if (!ws1 && !ws2) {
          // if line completely not visible (neither endpoint is on screen)
          // don't draw it at all
          continue;
        }
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(fromScreenX, fromScreenY),
                          ImVec2(toScreenX, toScreenY),
                          member.color, 1.0f);
      }

      float screenX, screenY;
      bool ws0 = WorldToScreen(
          Point3(member.pos[0] - 1.0F, member.pos[1] - 1.2F, member.pos[2]),
          worldMat,
          projMat, displaySize.x, displaySize.y, &screenX, &screenY);
      
      

      if (ws0) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        
        std::string text = fmt::format(
            "{}({})", member.steamUsername != std::string("???") ? member.steamUsername : member.steamId, elapsed);

        drawList->AddText(ImVec2(screenX, screenY), member.color,
                          text.c_str());
      }
    }



    std::unique_lock lk(IgIgHud::instance().hullRenderListMutex);
    for (auto ctx : hullRenderList) {
      for (int i = 0; i < ctx->edges.size(); i = i + 2) {
        float fromScreenX, fromScreenY, toScreenX, toScreenY;

        bool ws1 = WorldToScreen(
            Point3(ctx->verts[ctx->edges[i] * 3],
                   ctx->verts[(ctx->edges[i] * 3) + 1],
                   ctx->verts[(ctx->edges[i] * 3) + 2]),
            worldMat, projMat, displaySize.x,
            displaySize.y, &fromScreenX, &fromScreenY);

        bool ws2 =
            WorldToScreen(Point3(ctx->verts[ctx->edges[i + 1] * 3],
                                 ctx->verts[(ctx->edges[i + 1] * 3) + 1],
                                 ctx->verts[(ctx->edges[i + 1] * 3) + 2]),
            worldMat, projMat, displaySize.x,
                          displaySize.y,
                          &toScreenX, &toScreenY);

        if (!ws1 && !ws2) {
          // if line completely not visible (neither endpoint is on screen)
          // don't draw it at all
          continue;
        }
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(fromScreenX, fromScreenY),
                          ImVec2(toScreenX, toScreenY),
                          IM_COL32(0, 255, 255, 255), 1.0f);
      }
    }
  }
  ImGui::End();
}

IgIgHud::IgIgHud() {

  LuaManager::instance().registerGlobal("luaC_updateLocalDudeInfo",
                                        luaC_updateLocalDudeInfo);
  LuaManager::instance().registerGlobal("luaC_updateCameraInfo",
                                        luaC_updateCameraInfo);
  LuaManager::instance().registerGlobal("luaC_addHullToHudRender",
                                        luaC_addHullToHudRender);
}
