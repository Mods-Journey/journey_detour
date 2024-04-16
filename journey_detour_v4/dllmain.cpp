#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

#include "igig/hud.h"
#include "igig/console.h"
#include "igig/igig.h"
#include "sigscan.h"

#include <imgui.h>
#include <spdlog/details/registry.h>
#include <spdlog/spdlog.h>

static void init() {

  auto sink = std::make_shared<igig_console_sink_mt>();
  spdlog::details::registry::instance().apply_all(
      [&sink](const std::shared_ptr<spdlog::logger> logger) {
        logger->sinks().clear();
        logger->sinks().push_back(sink);
      });
  spdlog::set_pattern("[%H:%M:%S.%e %^%=7l%$] %v");
  auto &igig = IgIg::instance();
  igig.addDrawFunc([]() { IgIgHud::instance().draw(); });
  igig.addDrawFunc([]() { IgIgGui::instance().draw(); });
  igig.startHookThread();
  __sigScanDispatchAll();
  /*spdlog::info("test\ntest\ntest\ntest\ntest\ntest\ntest\ntest\ntest\ntest\ntes"
               "t\ntest\ntest\ntest\ntest");*/
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH: {
    DisableThreadLibraryCalls(hinstDLL);
    std::jthread initThread(init);
    initThread.detach();
    break;
  }
  default:
    break;
  }

  return TRUE;
}