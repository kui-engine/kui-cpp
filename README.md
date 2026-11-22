# KUI C++ Wrapper

**English** · [简体中文](README.zh-CN.md)

More screenshots and animations: [kui](https://github.com/kui-engine/kui)

KUI is a **cross-platform native GUI engine**: the interface is written in HTML / CSS, you drive it from C, C++, Go or Rust, and there are packages for Windows x64, Linux x64 and macOS arm64. This repository is its **C++ Wrapper**, **delivered as source**: the headers compile inside your project with your compiler and your CRT, and the
only binary boundary is the pure C ABI of `kui_c` (no STL, exceptions or vtables cross the DLL). Version **3.61.8**, on the same
version line as the C ABI. This repository carries the runtimes for three platforms (`runtime/`), `kui_c.h` (`c/include/`) and the
`find_package(KuiC)` config (`cmake/`), so you can build and run right after cloning. Details (coverage, main differences,
lifetimes) are in [`GUIDE.md`](GUIDE.md) (Chinese).

From an empty project to a window, with one DOM update and one event listener: [Quick start](QUICKSTART.md).

### Getting it

```bash
git clone --depth 1 https://github.com/kui-engine/kui-cpp.git
```

The repository carries the runtimes for all three platforms (`runtime/`, 475.7 MiB in this version), so you can build and run right after cloning. **Please use `--depth 1`**: every version commits its runtimes, so the full history grows with each release (roughly one `runtime/` per version). To get another version use `git clone --depth 1 --branch v<version> …`, or `git fetch --depth 1 origin tag v<version>` in an existing clone.

### Building and running the demo and an example

Requirements: CMake and a C++20 compiler (the examples use C++20; the Wrapper itself needs only C++17). The minimum CMake version is what `CMakeLists.txt` declares (3.20); the versions actually verified for this release are in the table below.

**On Linux you also need** the system libX11, libXext, libXrandr, libGL and libstdc++ (common Debian / Ubuntu package names: `libx11-6 libxext6 libxrandr2 libgl1`; server installs lack the X11 and GL runtime by default). The programs connect to a graphical session; without a desktop prefix the commands with `xvfb-run -a` (the Go / Rust `./build.sh` runs tests and needs a display server too).

From the repository root on Windows (PowerShell; the export self-check runs exactly these commands in a clean `git clone` and the program starts and keeps running):

```powershell
cmake -S . -B build "-DKUI_C_ROOT=$PWD" -DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON
cmake --build build --config Release --target kui_cpp_demo kui_cpp_html_01_box_model
.\build\Release\kui_cpp_demo.exe
.\build\Release\kui_cpp_html_01_box_model.exe
```

Linux / macOS (bash; the same commands on macOS. Verified after export in real `git clone`s: on Linux (Debian 12, Xvfb) these exact commands build and the programs start; on macOS arm64 the build and the dependency closure pass, but **no windowed program was launched** (the build Mac is a shared machine)):

```bash
cmake -S . -B build "-DKUI_C_ROOT=$PWD" -DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kui_cpp_demo kui_cpp_html_01_box_model
./build/kui_cpp_demo
./build/kui_cpp_html_01_box_model
```

Toolchains actually verified for this release:

| Platform | OS | CMake and C / C++ compiler | Go | Rust |
|---|---|---|---|---|
| Windows x64 | Windows 10 | CMake 3.31.6, MSVC 19.43 (Visual Studio 2022); LLVM clang 22.1.6 for Go's cgo | 1.26.7 | 1.98.0 |
| Linux x64 | Debian 12 (Xvfb) | CMake 4.4.0, gcc 12.2.0 | 1.26.7 | 1.98.0 |
| macOS arm64 | macOS 15.7.2 | CMake 4.0.2, Apple clang 17.0.0 | 1.26.7 | 1.98.0 |

`-DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON` enables the 203 examples (configure reports
`kui_cpp: 189 native example sources use the C ABI facade`, plus one line for the API tours); without it you get the demo and the tests. Runtime: on Windows
`kui_c_copy_runtime_dlls` copies it next to the programs; on Linux / macOS the programs carry an absolute RPATH to this
repository's `runtime/<platform>/lib` and run inside the clone (moving the clone breaks it; to ship, put the libraries next to the
program and use a relative RPATH). Assets: the two programs above embed their pages in the source and do not read `assets/`;
examples that load pages, images or media look for `assets/` next to the executable (same rule as `GUIDE.md`), so copy it first:
`Copy-Item -Recurse examples\assets build\Release\assets` on Windows, `cp -R examples/assets build/assets` on Linux / macOS.

