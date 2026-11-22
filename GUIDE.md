> 原样取自 Wrapper 包（`kui-cpp-wrapper-sdk-3.61.8-<平台>.zip`）的 `cpp_wrapper/README.md`：文中的 `cpp_wrapper/` 就是本仓库根目录，「包根目录」的 `bin/`、`lib/` 是本仓库的 `runtime/<平台>/`，`include/` 是 `c/include/`。从本仓库编译见 [README.md](README.md)。
> Copied from `cpp_wrapper/README.md` of the Wrapper package: its `cpp_wrapper/` is the root of this repository, the package root's `bin/` and `lib/` are `runtime/<platform>/` and its `include/` is `c/include/`. See [README.md](README.md) for building from this repository.

# KUI C++ Wrapper

**用 C++ 开发 KUI 应用，默认推荐用它。** 这是构建在 KUI 纯 C ABI 上的源码式 C++ 门面（CEF 的
`libcef_dll_wrapper` 那种做法）：保留原生 C++ SDK 的 `<kui/kui.h>`、命名空间、RAII 和主要高层调用方式，
头文件在你的工程里用你的编译器编译，唯一的二进制边界是 `kui_c` 的纯 C ABI——STL、异常、虚表都不跨 DLL。

> **发布状态：Windows x64 Full Profile GA；Linux x64 / macOS arm64 同口径 Release 包。** 自 3.61.8 起单独成包
> `kui-cpp-wrapper-sdk-<版本>-<平台>.zip`（版本跟随根 `VERSION`，与 C 包同一版本线），一个包就能用：
> `kui_c` 与 `kui` 运行库、运行时插件、FFmpeg 与许可文本（与 C 包逐字节同一份）、`kui_c.h` 与 Wrapper 头、
> `find_package(KuiC)` 配置、`cpp_wrapper/examples/native/` 下的示例源码（189 份与原生 C++ SDK 同源，另加 `native/api/`
> 下的 C++ API 巡览示例；数量以 `native_compat.cmake` 的登记为准）与运行素材；由最终 staging 自举编译的这些 Release 示例与
> `kui_cpp_demo` 在预编译示例包 `kui-cpp-wrapper-examples-*` 里。三个平台都只带 Release 运行库（2026-11-20 起 Windows 包也不再带
> `bin/debug`、`lib/debug`），也不带引擎实现源码。
> 解包验收会从源码重新编译全部示例，不依赖仓库 checkout。

## Wrapper 还是原生 C++ SDK

两者都提供 `<kui/kui.h>`。原生 C++ SDK（`kui-cpp-sdk`）保留为「完整版」：覆盖全部公开 C++ API，但约束都落在使用方身上
（依据见 `docs/design_C_ABI层与源码CppWrapper.md` §1）。

| | C++ Wrapper（`kui-cpp-wrapper-sdk`，推荐） | 原生 C++ SDK（`kui-cpp-sdk`，完整版） |
|---|---|---|
| 二进制边界 | 纯 C ABI，Wrapper 头编进你的程序 | C++ ABI，公开接口跨 DLL 传 `std::string` / `std::vector` / `std::function` 等 STL 类型 |
| Windows 使用方的 CRT | 随意：`/MD`、`/MDd`、`/MT`、`/MTd` 都行 | 必须与 `kui.dll` 一致：`/MD`，或用随包 Debug 核心配 `/MDd`；不能 `/MT` / `/MTd` |
| 编译器 | Windows 上 MSVC 2022 与 clang-cl 已验证；Linux gcc、macOS Apple clang | Windows 上只支持 MSVC |
| 升级 | C ABI 在 3.x 内只追加（冻结导出清单），升级换运行库即可 | 没有二进制兼容承诺，升级要整体重编 |
| 给使用方注入的编译设置 | MSVC 上只有 `/utf-8`（头文件带 UTF-8 中文注释），另要求 C++17 | `/utf-8`、`NOMINMAX`、`WIN32_LEAN_AND_MEAN`、`/STACK:8388608`、`KUI_HAS_*` 特性宏与 CRT 一致性守卫 |
| 覆盖面 | 冻结 C ABI 的 Full Profile，见下文「对齐范围」与「主要差异」 | 全部公开 C++ API |

