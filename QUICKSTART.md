# KUI from C++: quickstart

**English** · [简体中文](QUICKSTART.zh-CN.md)

[中文](QUICKSTART.zh-CN.md)

Build a native window from an HTML page with the C++ Wrapper: a card with a small form and a counter that goes up each time you click the
button.

<p align="center"><img src="docs/media/quickstart.gif" width="360" alt="The quickstart window, recorded: typing a name, picking a plan and a date, switching options, dragging the slider and clicking the button three times"></p>
<p align="center"><sub>A real window, recorded: edit the form, then click the button three times.</sub></p>

The page is ordinary HTML and CSS. Next to the counter it has a small form (text input, select, date picker, checkbox, radio buttons and a slider). KUI draws and handles those controls itself, so they work without any code: the recording above types a name, picks a plan and a date, switches the options, drags the slider and clicks the button.

The Wrapper headers (`include/kui/`) compile inside your project with your compiler; the only binary boundary is the pure
C ABI of `kui_c.dll`. Lifetimes and the differences from the native C++ SDK are in `GUIDE.md`.

## 1. What you need

- Visual Studio 2022 with the C++ workload (MSVC), CMake 3.20 or newer, Git. The Wrapper needs C++17.
- Verified on Windows 10 x64 with MSVC 19.43 (Visual Studio 2022 17.13), CMake 3.31.6 and Git 2.45.

> A plain PowerShell is enough: CMake finds Visual Studio by itself.

## 2. Get the SDK

```powershell
git clone --depth 1 https://github.com/kui-engine/kui-cpp.git
$KUI = "$PWD\kui-cpp"
```

For a fixed version: `git clone --depth 1 --branch v3.61.8 https://github.com/kui-engine/kui-cpp.git`.

> Keep `--depth 1`: every version commits its runtimes, so the full history is large.

## 3. Create an empty project

Next to the clone, create a folder `hello-kui` with three files: `CMakeLists.txt`, `main.cpp` and `app.html`.

```cmake
cmake_minimum_required(VERSION 3.20)
project(hello_kui LANGUAGES CXX)

find_package(KuiC 3 CONFIG REQUIRED)

add_executable(hello_kui main.cpp)
target_link_libraries(hello_kui PRIVATE KuiC::Cpp)
kui_c_copy_runtime_dlls(hello_kui)

# Put the page next to the program after every build.
add_custom_command(TARGET hello_kui POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/app.html" "$<TARGET_FILE_DIR:hello_kui>"
    VERBATIM)
```

```cpp
#include <kui/kui.h>

#include <cstdio>
#include <stdexcept>
#include <string>

int main() {
    if (!kui::Platform::init()) return 1;
    int exitCode = 0;
    int clicks = 0; // declared before the window, so it outlives the listener

    try {
        kui::WindowConfig config;
        config.title = "KUI Quick Start";
        config.width = 560; // client area
        config.height = 640;
        config.resizable = false;
        auto window = kui::Window::create(config); // Unique<Window>: sole owner

        kui::Document* doc = window->document(); // borrowed; lives as long as the window
        doc->loadFile("app.html");               // relative to the current directory
        kui::Element* count = doc->getElementById("count"); // borrowed; nullptr if missing
        kui::Element* button = doc->getElementById("inc");
        if (!count || !button) throw std::runtime_error("app.html not found in the current directory");

        button->addEventListener("click", [&clicks, count](kui::Event&) {
            count->setTextContent(std::to_string(++clicks)); // the DOM update
        });

        exitCode = kui::Platform::run(); // blocks until the window is closed
    } catch (const std::exception& e) { // KUI failures arrive as kui::Error
        std::fprintf(stderr, "%s\n", e.what());
        exitCode = 1;
    } // window destroyed here, together with its document, elements and listener

    kui::Platform::shutdown();
    return exitCode;
}
```

<details><summary>app.html (the interface, copy as is)</summary>