### Using it in your project

```cmake
set(KUI_C_ROOT "<this repository>" CACHE PATH "KUI C ABI root")
add_subdirectory(<this repository> kui_cpp)
target_link_libraries(my_app PRIVATE kui_cpp)
```

Or `find_package(KuiC 3 CONFIG REQUIRED)` (`-DKuiC_DIR=<this repository>/cmake`) with `target_link_libraries(my_app PRIVATE KuiC::Cpp)`
and `kui_c_copy_runtime_dlls(my_app)`. `cmake/KuiCConfig.cmake` is generated from the Wrapper package's file of the same name; only
the prefix (`runtime/<platform>/`) and the include directories (`c/include/`, `include/`) differ. `GUIDE.md` is copied from the
Wrapper package as is: its `cpp_wrapper/` is the root of this repository and the package root's `bin/` and `lib/` are
`runtime/<platform>/`.

### What is in runtime/

| Folder | Files | Size |
|---|---:|---:|
| `runtime/windows-x64/` | 18 | 166.7 MiB |
| `runtime/linux-x64/` | 17 | 227.7 MiB |
| `runtime/macos-arm64/` | 17 | 81.3 MiB |

Each folder holds the Wrapper package's `bin/` (the same as the C package plus the hello extension `kui_ext_hello`) and `lib/`, byte-identical to the same-platform SDK package on the Release and in the same relative layout (the Linux / macOS RPATHs rely on it).

### What is on the Release

Each version's [kui Release](https://github.com/kui-engine/kui/releases/tag/v3.61.8) has three kinds of attachments:

1. **SDK packages** `kui-<language>-sdk-<version>-<platform>.zip`: the corresponding source repository packed for one platform (`runtime/<platform>/` is `bin/` and `lib/` in the package, `native/` for Go / Rust), without prebuilt example programs; download one if you prefer not to `git clone`.
2. **Prebuilt examples packages** `kui-<language>-examples-<version>-<platform>.zip`: example programs plus the runtime; extract and run them from `bin/`, no compiler needed.
3. **Checksums** `SHA256SUMS.txt` (covering the two kinds above) and the FFmpeg corresponding source.