同一个翻译单元里不要混用两者的头：它们定义同名的 `kui::*`。

## 使用

解包后的 Wrapper 包（推荐）：

```cmake
find_package(KuiC 3 CONFIG REQUIRED)   # -DKuiC_DIR=<Wrapper 包>/cmake，或把包根加入 CMAKE_PREFIX_PATH
target_link_libraries(my_app PRIVATE KuiC::Cpp)
kui_c_copy_runtime_dlls(my_app)
```

`KuiC::Cpp` 只在 Wrapper 包里有（它带 `cpp_wrapper/`）；C 包（`kui-c-sdk`）只有纯 C 的 `KuiC::C`，没有 Wrapper。
Debug 配置同样链接并部署包里唯一那份 Release 版 `kui_c.dll` / `kui.dll`：C ABI 不传 CRT 对象，你的工程用 `/MDd`、`/MTd`
照常编译、运行。`KuiC_USE_DEBUG_VARIANT` 只在包里另有 `bin/debug` 时起作用，公开包里开 / 关结果相同。

源码目录方式（不经 `find_package`）：

```cmake
set(KUI_C_ROOT "<Wrapper 包根目录>" CACHE PATH "KUI C ABI root")
add_subdirectory(<Wrapper 包根目录>/cpp_wrapper kui_cpp)
target_link_libraries(my_app PRIVATE kui_cpp)
```

```cpp
#include <kui/kui.h>

int main() {
    if (!kui::Platform::init()) return 1;
    auto window = kui::Window::create({
        .title = "KUI Wrapper", .width = 800, .height = 600
    });
    auto* document = window->document();
    document->loadHTML("<button id='run'>Run</button>");

    auto engine = kui::ScriptEngine::create();
    engine->bindDocument(document);
    window->setScriptEngine(engine.get());
    engine->eval("full_profile_ready = true", "main");

    document->getElementById("run")->addEventListener(
        "click", [](kui::Event&) {});
    return kui::Platform::run();
}
```

## Full Profile 对齐范围

- Platform、Window、Document、Element、Event、style、geometry、DevTools。
- `Platform::postToMainThread`：任意线程把回调交回主线程。与 native SDK 一样不抛异常；
  区别是 `init()` 之前或 `shutdown()` 开始之后的投递直接丢弃（native SDK 会把 init 前的
  投递留到 init 后执行）。
- Window 的 `focus` / `hasFocus`、光标、全屏、运行时切换无框与阴影、`setFrameRadii` /
  `frameRadii`、`setImeCandidatePos`；Element 的 `insertBefore`、`replaceChild`、
  `cloneNode`、`matches`、`closest`、`outerHTML`、`insertAdjacentHTML`、指针捕获；
  Document 的 `getElementsByClassName` / `getElementsByTagName`（返回元素数组，原生是
  `NodeList`）、`elementFromPoint`；Platform 的 `messageBox`、`openUrl`、`now`、
  `monitorFromPoint` / `monitorFromWindow`。没有文本节点句柄，追加文字用 Wrapper 专有的
  `Element::appendText`。
- 桌面集成：`Window::setMenuBar` / `clearMenuBar`（`MenuBarDef` / `MenuItemDef`）、
  `Window::trayIcon()`（`TrayIcon`）、`Window::shortcutManager()`（`ShortcutManager`）、
  `Document::toastManager()`（`ToastManager`）、`Window::renderer()->saveScreenshot()`。
  只能经 C ABI 实现，所以与原生有三处差异：不提供 `Window::menuBar()`（原生的
  `NativeMenuBar` 按内部命令 id 改勾选 / 禁用；这里给菜单项一个 `MenuItemDef::id`，之后用
  `Window::setMenuItemChecked` / `setMenuItemEnabled` 改，不用重新 `setMenuBar`）；
  `ShortcutManager` 不提供 `tryDispatch()`；`Renderer` 只有 `saveScreenshot()`。
