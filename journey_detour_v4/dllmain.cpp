#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "igig/igig.h"
#include "igig/console.h"
#include "sigscan.h"

#include <imgui.h>
#include <spdlog/details/registry.h>
#include <spdlog/spdlog.h>

static void init() {
  auto sink = std::make_shared<igig_console_sink_mt>();
  spdlog::details::registry::instance().apply_all(
      [&sink](const std::shared_ptr<spdlog::logger> logger) {
        logger->sinks().push_back(sink);
      });
  auto &igig = IgIg::instance();
  igig.addDrawFunc([]() { IgIgConsole::instance().draw(); });
  igig.startHookThread();
  __sigScanDispatchAll();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hinstDLL);
    std::jthread(init).detach();
    break;
  default:
    break;
  }

  return TRUE;
}