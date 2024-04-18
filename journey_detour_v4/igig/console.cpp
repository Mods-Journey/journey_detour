#include <fcntl.h>
#include <io.h>

#include "ansicolor.h"
#include "console.h"
#include "game.h"
#include "hud.h"

#include <imgui_stdlib.h>
#include <regex>
#include <winrt/base.h>
std::regex colorCodeRegex("\x1b\[[0-9;]+m");
std::string stripColorCodes(const std::string &input) {

  return std::regex_replace(input, colorCodeRegex, "");
}

std::string_view getLineAtIdx(size_t idx) {
  std::unique_lock lk(IgIgPageConsole::instance().itemsMutex);
  return IgIgPageConsole::instance().rawItems[idx];
}

size_t getNumLines() {
  std::unique_lock lk(IgIgPageConsole::instance().itemsMutex);
  return IgIgPageConsole::instance().rawItems.size();
}

TextSelect textSelect{getLineAtIdx, getNumLines};

IgIgPageConsole &IgIgPageConsole::instance() {
  static IgIgPageConsole IGIG_PAGE_CONSOLE;
  return IGIG_PAGE_CONSOLE;
}
#define IGIG_CONSOLE_PIPE_STDOUT
IgIgPageConsole::IgIgPageConsole() {

#ifdef IGIG_CONSOLE_PIPE_STDOUT
  try {
    winrt::check_bool(AllocConsole());

    FILE *ignored = nullptr;
    errno_t freopenResult = freopen_s(&ignored, "CONOUT$", "w", stdout);
    if (freopenResult != 0) {
      log("stdout redirect failed: freopen_s failed with {}", freopenResult);
      winrt::check_bool(FreeConsole());
      return;
    }

    winrt::check_bool(
        CreatePipe(&stdoutPipeRead, &stdoutPipeWrite, NULL, 1048576));

    stdoutPipeReadThread = std::jthread([this](std::stop_token stoken) {
      std::string buf;
      while (!stoken.stop_requested()) {
        DWORD totalBytesAvail = 0;
        if (!PeekNamedPipe(stdoutPipeRead, NULL, 0, NULL, &totalBytesAvail,
                           NULL)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }
        if (totalBytesAvail <= 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }
        if (buf.size() < totalBytesAvail) {
          buf.resize(totalBytesAvail, '\0');
        }
        DWORD realBufSize = 0;
        if (ReadFile(stdoutPipeRead, buf.data(), totalBytesAvail, &realBufSize,
                     NULL)) {
          log(buf.substr(0, realBufSize));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
    });

    int redirectFd = _open_osfhandle((intptr_t)stdoutPipeWrite, _O_TEXT);
    if (redirectFd == -1) {
      log("stdout redirect failed: _open_osfhandle failed");
      winrt::check_bool(FreeConsole());
      stdoutPipeReadThread.request_stop();
      return;
    }

    if (_dup2(redirectFd, _fileno(stdout)) == -1) {
      log("stdout redirect failed: _dup2 failed with {}", errno);
      winrt::check_bool(FreeConsole());
      stdoutPipeReadThread.request_stop();
      return;
    }

    winrt::check_bool(FreeConsole());
  } catch (winrt::hresult_error e) {
    FreeConsole();
    log("stdout redirect failed: 0x{:X} {}", (uint32_t)e.code(),
        winrt::to_string(e.message()));
    stdoutPipeReadThread.request_stop();
  }

#endif
}

IgIgPageConsole::~IgIgPageConsole() {
  stdoutPipeReadThread.request_stop();
  if (stdoutPipeReadThread.joinable()) {
    stdoutPipeReadThread.join();
  }

  if (stdoutPipeRead != NULL) {
    CloseHandle(stdoutPipeRead);
  }

  if (stdoutPipeWrite != NULL) {
    CloseHandle(stdoutPipeWrite);
  }
}

void IgIgPageConsole::draw() {
  if (ImGui::BeginTabItem("Console")) {
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
      textSelect.update();
      if (ImGui::BeginPopupContextWindow()) {
        ImGui::BeginDisabled(!textSelect.hasSelection());
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
          textSelect.copy();
        ImGui::EndDisabled();

        if (ImGui::MenuItem("Select all", "Ctrl+A"))
          textSelect.selectAll();
        ImGui::EndPopup();
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
                IgIgPageConsole *console = (IgIgPageConsole *)data->UserData;
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
      if (reclaimFocusNextFrame || reclaimFocusOnShow) {
        reclaimFocusOnShow = false;
        ImGui::SetKeyboardFocusHere(-1);
      }
    }

    ImGui::EndTabItem();
  }
}

void IgIgPageConsole::clear() {
  std::unique_lock lk(itemsMutex);
  items.clear();
}

void IgIgPageConsole::execCmd(const std::string &cmd) {
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

  if (cmd.starts_with("dofile")) {
    if (cmd.size() > sizeof("dofile")) {
      LuaManager::instance().doNextFrame("dofile(\"" +
                                         cmd.substr(sizeof("dofile")) + "\")");
    } else {
      log("\033[31mExpected lua filename, got nothing");
    }
    return;
  }

  if (cmd.starts_with("tp")) {
    if (cmd.size() > sizeof("tp")) {

      if (cmd.substr(sizeof("tp")) == "goto") {
        LuaManager::instance().doNextFrame(
            "game:playerBarn():GetLocalDude():SetPos(game:playerBarn():"
            "GetRemoteDude():GetPos())");
        return;
      }
      if (cmd.substr(sizeof("tp")) == "bring") {
        LuaManager::instance().doNextFrame(
            "game:playerBarn():GetRemoteDude():SetPos(game:playerBarn():"
            "GetLocalDude():GetPos())");
        return;
      }

      std::string target_pos = cmd.substr(sizeof("tp"));
      LuaManager::instance().doNextFrame("game:playerBarn():GetLocalDude():SetPos({" + target_pos + "})");
    } else {
      log("\033[31mExpected lua filename, got nothing");
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

  if (cmd.starts_with("coeff")) {
    if (cmd.size() > sizeof("coeff")) {

      IgIgHud::instance().fovcoeff = stof(cmd.substr(sizeof("coeff")));
    } else {
      log("\033[31mExpected coeff, got nothing");
    }
    return;
  }

  if (cmd.starts_with("crop")) {
    if (cmd.size() > sizeof("crop")) {

      IgIgHud::instance().screencrop = stof(cmd.substr(sizeof("crop")));
    } else {
      log("\033[31mExpected crop, got nothing");
    }
    return;
  }

  if (cmd.starts_with("dist")) {
    if (cmd.size() > sizeof("dist")) {

      IgIgHud::instance().clipz = stof(cmd.substr(sizeof("dist")));
    } else {
      log("\033[31mExpected dist, got nothing");
    }
    return;
  }

  log("\033[31mCommand not found");
}

int IgIgPageConsole::TextEditCallBack(ImGuiInputTextCallbackData *data) {
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
      std::string historyStr =
          (historyPos >= 0 && (size_t)historyPos < history.size())
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