- 进程内 DevTools（不起服务）：`Document::dispatchMouse` / `dispatchKey` / `dispatchIme`（注入输入）、
  `dispatchDevToolsCommand`（执行一条 CDP 命令，返回 JSON）、`boxModel`（取元素四层盒的角点）。
  原生要经 `EventDispatcher::handle*` 与 `DevToolsServer::dispatchCommandForTest`，这里是 Wrapper 专有的入口。
- 事件监听带阶段：`Element::addEventListener(type, fn, capture)`，`capture = true` 的监听在事件从根往下走时执行
  （目标自己身上最先执行，早于普通监听），里面 `stopPropagation` 会终止下行。
- 其它补口：`Router::param`、`Platform::requestIdle` / `runtimeVersion`、`ObservableBase::unbindAll`、
  `TreeView::updateNode`（只改设了的字段）、`ListView::visibleStart` / `visibleEnd` / `totalItems` / `totalHeight` / `itemHeight`、
  `ScriptEngine::lastError`、`MutationRecord::targetNodeType` / `addedNodeType` / `removedNodeType`；
  `InputPicker::open*`、`Router::unregisterView`、`Observable::unsubscribe` 返回是否成功。
- 扩展与 DevTools：`ExtensionManager::loadExtension` / `unloadExtension` / `reloadExtension` / `unloadAll` /
  `count` / `startWatching` / `stopWatching` / `isWatching`；`Window::devToolsServer()` 与
  `DevToolsServer::stop` / `isRunning` / `clientCount` / `url`（原生同名）。
- 拖放数据：`DataTransfer` 补原生同名的 `setData` / `getData` / `hasData` / `clearData` / `types` /
  `dropEffect` / `setDropEffect` / `effectAllowed` / `setEffectAllowed`（`DropEffect`、`EffectAllowed`），
  直接读写当前事件，只在该事件的监听里有效；不是拖放事件时 `Event::dataTransfer()` 返回 `nullptr`。
- 脚本引擎：`ScriptValue`（与原生同形，函数值可以是 Lua 函数，也可以是任意 C++ 可调用对象）、
  `ScriptResult::value`，`ScriptEngine::evalFile` / `callFunction` / `registerFunction` / `setGlobal` /
  `getGlobal` / `setPrintHandler` / `memoryUsageBytes` / `setMemoryLimit` / `setInstructionLimit` /
  `memoryLimitBytes` / `instructionLimit`（原生同名）。注册的 C++ 函数抛出的异常在 Lua 里成为同样文字的错误。
- 自定义元素：`CustomElementDefinition` 补原生同名的 `paintCallback` / `tickCallback` / `needsTickCallback` /
  `keyEventCallback` / `mouseEventCallback` / `wheelEventCallback` / `imeEventCallback` 及其数据结构；
  `PaintContext::displayList` 是 Wrapper 的 `DisplayList`，提供原生同签名的 `save` / `restore` / `clipRect` /
  `clipRoundedRect` / `translate` / `scale` / `rotate` / `setOpacity` / `fillRect` / `fillRoundedRect` /
  `strokeRect` / `strokeRoundedRect` / `drawText` / `drawImage` / `drawPixelBuffer` / `drawDecorationLine`
  （C ABI 不承载的排版参数抛 `KUI_ERR_NOT_SUPPORTED`）；`Element::markNeedsPaint` 与 Wrapper 专有的
  `invalidateRect` / `measureText`；`Color` 移到 `kui.h`，新增 `CornerRadii` / `TextStyle` / `TextMetrics`。
  `intrinsicSizeCallback` / `repaintContract` 为源码兼容保留，不生效（原生的固有尺寸回调同样不生效）。
