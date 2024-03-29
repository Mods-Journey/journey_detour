#pragma once

#include <chrono>

class IgIgPageMistats {
public:
  static IgIgPageMistats &instance();

  void draw();

private:
  std::chrono::time_point<std::chrono::steady_clock> lastUpdate;
  std::string buf;

  IgIgPageMistats();
};