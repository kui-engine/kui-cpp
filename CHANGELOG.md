# KUI C SDK - Changelog

C ABI 的版本号跟随 KUI 产品版本（根 `VERSION`）。从 3.61.8 起，本文件与根 `CHANGELOG.md`、
`sdk/CHANGELOG.md`、`sdk/cpp/CHANGELOG.md` 一样在每次发版时把 `[Unreleased]` 提升为版本段。
源码 C++ Wrapper 与 C ABI 同一版本线，它的变更也记在这里；C 包（`kui-c-sdk`）与 Wrapper 包（`kui-cpp-wrapper-sdk`）
都随带本文件。

## [Unreleased]

### Added

- **宿主集成类导出 21 个（471 → 492）**：`kui_window_create_ex`、`kui_window_set_enabled` / `is_enabled` / `process_frame` / `set_devtools_token_auth`、`kui_element_start_momentum_scroll` / `scroll_into_view`、`kui_script_engine_tick_timers`、`kui_event_wheel_get_delta`、`kui_element_add_event_listener_ex`（捕获阶段监听）、`kui_document_set_base_path` / `get_base_path` / `set_render_device_pixel_ratio`、`kui_console_set_enabled` / `set_stderr_output` / `set_min_level`、`kui_stack_trace_install` / `uninstall`、`kui_crash_reporter_install` / `uninstall`、`kui_platform_disable_ime`。新增实现文件 `src/kui_c_diagnostics.cpp`。
- C 包带回 C ABI 层实现 `src/`、C 测试 `tests/`、`CMakeLists.txt`、`.gitignore` 与音频插件源码 `extensions/kui_media_audio/`（含随附的
  miniaudio 单头文件与许可）：泄露检查放宽为只保护核心 `src/` 后按用户决定补回。`CMakeLists.txt` 要 `find_package(Kui)`（原生 C++ SDK，
  内部包），公开用户拿它作参考；音频插件可用包内 `KuiC::MediaAPI` 直接重建。
- 预编译示例包 `kui-c-examples-<版本>-<平台>.zip` 与 `kui-cpp-wrapper-examples-<版本>-<平台>.zip`：11 + 225 个 C 示例与 hello 扩展、
  全部 Wrapper 示例与 demo，带与 SDK 包逐字节相同的运行库，解压即可运行；C 示例包的 `bin/` 另带 `script_html/`。
- canonical C 示例增至 225 个：211 个与 C++ 原版一一对应，另加 14 个 API 巡览示例（`kui_c_api_*`）；宿主逻辑 port
  （`examples/canonical/ports/`）、就绪钩子 `generated/*/ready.c` 与 `runtime/kui_c_example_dom.h` 随包，解包验收逐项核对。
  Wrapper 示例为 189 个同源原生示例加 `native/api/` 下的 C++ API 巡览。
- 追加 6 个导出：`kui_element_get_value` / `set_value`（`<input>` / `<textarea>` / `<select>` / `<output>` 的 value，`<progress>` / `<meter>` 的数值文本）、`kui_element_get_checked` / `set_checked`、`kui_element_get_selected_index` / `set_selected_index`；冻结清单 465 → 471。C++ Wrapper 的 `Element::value()` / `setValue()` 走新导出（没有当前值概念的元素退回内容属性），新增 `checked` / `setChecked` / `selectedIndex` / `setSelectedIndex`。

### Changed

- C 包与 C++ Wrapper 包不再带预编译示例程序、`bin/assets/` 与 `PREBUILT_ARTIFACTS.json`（在示例包里）；C 包的 `bin/` 也不再带 hello
  示例扩展（用 `extensions/hello/` 重建），Wrapper 包照旧带它。
- C 包与 C++ Wrapper 包只带公开内容：C 包不再带 `src/`（C ABI 层实现）、`tests/`、`CMakeLists.txt` 与 `extensions/kui_media_audio/`
  源码（音频插件只发预编译库）；Windows 两个包不再带 `bin/debug`、`lib/debug`，Debug 配置链接并部署 Release 版 `kui_c.dll` /
  `kui.dll`（`/MD`、`/MDd`、`/MTd` 使用方验收照旧）。`KuiCConfig.cmake` 的 `KuiC_USE_DEBUG_VARIANT` 保留，包里没有 Debug 变体时
  开 / 关结果相同。（其中 `src/`、`tests/`、`CMakeLists.txt` 与音频插件源码随后又补回，见上方 Added。）

---

## [3.61.8] - 2026-11-19

### Added

