#pragma once

#include <functional>
#include <vector>

#include <winrt/base.h>
#include <d3d11.h>

class IgIg {
public:
  HWND window = NULL;
  winrt::com_ptr<ID3D11Device> pDevice;
  winrt::com_ptr<ID3D11DeviceContext> pContext;
  
  std::vector<std::function<void()>> drawFuncs;
  bool hooked = false;
  bool inited = false;
  
  void startHookThread();
  void addDrawFunc(std::function<void()> &&f);

  static IgIg &instance();

private:
  uintptr_t* methodsTable = nullptr;

  IgIg();
  ~IgIg();

  bool tryHook();
};