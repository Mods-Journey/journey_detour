#include "igig.h"
#include "fontawesome.h"

#include <thread>

#include <detours/detours.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>

typedef HRESULT(WINAPI *D3D11CreateDeviceAndSwapChain_t)(
    _In_opt_ IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType,
    HMODULE Software, UINT Flags,
    _In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
    UINT FeatureLevels, UINT SDKVersion,
    _In_opt_ CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
    _COM_Outptr_opt_ IDXGISwapChain **ppSwapChain,
    _COM_Outptr_opt_ ID3D11Device **ppDevice,
    _Out_opt_ D3D_FEATURE_LEVEL *pFeatureLevel,
    _COM_Outptr_opt_ ID3D11DeviceContext **ppImmediateContext);

typedef HRESULT(STDMETHODCALLTYPE *IDXGISwapChain__Present_t)(
    IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags);

extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                       WPARAM wParam,
                                                       LPARAM lParam);

IDXGISwapChain__Present_t origPresent = nullptr;
WNDPROC origWndProc = nullptr;

static LRESULT CALLBACK igigWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
    return true;
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = io.WantCaptureMouse;

  if (io.WantCaptureMouse) {
    return true;
  }

  return CallWindowProc(origWndProc, hWnd, msg, wParam, lParam);
}

const ImWchar FONT_AWESOME_GLYPH_RANGES[] = {
    0xE005,
    0xF8FF,
    0,
};

static HRESULT STDMETHODCALLTYPE igigPresent(IDXGISwapChain *pSwapChain,
                                             UINT SyncInterval, UINT Flags) {
  auto &igig = IgIg::instance();
  if (!igig.inited) {
    if (igig.pDevice.try_capture(pSwapChain, &IDXGISwapChain::GetDevice)) {
      igig.pDevice->GetImmediateContext(igig.pContext.put());

      DXGI_SWAP_CHAIN_DESC swapChainDesc{};
      pSwapChain->GetDesc(&swapChainDesc);
      igig.window = swapChainDesc.OutputWindow;

      ImGui::CreateContext();

      ImGui_ImplWin32_Init(igig.window);
      ImGui_ImplDX11_Init(igig.pDevice.get(), igig.pContext.get());

      auto &io = ImGui::GetIO();

      io.Fonts->AddFontDefault();

      ImFontConfig fontConfigYaHei;
      fontConfigYaHei.GlyphOffset = ImVec2(0.0, 1.0);
      fontConfigYaHei.MergeMode = true;
      fontConfigYaHei.PixelSnapH = true;
      fontConfigYaHei.OversampleH = 1;
      io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 15.0f,
                                   &fontConfigYaHei,
                                   io.Fonts->GetGlyphRangesChineseFull());

      ImFontConfig fontConfigFontAwesome;
      strcpy_s(fontConfigFontAwesome.Name, "FontAwesome");
      fontConfigFontAwesome.GlyphOffset = ImVec2(0.0, 2.0);
      fontConfigFontAwesome.MergeMode = true;
      fontConfigFontAwesome.PixelSnapH = true;
      fontConfigFontAwesome.OversampleH = 1;
      io.Fonts->AddFontFromMemoryCompressedBase85TTF(
          FontAwesome_compressed_data_base85, 13.0f, &fontConfigFontAwesome,
          FONT_AWESOME_GLYPH_RANGES);

      ImGui_ImplDX11_CreateDeviceObjects();
      origWndProc = (WNDPROC)SetWindowLongPtrW(igig.window, GWLP_WNDPROC,
                                               (LONG_PTR)igigWndProc);

      igig.inited = true;
    } else {
      spdlog::warn("igIgPresent init failed");
      return origPresent(pSwapChain, SyncInterval, Flags);
    }
  }

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  for (const auto &func : igig.drawFuncs) {
    func();
  }

  ImGui::EndFrame();
  ImGui::Render();

  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  return origPresent(pSwapChain, SyncInterval, Flags);
}

IgIg::IgIg() {}

IgIg::~IgIg() { free(methodsTable); }