```html
﻿<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>KUI Quick Start</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body { height: 100%; }
  body {
    display: flex; align-items: center; justify-content: center;
    font-family: "Segoe UI", system-ui, -apple-system, "Helvetica Neue", Arial, sans-serif;
    font-size: 14px; color: #e2e8f0;
    background: linear-gradient(135deg, #0f172a 0%, #1e1b4b 55%, #312e81 100%);
  }
  .card {
    display: flex; flex-direction: column;
    width: 480px; padding: 28px 30px 20px;
    border-radius: 24px;
    background: rgba(255, 255, 255, 0.07);
    border: 1px solid rgba(255, 255, 255, 0.14);
    box-shadow: 0 24px 60px rgba(0, 0, 0, 0.45);
  }
  .head { display: flex; flex-direction: column; align-items: center; text-align: center; }
  .badge {
    display: inline-block; padding: 4px 14px; border-radius: 999px;
    font-size: 12px; font-weight: 600; letter-spacing: 2px;
    color: #c7d2fe; background: rgba(99, 102, 241, 0.28);
  }
  h1 { margin-top: 12px; font-size: 26px; font-weight: 700; color: #ffffff; }
  .sub { margin-top: 6px; font-size: 14px; color: #94a3b8; }

  .field { display: flex; flex-direction: column; margin-top: 16px; }
  .field label { margin-bottom: 6px; font-size: 12px; font-weight: 600; letter-spacing: 0.5px; color: #a5b4fc; }
  .pair { display: flex; gap: 14px; }
  .pair .field { flex: 1; }
  input[type="text"], input[type="date"], select {
    width: 100%; padding: 9px 12px; font-size: 14px; color: #f1f5f9;
    background: rgba(15, 23, 42, 0.55);
    border: 1px solid rgba(165, 180, 252, 0.28); border-radius: 10px;
  }
  input[type="text"]:focus, input[type="date"]:focus, select:focus { border-color: #818cf8; outline: none; }

  .opts { display: flex; align-items: center; justify-content: space-between; margin-top: 16px; }
  .opts label { display: flex; align-items: center; font-size: 14px; color: #cbd5e1; }
  .opts input { margin-right: 8px; }
  .radios { display: flex; gap: 16px; }
  input[type="range"] { width: 100%; }

  .action { display: flex; align-items: center; justify-content: space-between; margin-top: 22px; padding-top: 18px;
            border-top: 1px solid rgba(255, 255, 255, 0.1); }
  .counter { display: flex; align-items: baseline; }
  #count { font-size: 44px; font-weight: 800; line-height: 1; color: #a5b4fc; }
  #label { margin-left: 8px; font-size: 14px; color: #94a3b8; }
  #inc {
    padding: 12px 30px; border: 0; border-radius: 14px;
    font-size: 16px; font-weight: 600; color: #ffffff;
    background: linear-gradient(135deg, #6366f1, #8b5cf6);
    box-shadow: 0 10px 24px rgba(99, 102, 241, 0.45);
    transition: transform 0.15s, box-shadow 0.15s;
  }
  #inc:hover { transform: translateY(-2px); box-shadow: 0 14px 30px rgba(99, 102, 241, 0.6); }
  #inc:active { transform: translateY(1px); }
  .foot { margin-top: 18px; text-align: center; font-size: 12px; color: #64748b; }
</style>
</head>
<body>
  <div class="card">
    <div class="head">
      <span class="badge">KUI</span>
      <h1>Hello from KUI</h1>
      <p class="sub">A native window drawn from HTML and CSS.</p>
    </div>

    <div class="field">
      <label>NAME</label>
      <input id="name" type="text" value="Ada Lovelace">
    </div>
    <div class="pair">
      <div class="field">
        <label>PLAN</label>
        <select id="plan">
          <option>Free</option>
          <option selected>Pro</option>
          <option>Team</option>
        </select>
      </div>
      <div class="field">
        <label>START DATE</label>
        <input id="start" type="date" value="2026-12-01">
      </div>
    </div>
    <div class="opts">
      <label><input id="news" type="checkbox" checked>Email me updates</label>
      <div class="radios">
        <label><input type="radio" name="billing" checked>Monthly</label>
        <label><input type="radio" name="billing">Yearly</label>
      </div>
    </div>
    <div class="field">
      <label>VOLUME</label>
      <input id="volume" type="range" min="0" max="100" value="50">
    </div>

    <div class="action">
      <div class="counter"><div id="count">0</div><div id="label">clicks</div></div>
      <button id="inc">Click me</button>
    </div>
    <div class="foot">One engine · Windows, Linux, macOS</div>
  </div>
</body>
</html>
```

