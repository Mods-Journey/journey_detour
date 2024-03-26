#include "ansicolor.h"
#include "console.h"

IgIgConsole &IgIgConsole::instance() {
  static IgIgConsole IGIG_CONSOLE;
  return IGIG_CONSOLE;
}

void IgIgConsole::toggle() { show = !show; }

void IgIgConsole::draw() {
  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Console")) {
    ImGui::End();
    return;
  }
  const float heightReserved =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -heightReserved),
                        ImGuiChildFlags_None,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto &item : items) {
      ImGui::TextAnsiUnformatted(item.c_str(), item.c_str() + item.size());
    }
    ImGui::PopStyleVar();
  }
  ImGui::EndChild();

  ImGui::End();
}

void IgIgConsole::addLog(const std::string &log) { items.push_back(log); }

void IgIgConsole::addLog(std::string &&log) { items.push_back(log); }

void IgIgConsole::clearLog() { items.clear(); }

IgIgConsole::IgIgConsole() {}