- `kui_http_client_*` 在 Linux / macOS 上能用了：以前一律返回「HTTP not implemented on this platform」；内核现在用 libcurl
  （Linux，运行期加载发行版的 `libcurl.so.4` / `libcurl-gnutls.so.4`）与 NSURLSession（macOS）实现，状态码、回应体、
  Content-Type、超时、重定向、取消与回调线程的语义与 Windows 相同。C ABI 不变（`T-DEBT-POSIX-HTTPCLIENT-NOT-IMPLEMENTED`）。

### Changed

- 引擎自带的界面文字（内置右键菜单、`<input>` 选择器、文件输入框、macOS 上 `kui_platform_message_box` 的按钮、
  Toast 关闭按钮的无障碍名称、Linux 托盘通知的动作名）随系统界面语言显示：中文系统用中文，其它语言用英文；环境变量
  `KUI_UI_LANGUAGE`（`zh` / `en`）可以覆盖。C ABI 不变。

### Fixed

- 页面里显示着的本地图片文件可以被宿主原地改写（以前 Windows 上写入 / 截断失败，Linux 上截断后解码崩溃）；缓存的图保持读入时的
  内容。缓存中本地图片的编码字节计入进程私有内存，`KUI_MMAP_LOCAL_IMAGE_FILES=1` 回到映射。C ABI 不变。
- GPU 窗口上在帧内（回调里）调用 `kui_window_save_screenshot`：存本窗口最近一次上屏的帧，以前可能存下别的窗口、上一帧或白底
  半成品；这个窗口第一次这样调用时还没有保留的帧，返回失败，从下一次上屏起开始保留（`KUI_GPU_PRESENTED_FRAME_RETAIN_FROM_START=1`
  从窗口创建起保留）。帧外调用本来就会现画一帧抓取，行为不变，也不会让窗口开始保留。C ABI 不变。
- macOS 上 `kui_platform_wait_events(timeout)`：定时器或 `kui_platform_post_to_main_thread` 的回调执行后立即返回，以前要等满
  `timeout`。C ABI 不变。
- Windows 上 `kui_http_client_fetch_*`：连接在回应体收全之前断开以前返回截断的回应体且不报错，现在带错误；
  `kui_http_client_cancel_all` 之后再发的请求立即以 `Cancelled` 结束、不再发出。C ABI 不变。
- Windows 上 `kui_http_client_fetch_*` 的 `timeout_seconds`：以前 4 s 以内都要约 4 s 才超时，现在按给的秒数结束（多久没收到数据，
  与 Linux / macOS 相同）；超时的错误文字改为 `… failed: timed out (no data for <秒> s)`。C ABI 不变。
- Linux 包：`bin/` 里的扩展插件不再带构建机的绝对 RUNPATH，视频插件需要的 FFmpeg 库与 `LICENSE.FFmpeg.txt` 随包放在 `bin/`；
  以前视频插件只在构建机上能加载。`verify_c_sdk_package.py` 在 Linux 上核对包内运行时闭包。C ABI 不变。
- 视频插件随带的 FFmpeg 换成 LGPL 构建（3.61.7 及以前 `bin/` 里的是 GPL 构建）：DLL 名变为 `avcodec-63.dll` 等，旁边附
  `LICENSE.FFmpeg.txt` / `LICENSE.FFmpeg.GPLv3.txt`；`KuiCConfig.cmake` 部署运行时时一并复制两份许可文本。包验收拒绝 GPL / nonfree
  构建。C ABI 不变。
- macOS 包：`bin/` 里的扩展插件不再带构建机的 LC_RPATH，视频插件随带 LGPL 构建的 FFmpeg 与许可文本（以前链接 Homebrew 的
  FFmpeg、包里没有）；`verify_c_sdk_package.py` 在 macOS 上同样核对包内运行时闭包。C ABI 不变。
- macOS 包：`bin/` 下预编译的 hello 扩展与纯 C 音频插件的 LC_RPATH 改为 `@loader_path/../lib` 与 `@loader_path`（以前误用可执行
  程序的 `@executable_path`；两者只依赖系统库，修前也能加载）。Linux 包不变。C ABI 不变。
- Windows：`find_package(KuiC)` 后链接 `KuiC::C` / `KuiC::Cpp` / `KuiC::ExtensionAPI` / `KuiC::MediaAPI` 的 MSVC 目标自动带 `/utf-8`
  （与 C++ SDK 的 `Kui::Kui` 相同）。包里的头带 UTF-8 中文注释，以前中文 Windows 上不加 `/utf-8` 的 Wrapper 使用方编不过
  （注释吞掉下一行声明）。C ABI 不变。
- Linux：窗口句柄还没释放就调 `kui_platform_shutdown()`，之后再 `kui_window_release` 不再段错误（X 连接留到最后一个窗口释放
  才关闭）。C ABI 不变。
