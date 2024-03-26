#pragma once

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#define __CONCAT2(x, y) x##y
#define CONCAT2(x, y) __CONCAT2(x, y)

struct __SigScanEntry {
  std::string name;
  std::string sig;
  uintptr_t *target;
  void *hook;
};

extern std::vector<__SigScanEntry> __sigScanEntries;

void __sigScanDispatchAll();

class __SigScanSentinel {
public:
  __SigScanSentinel(std::string name, std::string sig, uintptr_t *target);

  __SigScanSentinel(std::string name, std::string sig, uintptr_t *target,
                    void *hook);
};

#define SIGSCAN_FUNC(name, sig, cc, ret, ...)                                  \
  namespace __sigscan_func {                                                   \
  typedef ret(cc *##name##_t)(__VA_ARGS__);                                    \
  }                                                                            \
  __sigscan_func::##name##_t name;                                             \
  __SigScanSentinel CONCAT2(__sigScanSentinel, __LINE__)(#name, sig,           \
                                                         (uintptr_t *)&name);

#define SIGSCAN_HOOK(name, sig, cc, ret, ...)                                  \
  namespace __sigscan_func {                                                   \
  typedef ret(cc *##name##_t)(__VA_ARGS__);                                    \
  }                                                                            \
  __sigscan_func::##name##_t name;                                             \
  ret cc CONCAT2(__sigScanHooked, name)(__VA_ARGS__);                          \
  __SigScanSentinel CONCAT2(__sigScanSentinel, name)(                          \
      #name, sig, (uintptr_t *)&name, CONCAT2(__sigScanHooked, name));         \
  ret cc CONCAT2(__sigScanHooked, name)(__VA_ARGS__)