</details>

## 4. Where the runtime lives

`find_package(KuiC)` reads `$KUI/cmake/KuiCConfig.cmake`. `KuiC::Cpp` adds the Wrapper headers (`$KUI/include`) and
`kui_c.h` (`$KUI/c/include`), requires C++17 and links `KuiC::C`, whose runtime is in `$KUI/runtime/windows-x64/`:
`bin/` holds `kui_c.dll`, `kui.dll` and the plugins, `lib/` the import library. Windows looks for DLLs next to the exe,
so `kui_c_copy_runtime_dlls(hello_kui)` copies them into the output folder after each build; the custom command does the
same for `app.html`. To ship the program, copy the whole output folder.

## 5. A minimal window

1. `kui::Platform::init()` on the thread that runs the UI; KUI calls belong on that thread
   (`kui::Platform::postToMainThread` hands work over from other threads).
2. Fill a `kui::WindowConfig` and call `kui::Window::create`; `width` and `height` are the client area, and the window
   is centered and visible by default.
3. `window->document()` is the window's document; `loadFile` loads the page into it.

## 6. One DOM update

`getElementById` returns a `kui::Element*`, or `nullptr` when there is no such element. On each click,
`setTextContent` replaces the text of `#count` with the new number. Strings are `std::string` in UTF-8.

## 7. One event listener

`addEventListener` takes any callable that accepts a `kui::Event&`; the lambda here captures the counter and the
`#count` element. It runs on the main thread, and changing the DOM inside it is allowed. The call returns an id for
`removeEventListener("click", id)` if you need to remove it.

## 8. Releasing resources

- `kui::Window::create` returns a `std::unique_ptr` (`kui::Unique<Window>`), the only owner of the native window.
  Destroying it closes the window and drops the window's document.
- `window->document()` and the `Element*` from `getElementById` are borrowed: do not delete them, and do not use them
  after the window is gone. The listener closure is stored with the element and is destroyed with it.
- To keep a document beyond its window, hold a `kui::RefPtr<kui::Document>` (a `shared_ptr`) instead.
- `kui::Event&` is valid only during the callback.
- Call `kui::Platform::shutdown()` after every window is destroyed; that is why the window lives inside the `try` block.

## 9. Build and run

From the folder that holds the clone and `hello-kui`, in Windows PowerShell:

```powershell
cd hello-kui
cmake -S . -B build "-DKuiC_DIR=$KUI/cmake"
cmake --build build --config Release --parallel 4
cd build\Release
.\hello_kui.exe
```

The "KUI Quick Start" window opens and the number goes up with each click. Closing the window ends the program.

> The program is a console program, so a console window opens with it; error messages go there.

Linux and macOS: this page is verified on Windows x64 only. The differences are the runtime folder
(`$KUI/runtime/linux-x64/lib` or `$KUI/runtime/macos-arm64/lib`), the compiler (gcc, Apple clang), a single-config build
(`-DCMAKE_BUILD_TYPE=Release`, program in `build/`), and an absolute RPATH into the clone instead of copied libraries.
The repository's `README.md`, section "Building and running the demo and an example", has commands verified on all
three platforms.

## 10. Common problems

- **"app.html not found in the current directory".** The page path is relative to the current directory, not to the
  exe. Start the program from its own folder (double-clicking it in Explorer does that), or pass an absolute path.
  `loadFile` currently does not throw for a missing file, which is why the program checks the elements instead.
- **"kui_c.dll was not found" (a system error box, or exit code `0xc0000135`).** The DLLs are not next to the exe:
  keep `kui_c_copy_runtime_dlls(...)`, or copy `$KUI/runtime/windows-x64/bin/*` next to the exe.
- **Debug builds and CRT choice.** Only Release runtimes ship, and Debug builds link the same `kui_c.dll`. Because no
  C++ types cross the DLL boundary, any CRT works (`/MD`, `/MDd`, `/MT`, `/MTd`).
- **Hundreds of compile errors on a Chinese-locale Windows.** The headers have UTF-8 comments and MSVC needs `/utf-8`;
  `KuiC::Cpp` adds it, so link the target instead of only adding include paths.