- Linux：页面上有过渡 / 关键帧动画时主线程不再空转（以前帧尾续帧排 0 ms 定时器，CPU 占满一个核），帧按约 16 ms 的动画帧
  节拍出。C ABI 不变。
- macOS：有动画时不再重复出帧，画面不变的帧（如 `steps()` 动画两次跳变之间）与 CPU 光栅路径不再空转；动画帧按窗口所在屏幕的
  刷新率出（120 Hz 屏 8 ms 一帧）。C ABI 不变。
- 页面里 `<select>` / `<input list>` 的原生下拉、`kui_document_show_context_menu` 在 NativeWindow 弹窗模式下的菜单不再被屏幕
  截断：下拉在下方放不下时翻到控件上方并限制高度，菜单放不下时翻到指针另一侧。C ABI 不变。
- `kui_window_minimize` 之后紧接 `kui_window_restore` / `kui_window_maximize`：macOS 上以前还原不生效（动画结束窗口照样
  最小化）、最大化被丢掉，`kui_window_is_minimized` 要过半秒才为 1；内核修复后与 Windows 一致。
- `kui_document_get_extension_state` 的 `watching`：Linux / macOS 上 `kui_document_watch_extensions` 之后以前读到 1 还是 0
  看时机；这两个平台还没有目录监视，现在如实为 0。
- `kui_window_on_resize`：Linux / macOS 上以前从不调用（内核只存回调不调；程序主动改尺寸时连 `resize` 事件也漏掉）；
  内核修复后与 Windows 一致。
- `kui_platform_post_to_main_thread`：macOS 上 `kui_platform_shutdown` 时还在排队的回调以前不在 shutdown 内部调
  `discard_fn`（内核把回调直接交给 GCD）；内核修复后与 Windows / Linux 一致。
- `kui_platform_get_monitor_from_window`：macOS 上传入 `kui_window_get_native_handle` 的结果以前返回 `KUI_ERR_INTERNAL`
  （内核把内容视图当窗口用）；说明同步改为「macOS 上是内容视图 `NSView*`」。
- `kui_script_engine_register_function` 的 C 函数返回错误时，macOS 上 Lua 拿到的错误文本以前是 `caught (...) exception`
  （内核 libkui 带了 Skia 的私有标准异常 typeinfo）；修复后与其它平台一致。
- `kui_window_save_screenshot` 写出的 PNG 不是页面画面（内核 `Renderer::saveScreenshot` 对已上屏的 GL 表面做快照）；
  内核修复后先画一帧再读回，头注释同步说明。
- `kui_element_append_child` 以前不检查把祖先（或元素自己）追加到自己子树下的情况，
  原生 `appendChild` 也不检查，结果是 DOM 变成环；现在返回 `KUI_ERR_INVALID_ARG`。

### Added

- `kui_event_get_related_target`：鼠标事件（mouseover / mouseout / mouseenter / mouseleave）与焦点事件（focus / blur）的
  `relatedTarget`，`<kui-context-menu>` 的 `menu-open` 里是被右键的元素；没有时 `KUI_ERR_NOT_FOUND`。
- `kui_document_set_default_context_menu_enabled` / `kui_document_get_default_context_menu_enabled`：引擎自带的默认右键
  菜单（剪切 / 复制 / 粘贴 / 全选）3.61.8 起默认不显示，用它对整个文档开启（`kui_document_load_html` 换页后保持）；页面也可以
  写 `<html context-menu>`。`contextmenu` 事件与 `kui_document_show_context_menu` 弹的菜单不受影响。
- Linux x64 / macOS arm64 正式包（Release），与 Windows 包同一冻结清单、同一组示例与验收口径；`find_package(KuiC)` 在这两个
  平台导入 `lib/libkui_c.so|.dylib`，`kui_c_copy_runtime_dlls()` 只把 `bin/` 里按目录加载的插件放到目标旁边。
- `kui_platform_post_to_main_thread(fn, discard_fn, user_data)`：本头文件里唯一可以从任何线程
  调用的函数，把回调交给主线程，在事件循环里异步执行（同一线程投递的按顺序）。平台未初始化或
  已开始关闭时返回 `KUI_ERR_NOT_SUPPORTED`；关闭时还在排队的任务不执行，改调可选的
  `discard_fn`（在关闭线程上），供外部运行时释放状态。
- `kui_http_client_fetch_async_on_main_thread`：与 `kui_http_client_fetch_async` 相同的请求，
  但回调在主线程执行，可以直接操作 DOM。`kui_http_client_fetch_async` 的回调一直在 HTTP 工作
  线程上执行，头文件现在写明了这一点。
