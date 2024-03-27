#define _CRT_SECURE_NO_WARNINGS

#include <fcntl.h>
#include <io.h>

#include "ansicolor.h"
#include "console.h"
#include "game.h"

#include <imgui_stdlib.h>
#include <winrt/base.h>

void IgIgConsolePage::draw() {
  if (ImGui::BeginTabItem(name.c_str())) {
    const float heightReserved =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -heightReserved),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      if (ImGui::BeginPopupContextWindow()) {
        ImGui::Checkbox("Auto Scroll", &autoScroll);
        if (ImGui::Selectable("Clear Log")) {
          clear();
        }
        ImGui::EndPopup();
      }

      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
      {
        std::unique_lock lk(itemsMutex);
        for (const auto &item : items) {
          ImGui::TextAnsiUnformatted(item.c_str(), item.c_str() + item.size());
        }
      }

      if (scrollToBottomNextFrame ||
          (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
      }
      scrollToBottomNextFrame = false;

      ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
  }
}

void IgIgConsolePage::clear() {
  std::unique_lock lk(itemsMutex);
  items.clear();
}

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

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("Pages", tab_bar_flags)) {
    pageDetour.draw();
    pageStdout.draw();
  }

  ImGui::Separator();

  ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_EscapeClearsAll;
  bool reclaimFocusNextFrame = false;
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

void IgIgConsole::execCmd(const std::string &cmd) {
  pageDetour.log("\033[38m# {}", cmd);

  if (cmd.starts_with("echo")) {
    if (cmd.size() > 5) {
      pageDetour.log(cmd.substr(5));
    }
  } else if (cmd.starts_with("quit")) {
    exit(EXIT_SUCCESS);
  } else if (cmd.starts_with("exec")) {
    if (cmd.size() > 5) {
      std::unique_lock lk(pendingOperationsMutex);
      pendingOperations.push_back(luaJ_loadstring(cmd.substr(5)));
    }
  } else {
    pageDetour.log("\033[31mCommand not found");
  }
}

IgIgConsole::IgIgConsole() {
  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  CreatePipe(&stdoutPipeRead, &stdoutPipeWrite, NULL, 10240);

  std::jthread readThread([this]() {
    char buf[1024]{};
    DWORD bufSize = 0;
    while (true) {
      bufSize = 0;
      if (ReadFile(stdoutPipeRead, buf, 1024, &bufSize, NULL)) {
        pageStdout.log(std::string(buf, bufSize));
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  });
  readThread.detach();

  int redirectFd = _open_osfhandle((intptr_t)stdoutPipeWrite, _O_TEXT);
  _dup2(redirectFd, _fileno(stdout));

  FreeConsole();
}

IgIgConsole::~IgIgConsole() {
  if (stdoutPipeRead != NULL) {
    CloseHandle(stdoutPipeRead);
  }

  if (stdoutPipeWrite != NULL) {
    CloseHandle(stdoutPipeWrite);
  }
}
