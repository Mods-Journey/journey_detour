#pragma once
#include <chrono>
class IgIgLobbyBrowser {
public:
 
  static IgIgLobbyBrowser &instance();

  void draw();

  std::chrono::steady_clock::time_point lastLobbyUpdate;

private:

  IgIgLobbyBrowser();
};