- 组件（新头 `components.h`，`kui/component/*.h` 路径转发）：原生同名的 `registerBuiltinComponents`、
  `ThemeManager` / `Theme`、`VirtualList` / `VirtualListConfig` / `ScrollUnit`、`ListView` / `ListViewConfig` /
  `ListViewItem`、`TreeView` / `TreeViewConfig` / `TreeNodeData`。`VirtualList` 改为走原生实现（以前是 Wrapper
  自己的简化版，没有回收池、键盘导航与变高行）。差异：`TreeView::node()` 返回只读副本，改节点用
  `setNode()`；`ThemeManager::getTheme()` 返回的指针到下一次调用为止有效。
- 文档样式配置：`Document::StyleProfile`、`styleProfile()` / `setStyleProfile()`、`Document::create(StyleProfile)`
  （原生同名）。
- 媒体与 Lottie 播放控制：`Element` 上与原生同名的 `load()` / `loop()` / `setLoop()` / `autoplay()` /
  `setAutoplay()` / `ended()`（媒体）与 `stop()` / `seek()` / `speed()` / `isPlaying()` /
  `currentPosition()`（Lottie），另有 `mediaLoad` / `lottieSetSpeed` / `lottieState` 等显式版本。
- 弹出层 / 选择器 / 音频：`Document::popupManager()`（`PopupManager`、`MenuItem`、`TooltipTheme`、
  `Color`）、`Document::inputPicker()`（`InputPicker`）、`Document::audioManager()`（`AudioManager`、
  `PlayOptions`、`SoundInfo`）。与原生的差异：没有 `showCustomPopup()`（原生只画一个空矩形）、命中 / 绘制
  钩子与提示目标管理；`InputPicker::colorHex()` 由 HSV 换算；`AudioManager` 没有 `setAdapter()` /
  `tick()`（由文档负责）。
- `Element::computedStylePropertyValue(name)`（与原生同名，按 CSS 属性名读取计算样式）。
- 应用框架：`Document::router()`（`Router`、`RouteDefinition`、`RouteContext`）、
  `Observable<T>` 与 `Document::bindingManager()`（`BindingManager`）、
  `Document::animationManager()` / `transitionManager()`（只读查询）、`MutationObserver` /
  `ResizeObserver` / `IntersectionObserver`。与原生的差异：`Observable<T>` 以文本保存值，没有
  `beginBatch` / `endBatch` / `forceNotify`；路由自己的 `beforeEnter` / `beforeLeave` 以全局守卫
  实现；页面里的 `<kui-router-view>` 自动登记，其他容器用 `Router::registerView`；Observer
  在 `observe` 时自动注册，注册表的 `registerObserver` 不做事、`unregisterObserver` 等于
  `disconnect`，`MutationObserver::takeRecords` 返回空；动画只能查询，控制请改 CSS。
- `HttpClient` 同步/异步 networking；`fetchAsync` 的回调在 HTTP 工作线程，
  `fetchAsyncOnMainThread` 的回调在主线程（与 native SDK 相同）。
- `ScriptEngine`、DOM Lua 绑定；Canvas2D / WebGL 只能经 Lua（页面脚本）使用，见下文「主要差异」。
- dynamic extension 加载、方法调用、typed value、性能统计。
- custom elements lifecycle 与 observedAttributes。
- media、Lottie、PDF、anchored positioning、drag files、file dialogs。
- standalone 3D extension 使用的通用 extension invoke 协议。
- `VirtualList`、常用 native include path shim 与 specialized element aliases。

`RefPtr<T>` 是在 `std::shared_ptr<T>` 之上补了原生的裸指针构造：`RefPtr<Element>(element)`、
`RefPtr<Document>(window->document())` 与原生一样是「再持有一份」，共享已有的所有权
（`Element` / `Document` 是 `enable_shared_from_this`），不会重复释放；窗口销毁后这样持有的
文档仍然可用。

