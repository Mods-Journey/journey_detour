#pragma once
#include <vector>
#include <mutex>
struct HullRenderContext 
{
  std::vector<float> edges;
  std::vector<float> verts;
};


class IgIgHud {
public:
  float worldMatrix[4][4]{};
  //Matrix4 worldMatrix;
  float projMatrix[4][4]{};
  static IgIgHud &instance();

  void draw();

  std::vector<HullRenderContext*> hullRenderList;
  std::mutex hullRenderListMutex;

private:


  IgIgHud();
};