- 常用窗口、DOM 与平台操作 32 个（C 头第 9 节）：`kui_window_focus` / `has_focus`、
  `set_cursor`（`KuiCCursor`）/ `set_cursor_visible`、`set_fullscreen` / `is_fullscreen`、
  `set_frameless` / `set_frameless_shadow` / `get_frame_state`、`set_frame_radii` /
  `get_frame_radii`、`set_ime_candidate_pos`；`kui_element_insert_before`、`replace_child`、
  `clone`、`matches`、`closest`、`get_outer_html`、`insert_adjacent_html`、`append_text`、
  `has_focus`、指针捕获三个；`kui_document_element_from_point`、
  `get_elements_by_class_name` / `get_elements_by_tag_name`；`kui_platform_message_box`、
  `open_url`、`now`、`get_monitor_from_point` / `get_monitor_from_window`。窗口关闭 / 聚焦
  通知继续用窗口事件 `close`、`focus`、`blur`。
- 桌面集成 25 个（C 头第 10 节）：`kui_window_set_menu_bar` / `clear_menu_bar` /
  `menu_set_checked` / `menu_set_enabled`（菜单以 POD 结构描述，每项带调用方 id，激活时回调
  该 id；C 层保存定义副本，改勾选 / 禁用时重建原生菜单，不依赖原生内部命令 id）；托盘
  `kui_window_tray_*` 13 个（创建 / 销毁 / 显隐 / 提示 / 图标 / 右键菜单 / 弹出菜单 / 气泡 /
  四种点击事件回调 / 模拟事件）；`kui_window_bind_shortcut` / `unbind_shortcut` /
  `unbind_all_shortcuts` / `set_shortcuts_enabled`；`kui_document_show_toast` /
  `dismiss_toast` / `dismiss_all_toasts` / `get_toast_count`；`kui_window_save_screenshot`。
- 应用框架 40 个（C 头第 11 节）：路由 18 个（`kui_document_router_*`：增删 / 清空 / 查询路由，
  `register_view` / `unregister_view` 登记显示视图的容器，`push` / `replace` / `go`，取当前路由
  （回调一次给出路径、模式、`:name` 参数与查询参数）/ 当前路径 / 参数 / 历史，全局进入 / 离开守卫，
  导航完成回调）；数据绑定 11 个（`KuiObservable` 字符串值：`create` / `retain` / `release` / `set` /
  `get` / `subscribe` / `unsubscribe` / `bind`（单向或双向，绑定时立即写入）/ `unbind` /
  `unbind_all`，以及 `kui_document_flush_bindings`）；动画状态 2 个（`kui_element_get_animation_state`、
  `kui_document_get_animation_counts`）；Observer 9 个（`KuiObserver`：Mutation / Resize /
  Intersection 三种创建函数、`kui_mutation_observer_observe`、`kui_observer_observe` /
  `unobserve` / `disconnect` / `retain` / `release`）。带回调的函数都接受 `release_fn`，KUI 在不再
  引用 `user_data` 时调用它一次；回调里释放 Observable / Observer 的最后一个引用是安全的。
- `kui_element_get_computed_style_value`：按 CSS 属性名读取计算后的值（等于
  `getComputedStyle(el).getPropertyValue(name)`，与 Lua 同口径，逻辑属性按 `direction` 映射）。
- 弹出层 / 选择器 / 音频 37 个（C 头第 12 节）：提示框与右键菜单（`kui_document_show_tooltip` /
  `show_context_menu`（复用第 10 节的 `KuiCMenuItem`，带 `release_fn`）/ `dismiss_popup` /
  `dismiss_all_popups` / `dismiss_popups_of_type` / `has_popups` / `has_popup_of_type` /
  提示延迟与配色（颜色为 `0xRRGGBBAA`）/ `set_popup_mode`）；`<input>` 选择器
  （`kui_element_open_input_picker` 与 `kui_document_close` / `commit` / `get_input_picker_state` /
  `get_input_picker_target` / `set_input_picker_enabled` / `month` / `color_hsv` / `time`）；游戏音频
  （`kui_document_audio_*` 16 个：可用性、播放、停止、暂停、音量 / 声像 / 音高、跳转、状态、分组音量与
  静音、全部停止 / 暂停、预加载 / 卸载、计数）。
- 媒体与 Lottie 播放控制 9 个（C 头第 13 节）：`kui_media_load` / `set_loop` / `set_autoplay` /
  `get_playback_flags`（loop / autoplay / ended），`kui_lottie_stop` / `seek` / `set_speed` / `set_loop` /
  `get_state`（`KuiCLottieState`：播放、循环、自动播放、速度、0-1 位置、加载状态）。
