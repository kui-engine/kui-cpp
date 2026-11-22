# 用 C++ 接入 KUI：快速上手

[English](QUICKSTART.md) · **简体中文**

[English](QUICKSTART.md)

用 C++ Wrapper 把一个 HTML 页面做成原生窗口：一张带小表单和计数器的卡片，每点一次按钮，计数加一。

<p align="center"><img src="docs/media/quickstart.gif" width="360" alt="快速开始窗口的录屏：输入姓名、选套餐和日期、切换选项、拖动滑块，再点三次按钮"></p>
<p align="center"><sub>真实窗口的录屏：改表单，再点三次按钮。</sub></p>

页面就是普通的 HTML 和 CSS。除了计数器，还有一小块表单（文本框、下拉选择、日期选择、复选框、单选和滑块）。这些控件由 KUI 自己绘制和处理，不需要写代码：上面的录屏里依次输入了姓名、选了套餐和日期、切换了选项、拖了滑块，又点了按钮。

Wrapper 的头文件（`include/kui/`）在你的工程里用你的编译器编译，唯一的二进制边界是 `kui_c.dll` 的纯 C ABI。
生命周期和与原生 C++ SDK 的差异见 `GUIDE.md`。

## 1. 要什么

- Visual Studio 2022（装上 C++ 工作负载，即 MSVC）、CMake 3.20 或更新、Git。Wrapper 要求 C++17。
- 在这些版本上验证过：Windows 10 x64，MSVC 19.43（Visual Studio 2022 17.13），CMake 3.31.6，Git 2.45。

> 用普通的 PowerShell 就行，CMake 会自己找到 Visual Studio。

## 2. 拿到 SDK

```powershell
git clone --depth 1 https://github.com/kui-engine/kui-cpp.git
$KUI = "$PWD\kui-cpp"
```

要固定版本时写成 `git clone --depth 1 --branch v3.61.8 https://github.com/kui-engine/kui-cpp.git`。

> 请保留 `--depth 1`：每个版本都把运行库提交进历史，完整历史很大。

## 3. 建一个空项目

在克隆目录旁边新建文件夹 `hello-kui`，放三个文件：`CMakeLists.txt`、`main.cpp`、`app.html`。

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

<details><summary>app.html（界面，复制即可）</summary>

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

## 4. 运行库放哪

`find_package(KuiC)` 读的是 `$KUI/cmake/KuiCConfig.cmake`。`KuiC::Cpp` 加上 Wrapper 头文件（`$KUI/include`）和
`kui_c.h`（`$KUI/c/include`），要求 C++17，并链接 `KuiC::C`；运行库在 `$KUI/runtime/windows-x64/`——`bin/` 里是
`kui_c.dll`、`kui.dll` 和各插件，`lib/` 里是导入库。Windows 在 exe 所在目录找 DLL，所以
`kui_c_copy_runtime_dlls(hello_kui)` 在每次构建后把它们复制到输出目录；那条自定义命令对 `app.html` 做同样的事。
发给别人时，把整个输出目录一起拷走。

## 5. 最小窗口

1. 在跑界面的线程上调用 `kui::Platform::init()`，KUI 的调用都放在这个线程上（其他线程用
   `kui::Platform::postToMainThread` 把工作交回来）。
2. 填好 `kui::WindowConfig`，调用 `kui::Window::create`；`width`、`height` 是客户区大小，窗口默认居中并直接显示。
3. `window->document()` 是窗口自己的文档，`loadFile` 把页面加载进去。

## 6. 一个 DOM 更新

`getElementById` 返回 `kui::Element*`，没有这个元素时返回 `nullptr`。每次点击用 `setTextContent` 把 `#count` 的文字
换成新的数字。字符串是 UTF-8 的 `std::string`。

## 7. 一个事件监听

`addEventListener` 接受任何以 `kui::Event&` 为参数的可调用对象；这里的 lambda 捕获了计数和 `#count` 元素。它在主线程
上执行，可以在里面改 DOM。调用返回一个 id，需要移除时用 `removeEventListener("click", id)`。

## 8. 资源释放

- `kui::Window::create` 返回 `std::unique_ptr`（`kui::Unique<Window>`），它是原生窗口唯一的所有者；销毁它就关闭窗口，
  并放掉窗口的文档。
- `window->document()` 和 `getElementById` 拿到的 `Element*` 都是借用的：不要 delete，窗口销毁后不能再用。监听的闭包
  存在元素上，随元素一起销毁。
- 要让文档比窗口活得久，改为持有 `kui::RefPtr<kui::Document>`（即 `shared_ptr`）。
- `kui::Event&` 只在回调期间有效。
- 所有窗口销毁之后再调用 `kui::Platform::shutdown()`，所以窗口放在 `try` 块里面。

## 9. 构建与运行

在放着克隆目录和 `hello-kui` 的文件夹里，用 Windows PowerShell：

```powershell
cd hello-kui
cmake -S . -B build "-DKuiC_DIR=$KUI/cmake"
cmake --build build --config Release --parallel 4
cd build\Release
.\hello_kui.exe
```

"KUI Quick Start" 窗口出现，每点一次数字加一；关掉窗口程序就结束。

> 这是控制台程序，运行时会同时出现一个控制台窗口，错误信息打印在那里。

Linux 与 macOS：本页只在 Windows x64 上验证过。差异在于运行库目录（`$KUI/runtime/linux-x64/lib` 或
`$KUI/runtime/macos-arm64/lib`）、编译器（gcc、Apple clang）、单配置构建（`-DCMAKE_BUILD_TYPE=Release`，程序在
`build/` 下），以及程序靠指向克隆目录的绝对 RPATH 找库、而不是复制库。三个平台都验证过的命令见仓库 `README.md` 的
「编译并运行 demo 与一个示例」一节。

## 10. 常见问题

- **提示 "app.html not found in the current directory"。** 页面路径相对的是当前目录，不是 exe 所在目录。从程序自己的
  目录启动（在资源管理器里双击就是这样），或者传绝对路径。`loadFile` 目前对不存在的文件不抛异常，所以程序改为检查
  元素是否找得到。
- **提示「由于找不到 kui_c.dll，无法继续执行代码」（系统错误框，或退出码 `0xc0000135`）。** DLL 不在 exe 旁边：
  保留 `kui_c_copy_runtime_dlls(...)`，或把 `$KUI/runtime/windows-x64/bin/*` 复制到 exe 旁边。
- **Debug 构建与 CRT。** 只带 Release 运行库，Debug 构建链接的是同一个 `kui_c.dll`。C++ 类型不跨 DLL 边界，所以用哪种
  CRT 都可以（`/MD`、`/MDd`、`/MT`、`/MTd`）。
- **中文 Windows 上编译报几百个错。** 头文件里有 UTF-8 注释，MSVC 要加 `/utf-8`；`KuiC::Cpp` 会自动加上，所以请链接
  这个目标，不要只加头文件路径。
