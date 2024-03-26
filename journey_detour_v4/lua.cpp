#include "lua.h"
#include "silver-bun.h"

#include <detours/detours.h>

std::vector<__SigScanEntry> __sigScanEntries;

void __sigScanDispatchAll() {
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  for (const auto &entry : __sigScanEntries) {
    *entry.target = CModule("Journey.exe").FindPatternSIMD(entry.sig.c_str());
    spdlog::info("{} -> 0x{:X}", entry.name, *entry.target);
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

SIGSCAN_HOOK(GameUpdate, "40 55 53 56 57 41 55 41 56 48 8D AC 24", __fastcall,
             __int64, __int64 a1, float a2) {
  return GameUpdate(a1, a2);
}