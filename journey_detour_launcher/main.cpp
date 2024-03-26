#include <filesystem>
#include <iostream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winrt/base.h>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

int main(int argc, char **argv) {
  try {
    winrt::check_bool(SetConsoleOutputCP(CP_UTF8));

    fs::path dllPath;

    if (argc > 1) {
      dllPath = fs::absolute(argv[1]);
      if (!fs::is_regular_file(dllPath)) {
        spdlog::error(L"\"{}\" is not a valid file", dllPath.c_str());
        std::cin.get();
        return EXIT_FAILURE;
      }
    } else {
      dllPath = fs::absolute("journey_detour_v4.dll");
      if (!fs::is_regular_file(dllPath)) {
        dllPath = fs::absolute("journey_detour_v3.dll");
        if (!fs::is_regular_file(dllPath)) {
          spdlog::error("No valid dll is found");
          std::cin.get();
          return EXIT_FAILURE;
        }
      }
    }

    std::wstring dllPathString = dllPath.wstring();

    STARTUPINFOW sInfo{};
    PROCESS_INFORMATION pInfo{};

    winrt::check_bool(CreateProcessW(L"Journey.exe", NULL, NULL, NULL, FALSE,
                                     CREATE_SUSPENDED, NULL, NULL, &sInfo,
                                     &pInfo));

    void *payload = winrt::check_pointer(VirtualAllocEx(
        pInfo.hProcess, NULL, (dllPathString.size() + 1) * sizeof(wchar_t),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    spdlog::info("Payload allocated at 0x{:X} with length {}",
                 (uintptr_t)payload,
                 (dllPathString.size() + 1) * sizeof(wchar_t));

    winrt::check_bool(
        WriteProcessMemory(pInfo.hProcess, payload, dllPathString.c_str(),
                           dllPathString.size() * sizeof(wchar_t), NULL));

    HANDLE injThread = CreateRemoteThread(pInfo.hProcess, NULL, 0,
                                          (LPTHREAD_START_ROUTINE)LoadLibraryW,
                                          payload, NULL, NULL);
    if (injThread == NULL) {
      winrt::throw_last_error();
    } else {
      if (WaitForSingleObject(injThread, INFINITE) == 0xFFFFFFFF) {
        winrt::throw_last_error();
      }
      winrt::check_bool(CloseHandle(injThread));
    }

    winrt::check_bool(VirtualFreeEx(pInfo.hProcess, payload, 0, MEM_RELEASE));

    if (ResumeThread(pInfo.hThread) == -1) {
      winrt::throw_last_error();
    }

    if (pInfo.hThread != NULL) {
      winrt::check_bool(CloseHandle(pInfo.hThread));
    }

    if (pInfo.hProcess != NULL) {
      winrt::check_bool(CloseHandle(pInfo.hProcess));
    }

  } catch (winrt::hresult_error e) {
    spdlog::error(L"0x{:X} {}", (uint32_t)e.code().value, e.message().c_str());
    std::cin.get();
  } catch (fs::filesystem_error e) {
    spdlog::error("{}", e.what());
    std::cin.get();
  }
  return EXIT_SUCCESS;
}