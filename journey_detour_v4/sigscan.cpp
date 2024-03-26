#include "sigscan.h"
#include "silver-bun.h"

#include <detours/detours.h>

std::vector<__SigScanEntry> __sigScanEntries;

void __sigScanDispatchAll() {
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  for (const auto &entry : __sigScanEntries) {
    *entry.target = CModule("Journey.exe").FindPatternSIMD(entry.sig.c_str());
    spdlog::info("Resolved {} -> 0x{:X}", entry.name, *entry.target);
    if (entry.hook != nullptr && *entry.target != 0) {
      DetourAttach((PVOID *)entry.target, entry.hook);
    }
  }
  DetourTransactionCommit();
}

__SigScanSentinel::__SigScanSentinel(std::string name, std::string sig,
                                     uintptr_t *target) {
  __sigScanEntries.emplace_back(name, sig, target, nullptr);
}

__SigScanSentinel::__SigScanSentinel(std::string name, std::string sig,
                                     uintptr_t *target, void *hook) {
  __sigScanEntries.emplace_back(name, sig, target, hook);
}