- 扩展与 DevTools 生命周期 8 个（C 头第 14 节）：`kui_document_load_extension` / `unload_extension` /
  `reload_extension` / `unload_all_extensions` / `watch_extensions`（目录热重载，NULL 停止）/
  `get_extension_state`，`kui_window_stop_devtools` / `get_devtools_state`（运行、端口、连接数；访问 token
  在已有的 `kui_window_get_devtools_url` 里）。
- 拖放数据 9 个（C 头第 15 节）：`kui_event_drag_set_data` / `get_data`（未设置为 `KUI_ERR_NOT_FOUND`）/
  `clear_data`（format 为 NULL 清全部）/ `get_type_count` / `get_type`，`set_drop_effect` / `get_drop_effect`
  （`KuiCDropEffect`）、`set_effect_allowed` / `get_effect_allowed`（`KuiCEffectAllowed`）；不是拖放事件时
  `KUI_ERR_NOT_SUPPORTED`。
- 脚本引擎控制 15 个（C 头第 16 节，值类型 `KuiCScriptValue`，函数句柄 `KuiScriptFunction`）：
  `kui_script_engine_eval_value` / `eval_file` / `call_function` / `get_last_error`（完整错误文本）/
  `register_function` / `set_global` / `get_global` / `set_print_handler` / `set_memory_limit` /
  `set_instruction_limit` / `get_resource_usage`，`kui_script_function_create` / `call` / `retain` / `release`。
  C 函数返回非 `KUI_OK` 时在 Lua 里抛出错误；回调里释放引擎会推迟到回调返回之后。
- 自定义元素自绘、tick 与输入 22 个（C 头第 17 节）：`kui_document_define_custom_element_ex`
  （`KuiCCustomElementDefinition`：生命周期回调之外加 paint / tick / needs_tick / key / mouse / wheel / ime，
  共用一个 user_data 与 release_fn）；绘制 `kui_paint_get_info` / `fill_rect` / `fill_rounded_rect` /
  `stroke_rect` / `stroke_rounded_rect` / `draw_text` / `draw_pixels` / `draw_image` / `draw_decoration_line` /
  `save` / `restore` / `translate` / `scale` / `rotate` / `set_opacity` / `clip_rect` / `clip_rounded_rect`；
  `kui_element_measure_text` / `request_repaint` / `invalidate_rect`；`kui_devtools_dispatch_ime_event`。
  原生的固有尺寸回调布局从不读取，未开放（`T-DEBT-CUSTOM-ELEMENT-INTRINSIC-SIZE-UNUSED`）。
- 组件 65 个（C 头第 18 节，新源文件 `src/kui_c_components.cpp`）：`kui_document_register_builtin_components`；
  `kui_theme_manager_*` 9 个（创建、释放、注册、应用、当前主题、主题名、变量）；`kui_virtual_list_*` 14 个
  （`KuiCVirtualListConfig`：渲染 / 逐行高度 / 焦点 / 激活回调，键盘导航；状态、增删行、滚动、焦点）；
  `kui_list_view_*` 16 个（`KuiCListViewConfig` 的行数据回调与配色，选择、滚动、状态）；`kui_tree_view_*` 25 个
  （节点增改查、展开收起、单选 / 多选、滚动）。组件在自己的回调里释放时推迟到回调返回之后。
- 文档样式配置 2 个（C 头第 19 节）：`kui_document_set_style_profile` / `get_style_profile`（`KuiCStyleProfile`）。
- 冻结清单从 195 个导出追加到 462 个；原有 195 个函数的签名摘要逐字节不变。

### Added

- `KuiCConfig.cmake` 新增 `KuiC_USE_DEBUG_VARIANT`（默认 `ON`，与 C++ SDK 的 `Kui_USE_DEBUG_VARIANT` 同义）：
  设为 `OFF` 时 Debug 配置也链接并部署 Release 版 `kui_c.dll` / `kui.dll`；选中结果在
  `KuiC_DEBUG_VARIANT_SELECTED`。以前 Debug 路径写死为 `bin/debug`、`lib/debug`。
- 包根新增 `THIRD_PARTY_NOTICES.md`、`BUILD_INFO.txt`、覆盖全部文件的 `SHA256SUMS.txt` 和
  `verify_package.py`（在包根运行即可自查包是否完整、`bin/kui.dll` 自报的构建身份是否一致）。
- 包内随带 DevTools MCP（`tools/devtools_mcp`）和与宿主语言无关的文档（`docs/`，与 C++ 包同名文件相同）。

### Fixed

