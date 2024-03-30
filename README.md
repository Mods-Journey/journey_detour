# journey_detour

## Features
- ImGui console with Lua code execution and stdout redirection
- Loads external Lua files directly in `Data/`
- Uses mimalloc as Lua allocator to remove memory limitations

## Build
1. Setup vcpkg according to [the instructions](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-msbuild?pivots=shell-cmd).

2. Install all required dependencies in triplet `x64-windows-static-md`.
   - `detours`
   - `spdlog[wchar]`
   - `imgui[dx11-binding,win32-binding]`
   - `mimalloc`

3. Select `Release` + `x64` profile and build.