bool IgIg::tryHook() {
  if (methodsTable == nullptr) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.hIcon = NULL;
    windowClass.hCursor = NULL;
    windowClass.hbrBackground = NULL;
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = L"IgIg";
    windowClass.hIconSm = NULL;

    RegisterClassExW(&windowClass);

    HWND window = CreateWindowW(
        windowClass.lpszClassName, L"IgIg DirectX Window", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

    HMODULE libD3D11 = GetModuleHandleW(L"d3d11.dll");

    if (libD3D11 == NULL) {
      DestroyWindow(window);
      UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
      spdlog::warn("IgIg hook failed on GetModuleHandleW: 0x{:X}",
                   (uint32_t)GetLastError());
      return false;
    }

    D3D11CreateDeviceAndSwapChain_t pD3D11CreateDeviceAndSwapChain =
        (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(
            libD3D11, "D3D11CreateDeviceAndSwapChain");

    if (pD3D11CreateDeviceAndSwapChain == NULL) {
      DestroyWindow(window);
      UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
      spdlog::warn("IgIg hook failed on GetProcAddress: 0x{:X}",
                   (uint32_t)GetLastError());
      return false;
    }

    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_10_1,
                                               D3D_FEATURE_LEVEL_11_0};

    DXGI_RATIONAL refreshRate{};
    refreshRate.Numerator = 60;
    refreshRate.Denominator = 1;

    DXGI_MODE_DESC bufferDesc{};
    bufferDesc.Width = 100;
    bufferDesc.Height = 100;
    bufferDesc.RefreshRate = refreshRate;
    bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SAMPLE_DESC sampleDesc{};
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferDesc = bufferDesc;
    swapChainDesc.SampleDesc = sampleDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = 1;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    IDXGISwapChain *swapChain = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;

    HRESULT hresult = pD3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2,
        D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &device, &featureLevel,
        &context);

    if (hresult != S_OK) {
      DestroyWindow(window);
      UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
      spdlog::warn("IgIg hook failed on D3D11CreateDeviceAndSwapChain: 0x{:X}",
                   (uint32_t)hresult);
      return false;
    }

    methodsTable = (uintptr_t *)calloc(205, sizeof(uintptr_t));

    if (methodsTable == nullptr) {
      DestroyWindow(window);
      UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
      spdlog::warn("IgIg hook failed on calloc");
      return false;
    }

    memcpy(methodsTable, *(uintptr_t **)swapChain, 18 * sizeof(uintptr_t));
    memcpy(methodsTable + 18, *(uintptr_t **)device, 43 * sizeof(uintptr_t));
    memcpy(methodsTable + 18 + 43, *(uintptr_t **)context,
           144 * sizeof(uintptr_t));

    swapChain->Release();
    swapChain = nullptr;

    device->Release();
    device = nullptr;

    context->Release();
    context = nullptr;

    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

    origPresent = (IDXGISwapChain__Present_t)methodsTable[8];
  }

  LONG detourResult = NO_ERROR;

  detourResult = DetourTransactionBegin();
  if (detourResult != NO_ERROR) {
    spdlog::error("IgIg hook failed on DetourTransactionBegin: 0x{:X}",
                  (uint32_t)detourResult);
    return false;
  }
  detourResult = DetourUpdateThread(GetCurrentThread());
  if (detourResult != NO_ERROR) {
    spdlog::error("IgIg hook failed on DetourUpdateThread: 0x{:X}",
                  (uint32_t)detourResult);
    DetourTransactionAbort();
    return false;
  }
  detourResult = DetourAttach((PVOID *)&origPresent, igigPresent);
  if (detourResult != NO_ERROR) {
    spdlog::error("IgIg hook failed on DetourAttach: 0x{:X}",
                  (uint32_t)detourResult);
    DetourTransactionAbort();
    return false;
  }
  detourResult = DetourTransactionCommit();
  if (detourResult != NO_ERROR) {
    spdlog::error("IgIg hook failed on DetourTransactionCommit: 0x{:X}",
                  (uint32_t)detourResult);
    DetourTransactionAbort();
    return false;
  }

  return true;
}

void IgIg::startHookThread() {
  std::jthread([]() {
    auto &igig = IgIg::instance();
    while (true) {
      if (igig.tryHook()) {
        spdlog::info("IgIg hook succeeded");
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }).detach();
}

void IgIg::addDrawFunc(std::function<void()> &&f) { drawFuncs.push_back(f); }

IgIg &IgIg::instance() {
  static IgIg IGIG;
  return IGIG;
}