- 包内 `cmake/KuiCConfig.cmake` 由打包脚本按系统代码页读写模板，中文注释被弄坏并吞掉换行，
  `get_target_property(... RUNTIME_OUTPUT_DIRECTORY)` 一行并进了注释（3.61.7 实测）：自带
  `RUNTIME_OUTPUT_DIRECTORY` 的目标因此拿不到逐目标的运行库拷贝。现按 UTF-8 原样生成，并由包级验证逐字节核对。
- 包内不再带上一次发版同步回仓库的 `sdk/c/bin` 里已删除的旧文件，也不再带 `cmake/*.cmake.in` 生成模板。

### 包结构（2026-11-20 补出，版本号不变）

- 源码 C++ Wrapper 从本包拆出，单独成包 `kui-cpp-wrapper-sdk-3.61.8-<平台>.zip`（C++ 的默认推荐，带与本包逐字节相同的运行库）。
  本包只留纯 C：`cpp_wrapper/`、`bin/kui_cpp_*` 与 `kui_cpp_demo` 不再随本包；`PREBUILT_ARTIFACTS.json` 去掉
  `cpp_wrapper_examples` / `wrapper_demo`。`KuiCConfig.cmake` 只在包里有 `cpp_wrapper/` 时定义 `KuiC::Cpp`（新变量
  `KuiC_CPP_WRAPPER_FOUND`），本包里链接 `KuiC::Cpp` 会在配置期报「找不到目标」。C ABI 不变。


---

## [3.61.7] - 2026-11-07

补记：下面是 3.1.0 之后、直至 3.61.7 随产品版本发布的 C ABI 变更。它们发布时一直挂在
`[Unreleased]` 下（本文件当时不在发版提升 CHANGELOG 的清单里），3.61.7 的 C 包里带的正是这一段。

### Added

- `kui_media_audio` is now a real C11 implementation, not merely a C ABI
  exported by C++ code. Its standalone project consumes `KuiC::MediaAPI` and
  retains the exact 26-symbol DLL export surface. The archive carries its source,
  pinned miniaudio header/license and a DLL rebuilt from the final staging tree;
  extracted-package verification rebuilds it again. Editor, PDF and Video keep
  their C++ implementations but can now obtain their protocol headers from
  `KuiC::ExtensionAPI` / `KuiC::MediaAPI`; 3D remains native-SDK-only.
- Formal Windows C SDK archives now include Release binaries built from the
  final staging tree itself: 11 C examples, 189 C++ Wrapper examples, the
  Wrapper demo, pure-C hello extension and pure-C audio plugin. `PREBUILT_ARTIFACTS.json` freezes
  the exact versioned inventory, while extracted-package verification executes
  shipped binaries and rebuilds the sources independently.
- Added `KuiC::ExtensionAPI` and `KuiC::MediaAPI` authoring targets plus the
  canonical protocol headers. The packaged `extensions/hello` example rebuilds
  without linking either `kui.dll` or `kui_c.dll`.
- Added 11 independently buildable pure-C Full Profile examples covering DOM,
  Window, Lua, HTTP, platform services, custom elements, media/Lottie,
  extensions/3D, Canvas/WebGL, PDF and DevTools.
- The Windows C SDK ZIP now carries the complete 189-source Wrapper example
  snapshot and its runtime assets. Extracted-package verification builds all C
  examples in Release/Debug, runs six deterministic examples and builds all 189
  Wrapper targets; a repository-only source reference is no longer sufficient.
- C packaging now pairs `kui_c.dll` with the exact `kui.dll` deployed by the
  same C ABI build configuration instead of taking the core runtime from an
  independently changing native build directory.
- Promoted the Windows x64 package from Core Profile to Full Profile: networking,
  Lua, extension/media, Canvas/WebGL, custom elements, PDF, platform services and
  the standalone 3D protocol are now available through a stable pure-C surface.
- Expanded and froze the product-major-3 ABI from 120 to 195 exports. Header,
  implementation, manifest, normalized signature digest, PE exports, POD layouts
  and enum values are checked together.
- Migrated the remaining 84 specialized native examples. The source C++ Wrapper
  now compiles all 189 native example sources and the release gate launches ten
  representative Full Profile GUI programs.
- Implemented the previously draft Document/Element surface: file loading,
  stylesheet management, `<head>`, selector lists, innerHTML, class helpers,
  element traversal and tree mutation.
- Added the Window/Platform bridge required by source-compatible consumers:
  size/position/state, resize callbacks, close, DevTools startup/URL, F12
  configuration and primary-monitor geometry.
- Implemented all four formerly declaration-only in-process DevTools automation
  functions (mouse/key dispatch, box model and raw command dispatch).