| Package | Contents | Windows x64 | Linux x64 | macOS arm64 |
|---|---|---|---|---|
| `kui-cpp-wrapper-sdk` | SDK (= the source repository for one platform) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-sdk-3.61.8-windows-x64.zip) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-sdk-3.61.8-linux-x64.zip) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-sdk-3.61.8-macos-arm64.zip) |
| `kui-cpp-wrapper-examples` | Prebuilt examples (run right after extraction) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-examples-3.61.8-windows-x64.zip) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-examples-3.61.8-linux-x64.zip) | [zip](https://github.com/kui-engine/kui/releases/download/v3.61.8/kui-cpp-wrapper-examples-3.61.8-macos-arm64.zip) |

### Platforms

| Platform | Status | Acceptance actually performed when packaged |
|---|---|---|
| Windows x64 | Full Profile GA | The 4 SDK packages are rebuilt from their own sources after extraction and pass the out-of-package consumer matrix; every program in the 4 prebuilt examples packages is actually launched (C 222, C++ Wrapper 190, Go 219, Rust 219). After export, the four repositories were built from real `git clone`s with the README commands and the programs launched (including the parameterless `build.ps1`) |
| Linux x64 | Release package of the same frozen C ABI | The SDK packages rebuild from their sources; the example programs are launched under Xvfb: C 221/222, C++ Wrapper 190/190, Go 218/219, Rust 218/219 — C, Go and Rust each leave out 1 tray example (Xvfb has no tray host, `kui_window_tray_create` reports not supported), allow-listed. After export, the four repositories were built from real `git clone`s with the README commands and launched under Xvfb (including the parameterless Go / Rust `build.sh`, 219 programs each) |
| macOS arm64 | Release package of the same frozen C ABI | The SDK packages rebuild from their sources (C 11 + 211, C++ Wrapper 189 + demo + 3 test suites, Go / Rust 8 + 211) without launching the rebuilt programs; the examples packages are checked for layout, manifest, byte-identical runtimes, dependencies and RPATHs, plus launching 6 windowless C examples and the C++ Wrapper demo. **Windowed programs were not launched on macOS** (the build Mac is a shared machine). After export, the four repositories were built from real `git clone`s with no parameters (including the Go / Rust `build.sh`, 219 programs each) and the dependency and rpath closure checked; only windowless programs and tests were run, **no windowed program was launched** |

All three platforms ship Release runtimes only, without debug symbols; your Debug builds link the same runtime (the C ABI passes no CRT objects across the boundary). Platform capabilities are not fully symmetric; see `docs/known_issues.md` for known differences.

### If a downloaded program does not start

The prebuilt programs and runtimes are not signed yet:

- **Windows**: for a zip downloaded by a browser, when SmartScreen says "Windows protected your PC", click "More info" → "Run anyway"; or run `Get-ChildItem -Recurse <extracted folder> | Unblock-File` in PowerShell once. Files from `git clone` carry no mark-of-the-web, so SmartScreen does not block them.
- **macOS**: Gatekeeper blocks a zip downloaded by a browser; run `xattr -dr com.apple.quarantine <extracted folder>`.
- **Linux**: if your unzip tool drops permissions, `chmod +x` the programs under `bin/` (`git clone` keeps the runtime's executable bits).

### License and third-party components

MIT, see `LICENSE` (copyright Alloy).

KUI itself is MIT. The video plugin `kui_media_video` loads FFmpeg (LGPL-3.0-or-later, an LGPL build without `--enable-gpl` / `--enable-nonfree`) as separate shared libraries; if you do not need video, leave out `kui_media_video` and the FFmpeg libraries (the audio plugin does not use FFmpeg). In the repository's `runtime/<platform>/` and in every package, `LICENSE.FFmpeg.txt` (LGPLv3) and `LICENSE.FFmpeg.GPLv3.txt` sit next to the FFmpeg libraries; all other third-party components are under permissive licenses, see `THIRD_PARTY_NOTICES.md`.

**The corresponding source of the FFmpeg libraries distributed with the binaries** is attached to the [kui Release](https://github.com/kui-engine/kui/releases/tag/v3.61.8); checksums are in `FFMPEG_SOURCE_SHA256SUMS.txt`:

- Windows x64 / Linux x64 (BtbN build `autobuild-2026-10-31-12-59`, FFmpeg `n9.0.2-39-g7adfbe40dc`): `ffmpeg-7adfbe40dcde598f93a6963d2f7b22cd75202ccc.tar.gz` (source of FFmpeg commit `7adfbe40dcde598f93a6963d2f7b22cd75202ccc`) and `FFmpeg-Builds-autobuild-2026-10-31-12-59.tar.gz` (the build scripts);
- macOS arm64 (built from the official release tarball): `ffmpeg-9.0.2.tar.xz` and its signature `ffmpeg-9.0.2.tar.xz.asc`.

Upstream: <https://github.com/FFmpeg/FFmpeg/tree/7adfbe40dcde598f93a6963d2f7b22cd75202ccc>, <https://github.com/BtbN/FFmpeg-Builds/tree/autobuild-2026-10-31-12-59>, <https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz>.