范围说明：这里的“可替代”指已发布 Full Profile 与 189 个现有
native example 的公共调用面。native SDK 内部 renderer/backend 类型、任意
C++ 继承扩展点或未来新增 API 不会自动穿过稳定 C ABI；如 consumer 使用了
这些能力，仍需保留 native SDK 或先补 additive C ABI。

### 主要差异（选型前先看）

上面各条里已逐项写明差异，这里汇总从原生 C++ SDK 迁过来最常碰到的几类：

- **Canvas2D / WebGL 只能经 Lua**：页面脚本里的 `<canvas>` 2D 与 WebGL API 照常可用（189 个示例里的 `canvas2d/*`、
  `webgl/*` 就是这样跑的），C++ 侧没有原生 SDK 的 `HTMLCanvasElement::surface()` / `CanvasSurface` 直接绘制。
- **没有内部渲染与布局类型**：`Renderer` 只有 `saveScreenshot()`；原生的 `RenderBackend`、`LayoutBox`、绘制列表回放等
  内部类型不对外。自定义绘制走自定义元素的 `paintCallback`（Wrapper 自己的 `DisplayList`，原生绘制命令的子集）。
- **没有 C++ 继承扩展点**：自定义元素、组件、观察者都是回调 / 配置结构，不能继承引擎类改写虚函数；需要扩展引擎本身时
  写扩展（`kuiext_*` 纯 C 协议，C 包带 hello 示例与协议头）。
- **少数 API 形态不同**：`getElementsBy*` 返回元素数组（原生是 `NodeList`）、没有文本节点句柄（用 `appendText`）、
  `Observable<T>` 以文本保存值、`TreeView::node()` 返回副本、`Window::menuBar()` / `ShortcutManager::tryDispatch()` /
  `PopupManager::showCustomPopup()` 不提供、`init()` 之前的 `postToMainThread` 直接丢弃。
- **新增 API 有时间差**：原生 SDK 新增的公开 API 要先补 additive C ABI 才能进 Wrapper（见 `sdk/c/ROADMAP.md`）。

这些能力用得上时，改用原生 C++ SDK（`kui-cpp-sdk`）并接受它的 CRT / 编译器 / 重编约束。

## 为什么必须源码交付

native C++ SDK 会跨 DLL 传递 STL 类型，consumer 必须匹配 MSVC/STL/CRT。
Wrapper 在 consumer 自己的翻译单元中编译，唯一二进制边界是纯 C ABI：

```text
application -> header-only Wrapper -> kui_c.dll -> kui.dll
                    consumer ABI       pure C ABI
```

因此没有、也不应新增预编译 `kui_cpp.lib`。已验证的 Windows 矩阵包括
MSVC 2022、clang-cl 22、Release/Debug 和 `/MD`、`/MDd`、`/MTd` consumer（每次发版对解包后的 Wrapper 包重跑）。
“源码式 Wrapper”与“预编译 Wrapper examples”并不矛盾：前者说明库门面
没有独立二进制 ABI，后者只是供用户直接运行和调试的 Release 应用程序（在单独的示例包里，见下文）。

## 生命周期

| 类型 | 语义 |
|---|---|
| `Window` | `Unique<Window>`，唯一所有权 |
| `Document` | `RefPtr<Document>` 或 Window 借用指针 |
| `Element` | Document 内稳定代理；创建结果为 `RefPtr<Element>` |
| `Event` | 仅回调期间借用 |
| `ScriptEngine` | `Unique<ScriptEngine>`；Window 不接管其所有权 |
| HTTP async response | C 层 response 由回调接管，Wrapper 转换为值对象 |

回调 trampoline 会捕获异常，绝不让 C++ 异常穿过 C function pointer。

## 构建全部 examples（189 个同源原生示例 + C++ API 巡览）