- Expanded the pure-C regression suite from 185 to 227 assertions.
- Frozen the original 120 Windows x64 Core Profile exports for product major 3.
  This historical baseline is now extended by the 195-export Full Profile gate. The gate
  compares the public header, implementations, manifest, normalized function
  signature SHA-256 and exact PE export table, and locks public POD sizes and enum values.
- Added the mandatory Windows package with Release/Debug `kui_c` and core
  binaries, import libraries, source C++ Wrapper, license/version metadata and
  relocatable `find_package(KuiC)` targets. Matching C PDBs enter the symbols package.
- Added extracted-package consumers covering MSVC and clang-cl, C and C++,
  Release and Debug, and `/MD`/`/MDd`/`/MTd` consumer CRTs.

### Changed

- `KUI_C_API_VERSION_*` now follows the repository root product version
  (`3.54.0` at this change) instead of maintaining an independent `3.1.x`
  maturity number. Version governance and the full-release version step keep the
  mirror synchronized.
- The source C++ facade now builds all 189 native example sources through the C
  ABI, including the 84 previously excluded specialized examples.
- Behavior change: `kui_element_get_offset_metrics` now returns whole pixels,
  like browsers (product decision, matching Chrome 153 measured on the same
  machine). `out_left` / `out_top` round the exact offset to the offset parent
  and `out_width` / `out_height` round the exact border-box size, each as
  floor(x + 0.5); a box that wraps across lines reports the bounding box of its
  line fragments. The out-parameters stay `float` and the 195-export surface is
  unchanged; the C++ Wrapper `Element::offset*()`, Go `OffsetMetrics()` and Rust
  `kui_sys` follow automatically. Use `kui_element_get_bounding_rect()` for
  sub-pixel geometry. See the root `CHANGELOG.md`
  (`T-DEBT-OFFSET-METRICS-ROUND-TO-INTEGER`).
- Behavior change: `kui_element_get_client_metrics` (all four values) and the
  `out_width` / `out_height` of `kui_element_get_scroll_metrics` now return whole
  pixels, like browsers (product decision, matching Chrome 153 measured on the
  same machine at DPR 1 / 1.25 / 1.5 / 2): each is floor(x + 0.5) of the exact
  padding box minus a rendered scrollbar, border width, or scrollable extent.
  A non-replaced inline box (`display:inline` `<span>` and the like) reports 0
  for all six values, as browsers do. The scroll offsets (`out_left` /
  `out_top` of `kui_element_get_scroll_metrics`) are not rounded, and scrolling
  itself is unchanged: the maximum scroll offset is still the exact one. The
  out-parameters stay `float` and the 195-export surface is unchanged; the C++
  Wrapper `Element::client*()` / `scrollWidth()` / `scrollHeight()`, Go
  `ClientMetrics()` / `ScrollMetrics()` and Rust `kui_sys` follow automatically.
  Use `kui_element_get_bounding_rect()` for sub-pixel geometry. See the root
  `CHANGELOG.md` (`T-DEBT-CLIENT-SCROLL-METRICS-ROUND-TO-INTEGER`).

### Fixed

- `kui_element_get_offset_metrics` now returns what its header comment always
  promised: `out_left` / `out_top` are the border-box offset relative to the
  offset parent's padding box (CSSOM View, matching Chrome 153), unaffected by
  scrolling and transforms (rounding: see the behavior change under Changed).
  Previously they were the content-box
  position in document coordinates. Signature and the 195-export surface are
  unchanged; the C++ Wrapper `Element::offsetLeft/Top()`, Go `OffsetMetrics()`
  and Rust `kui_sys` follow automatically. See the root `CHANGELOG.md`
  (`T-BUG-CSSOM-OFFSET-LEFT-TOP-PARENT`).

The GA claim is deliberately bounded to the Windows x64 Full Profile with MSVC
and clang-cl consumers. MinGW, Linux and macOS remain deferred.

---

## [3.1.0] - 2026-07-26 - First working implementation

Promotes the 3.1.0-preview design draft to an implemented API. The version now
matches `KUI_C_API_VERSION_*` in `include/kui_c.h`, which the preview tag did not.

### Added

- **Platform and Window — the M2 minimum-viable set is now complete.** A pure C
  program can open a window, load HTML, drive the DOM from a timer and exit
  cleanly; `examples/window_demo.c` does exactly that and is run as an end-to-end
  check (it quits itself from a timeout callback, so it runs unattended).
- **Three places where the draft did not match the library, found while
  implementing this:**
  - The draft declared a `KuiApplication` handle with create/release. **KUI has no
    Application class** — the corresponding surface is the all-static
    `kui::Platform`. Inventing a handle would have meant fabricating an object the
    library does not have, so that section is now handle-free `kui_platform_*`.
  - `kui_window_create` took a `KuiApplication*` that cannot exist.
  - `kui_window_set_document` was unimplementable: a window creates and owns its
    document. The API is `kui_window_get_document`, which yields a counted
    reference that outlives the window.
