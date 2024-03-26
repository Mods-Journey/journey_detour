#include "ansicolor.h"
#include "console.h"
#include "game.h"

#include <imgui_stdlib.h>

IgIgConsole &IgIgConsole::instance() {
  static IgIgConsole IGIG_CONSOLE;
  return IGIG_CONSOLE;
}

void IgIgConsole::toggle() { show = !show; }

void IgIgConsole::draw() {
  char temp[1024]{};
  DWORD tempLen = 0;
  BOOL readResult = ReadFile(conoutHandle, temp, 1024, &tempLen, NULL);
  if (tempLen > 0 && readResult == TRUE) {
    addLog(std::string(temp, tempLen));
  }

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
    if (ImGui::BeginPopupContextWindow()) {
      ImGui::Checkbox("Auto Scroll", &autoScroll);
      if (ImGui::Selectable("Clear Log")) {
        clearLog();
      }
      ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto &item : items) {
      ImGui::TextAnsiUnformatted(item.c_str(), item.c_str() + item.size());
    }

    if (scrollToBottomNextFrame ||
        (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
      ImGui::SetScrollHereY(1.0f);
    }
    scrollToBottomNextFrame = false;

    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::Separator();

  bool reclaimFocusNextFrame = false;

  ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_EscapeClearsAll;
  if (ImGui::InputText("Input", &inputBuf, inputTextFlags)) {
    if (!inputBuf.empty()) {
      execCmd(inputBuf);
      inputBuf.clear();
    }
    reclaimFocusNextFrame = true;
  }

  ImGui::SetItemDefaultFocus();
  if (reclaimFocusNextFrame) {
    ImGui::SetKeyboardFocusHere(-1);
  }

  ImGui::End();
}

void IgIgConsole::addLog(const std::string &log) { items.push_back(log); }

void IgIgConsole::addLog(std::string &&log) { items.push_back(log); }

void IgIgConsole::clearLog() { items.clear(); }

void IgIgConsole::execCmd(const std::string &cmd) {
  addLog(fmt::format("\033[38m# {}", cmd));

  if (cmd.starts_with("echo")) {
    if (cmd.size() > 5) {
      addLog(cmd.substr(5));
    } else {
      addLog("");
    }
  } else if (cmd.starts_with("quit")) {
    addLog("Goodbye!");
    exit(EXIT_SUCCESS);
  } else if (cmd.starts_with("exec")) {
    if (cmd.size() > 5) {
      std::unique_lock lk(pendingOperationsMutex);
      pendingOperations.push_back(luaJ_loadstring(cmd.substr(5)));
    }
  } else {
    addLog("\033[31mCommand not found");
  }
}

IgIgConsole::IgIgConsole() {
  conoutHandle = CreateFileA("CONOUT$", GENERIC_READ, FILE_SHARE_WRITE, NULL,
                          OPEN_EXISTING,
              FILE_ATTRIBUTE_NORMAL, NULL);
}

IgIgConsole::~IgIgConsole() {
  if (conoutHandle != NULL) {
    CloseHandle(conoutHandle);
  }
}