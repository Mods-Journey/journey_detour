#define _CRT_SECURE_NO_WARNINGS

#include <fcntl.h>
#include <io.h>

#include "ansicolor.h"
#include "console.h"
#include "game.h"

#include <imgui_stdlib.h>
#include <winrt/base.h>
#include <mimalloc.h>

IgIgConsolePage::IgIgConsolePage(std::string name) : name(name) {
  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  CreatePipe(&stdoutPipeRead, &stdoutPipeWrite, NULL, 10240);

  std::jthread readThread([this]() {
    char buf[10240]{};
    DWORD bufSize = 0;
    while (true) {
      bufSize = 0;
      if (ReadFile(stdoutPipeRead, buf, 10240, &bufSize, NULL)) {
        log(std::string(buf, bufSize));
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

IgIgConsolePage::~IgIgConsolePage() {
  if (stdoutPipeRead != NULL) {
    CloseHandle(stdoutPipeRead);
  }

  if (stdoutPipeWrite != NULL) {
    CloseHandle(stdoutPipeWrite);
  }
}

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
      ImGui::EndChild();
      ImGui::Separator();

      ImGuiInputTextFlags inputTextFlags =
          ImGuiInputTextFlags_CallbackHistory |
          ImGuiInputTextFlags_EnterReturnsTrue |
          ImGuiInputTextFlags_EscapeClearsAll;
      bool reclaimFocusNextFrame = false;
      if (ImGui::InputText(
              "Input", &inputBuf, inputTextFlags,
              [](ImGuiInputTextCallbackData *data) {
                IgIgConsolePage *console = (IgIgConsolePage *)data->UserData;
                return console->TextEditCallBack(data);
              },
              (void *)this)) {
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
    }

    ImGui::EndTabItem();
  }
}

void IgIgConsolePage::clear() {
  std::unique_lock lk(itemsMutex);
  items.clear();
}

static void mi_stats_print_forward(const char *msg, void *arg) {
  IgIgConsolePage *page = (IgIgConsolePage*)arg;
  page->log(std::string(msg));
}

void IgIgConsolePage::execCmd(const std::string &cmd) {
  log("\033[38m# {}", cmd);

  historyPos = -1;

  history.push_back(cmd);

  if (cmd.starts_with("echo")) {
    if (cmd.size() > 5) {
      log(cmd.substr(5));
    }
    return;
  }

  if (cmd.starts_with("quit")) {
    exit(EXIT_SUCCESS);
  }

  if (cmd.starts_with("exec")) {
    if (cmd.size() > sizeof("exec")) {
      LuaManager::instance().doNextFrame(cmd.substr(sizeof("exec")));
    } else {
      log("\033[31mExpected lua code, got nothing");
    }
    return;
  }

  if (cmd.starts_with("inspect")) {
    if (cmd.size() > sizeof("inspect")) {
      LuaManager::instance().doNextFrame("print(inspect({}))",
                                         cmd.substr(sizeof("inspect")));
    } else {
      log("\033[31mExpected lua code, got nothing");
    }
    return;
  }

  if (cmd.starts_with("mistats")) {
    mi_stats_print_out(mi_stats_print_forward, this);
    return;
  }

  log("\033[31mCommand not found");
}

int IgIgConsolePage::TextEditCallBack(ImGuiInputTextCallbackData *data) {
  switch (data->EventFlag) {
  case ImGuiInputTextFlags_CallbackHistory: {
    int64_t prevHistoryPos = historyPos;
    if (data->EventKey == ImGuiKey_UpArrow) {
      if (historyPos == -1) {
        historyPos = history.size() - 1;
      } else if (historyPos > 0) {
        historyPos--;
      }
    } else if (data->EventKey == ImGuiKey_DownArrow) {
      if (historyPos != -1) {
        if (++historyPos >= (int64_t)history.size()) {
          historyPos = -1;
        }
      }
    }

    if (prevHistoryPos != historyPos) {
      std::string historyStr = (historyPos >= 0 && (size_t)historyPos < history.size())
                                   ? history[historyPos]
                                   : "";
      data->DeleteChars(0, data->BufTextLen);
      data->InsertChars(0, historyStr.c_str());
    }
    break;
  }
  }
  return 0;
}

IgIgGui &IgIgGui::instance() {
  static IgIgGui IGIG_CONSOLE;
  return IGIG_CONSOLE;
}

void IgIgGui::toggle() { show = !show; }

void IgIgGui::draw() {
  if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false)) {
    toggle();
  }

  if (!show) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Journey Detour v4", &show)) {
    ImGui::End();
    return;
  }

  ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("Pages", tabBarFlags)) {
    pageConsole.draw();
  }

  ImGui::End();
}

IgIgGui::IgIgGui() {}

IgIgGui::~IgIgGui() {}
