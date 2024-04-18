#pragma once
#include <vector>
#include <mutex>
#include <ctime>  
#include "vectormath/vectormath.hpp"
#include "imgui.h"




struct HullRenderContext 
{
  std::vector<float> edges;
  std::vector<float> verts;
};
struct LobbyMemberRenderContext 
{
  std::string steamId;
  std::string steamUsername;
  float pos[3];
  ImColor color;
  std::chrono::steady_clock::time_point lastUpdate;
};

class IgIgHud {
public:
  float worldMatrix[4][4]{};
  //Matrix4 worldMatrix;

  float fov;
  float aspect;
  float nearPlane;
  float farPlane;

  float fovcoeff = 1.0f;
  float screencrop = 1.0f;
  float clipz = 1.0f;

  static IgIgHud &instance();

  void draw();

  std::vector<LobbyMemberRenderContext> LobbyMembersRenderList; 
  std::vector<HullRenderContext*> hullRenderList;
  std::mutex hullRenderListMutex;

  float localDudePos[3];
  std::string remoteDudeName;

private:


  IgIgHud();
};