- **A third handle lifetime model, now documented alongside the other two.**
  Windows are uniquely owned (C++ hands back a `unique_ptr`), so there is no
  retain and `kui_window_release` **destroys** rather than decrements. Compare:
  Document/Element are reference counted; a `KuiEvent*` is borrowed for the
  duration of its callback.
- Measured behaviours written into the header rather than guessed: a created
  window is **already visible** (there is no hidden-until-show step), and
  `KuiWindowProps.no_resize` / `no_decorations` are phrased negatively because the
  library defaults are on and `Window` exposes no setter for either — a
  positively-named field would have made `memset(0)` mean "fixed size, no
  chrome", with no way back.
- **First working implementation — a vertical slice through the whole stack.** The
  header is no longer draft-only: `kui_c.dll` builds, exports the implemented
  functions, and is covered by tests written in C. Implemented so far:
  `kui_get_version`, `kui_status_message`, Document create/retain/release/
  load_html/root/body/get_element_by_id/query_selector/create_element, Element
  retain/release/get_tag_name/get_id/attributes (get/set/has/remove)/text_content
  (get/set), the whole Events category (listener add/remove/remove-all, synthetic
  dispatch, event type/target/currentTarget/flags, preventDefault,
  stopPropagation, mouse client/page/offset coordinates and button, keyboard
  key/code/modifiers), inline style (property get/set/remove, !important query,
  whole-declaration cssText read/write, property enumeration) and geometry
  (bounding rect plus client/offset/scroll metrics). Everything else in the header
  stays declared but unimplemented, and is now marked `[DRAFT]` per section so it
  is clear what links and what does not.
- `examples/hello_world.c` — the ROADMAP M2 exit criterion, in plain C, headless.
- Inline style values are returned **serialized**, which is not always the string
  that was set: `color: red` reads back as `#ff0000` (measured, and asserted in
  the tests so a change in serialization is caught). Documented in the header.
- The README's "Design Sketch" was replaced: its example used the draft API
  (`KuiApplication`, `kui_window_set_document`) and therefore could not compile —
  worse than having no example. It is now the real call sequence, matching
  `examples/window_demo.c`.
- `tests/test_kui_c_abi.c` — 185 assertions, written in C rather than C++ so that
  it also proves the header is C-clean and usable with no C++ runtime present.
  Covers NULL arguments, absent-vs-empty distinction, the two-call string
  pattern (length query / too-small buffer / exact fit), UTF-8 byte fidelity,
  retain/release balance, and that a handle stays valid after its element leaves
  the document.
- `CMakeLists.txt` — builds in tree against the `kui` target, or standalone
  against a packaged SDK via `find_package(Kui)`.

### Changed

- **`KuiMouseEventType`, `KuiMouseButton` and `KuiKeyEventType` renamed to
  `KuiCMouseEventType`, `KuiCMouseButton`, `KuiCKeyEventType`** (enumerators to
  `KUI_C_*`). The draft names collided with the extension ABI in
  `<kui/extension/kui_extension_api.h>` (v11, shipped, used by the editor/PDF/3D
  extensions) — with *different values for the same names*: button 0 is LEFT
  there and NONE here. Same name with different meaning is the one collision that
  must never ship, and the extension ABI cannot move.
- `KuiWindowProps.title_len` is now `size_t` with `0 = strlen`, matching every
  other string input in the header. It was `int` with `-1 = strlen`.
- Added `KUI_ERR_BUFFER_TOO_SMALL`, which the two-call output pattern requires
  and the draft never defined, despite referring to it in comments.
- Added the `KUI_C_API` export macro. Without it, nothing linked on Windows.
- Handle lifetime is now specified and implemented: handles are reference
  counted, every function returning a handle gives the caller a reference, and
  `kui_document_retain` / `kui_element_retain` / `kui_element_release` were added
  (the draft had no way to release an element at all).

---

## [3.1.0-preview] - 2026-05-23 - Design draft published

### Added

- Initial design draft for the KUI C ABI SDK:
  - `README.md` with status, motivation, design principles
  - `ROADMAP.md` with three-milestone implementation plan (M1 ABI freeze / M2 skeleton / M3 GA)
  - `include/kui_c.h` - C header draft showcasing the future API surface

### Notes

This is a **design preview**, not a working SDK. The header file is intended as a discussion artifact and reference for the implementation in a future minor release.

For working KUI in v3.1.0, use the C++ SDK at `../cpp/`.