在解压后的 Wrapper 包根目录构建（`KUI_C_ROOT` 指向包根，`kui_c` 与运行库都取自包内）：

```powershell
cmake -S cpp_wrapper -B build-wrapper-examples `
  -DKUI_C_ROOT=$PWD `
  -DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON
cmake --build build-wrapper-examples --config Release
```

本包（2026-11-20 起）不带预编译示例程序，`bin/` 只有运行库（含给扩展类示例用的 `kui_ext_hello`）。不想编译就下载预编译
示例包 `kui-cpp-wrapper-examples-<版本>-<平台>.zip`（同一个 GitHub Release），直接运行其中的 `bin/kui_cpp_*.exe` 与
`bin/kui_cpp_demo.exe`（Linux / macOS 无扩展名）：它们与运行库（与本包逐字节相同）、`assets/` 在同一目录，是打包时只用
最终 Wrapper staging 重新配置和编译的产物。自己编出的示例按「可执行文件所在目录」找 `assets/`：运行前把
`cpp_wrapper/examples/assets` 复制到输出目录（如 `build-wrapper-examples/Release/assets`）。

仓库开发态直接复用权威 `examples/native`：

```powershell
cmake -S sdk/cpp_wrapper -B sdk/cpp_wrapper/build-full-profile `
  -DKUI_C_ROOT=<仓库根>/sdk/c `
  -DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON
cmake --build sdk/cpp_wrapper/build-full-profile --config Release
ctest --test-dir sdk/cpp_wrapper/build-full-profile -C Release --output-on-failure
```

`native_compat.cmake` 对 189 个同源原生 target 做精确数量门禁，C++ API 巡览在文件末尾的生成块里登记。仓内不维护第二份
手工副本；打包脚本把权威源码物化到 Wrapper 包，再从最终 staging 自举编译全部
`kui_cpp_*.exe` 与 demo，放进示例包。数量只由 `scripts/derived_sdk_checks.py --wrapper-example-count`（数 `native_compat.cmake`
里登记的 `native/.../main.cpp`）读一处；示例包的 `PREBUILT_ARTIFACTS.json` 与它的验收（`scripts/verify_examples_packages.py`，
全部程序启动）按它计数；SDK 包的解包门（`scripts/verify_cpp_wrapper_sdk_package.py`）再次从源码编译全部示例与 demo。

## 调试与发布边界

- 只有 Release 运行库，不带符号；Debug 构建的使用方同样链接、运行它（见上文「使用」）。
- 包根有 `THIRD_PARTY_NOTICES.md`、`BUILD_INFO.txt`、覆盖全部文件的 `SHA256SUMS.txt` 与 `verify_package.py`（解包后自检），
  另带 `tools/devtools_mcp/`（调试运行中的 KUI 程序）与 `docs/`（与语言无关的文档），口径与 C 包相同。
  C ABI 与 Wrapper 同一版本线，包根 `CHANGELOG.md` 就是 C SDK 的那一份。
- extension/media/3D/PDF/FFmpeg runtime 由 `kui_c_copy_runtime_dlls()` 部署。
- Wrapper header 可由 MSVC、clang-cl、gcc（Linux）与 Apple clang（macOS）编译；MinGW 本轮跳过。
- Linux / macOS 上 Wrapper 的全部定义是隐藏可见性（`KUI_CPP_HIDDEN_BEGIN/END`）：它们与原生 SDK 同名，
  若进入程序的动态符号表，ELF 会把 `libkui_c` 对原生 `libkui` 的调用抢到这些同名定义上（启动即递归崩溃）。
- 不要在同一翻译单元混用 Wrapper 与 native SDK 头；两者定义相同的
  `kui::*` 公共名字。

旧 `<kui_cpp.h>` value/handle API继续保留给既有 consumer；新项目与 native
SDK 迁移统一使用 `<kui/kui.h>`。

## License

MIT，见仓库根目录 `LICENSE`。
