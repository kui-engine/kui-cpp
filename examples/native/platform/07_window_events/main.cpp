// ===========================================================================
// 07_window_events — Window-level focus/blur + move/drag-region events.
//
//   KUI-013  window focus / blur / focusin / focusout / close events
//   KUI-015  window movestart / move / moveend + dragregionpointerdown
//   SDK 3.13.0
//
// All events are dispatched by the Win32 platform layer and delivered through
// Window::addEventListener(type, cb)  (the window is its own event target — it
// does NOT go through the DOM / EventDispatcher).  See:
//   include/kui/platform/window.h:137-150  (window event API + type list)
//   src/platform/window_win32.cpp:577-637  (WM_SIZE/WM_MOVE/WM_CLOSE -> dispatch)
//   src/platform/window_win32.cpp:612-632  (WM_NCLBUTTONDOWN / WM_MOVE)
//
// Drag region is declared with the CSS property `-kui-app-region: drag`
// (alias `-webkit-app-region: drag`); it only takes effect on a *frameless*
// window — see src/platform/window_win32.cpp:452,522-525 and the CSS registry
// at src/css/css_property_registry.cpp:409-410.  This demo therefore creates a
// frameless window with a custom HTML title bar.
//
// NOTE (KUI-016 真机复核坑): `movestart` only fires once the drag crosses the
// OS drag threshold (~4px); a bare click on the title bar does NOT fire it.
// "title-bar pressed" is `dragregionpointerdown`.  Do not conflate the two.
// ===========================================================================

#include <kui/kui.h>
#include <kui/event/event.h>
#include <string>

int main() {
    kui::Platform::init();

    // app-region 仅在 frameless 窗口生效,故用无边框窗口 + 自绘标题栏。
    kui::WindowConfig cfg;
    cfg.title     = "KUI — Window Events (KUI-013/015)";
    cfg.width     = 760;
    cfg.height    = 540;
    cfg.frameless = true;   // 必须:启用 -kui-app-region 命中测试
    cfg.shadow    = true;
    cfg.sizeable  = true;
    auto window = kui::Window::create(cfg);

    auto* doc = window->document();
    doc->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI","Microsoft YaHei",sans-serif;background:#0f172a;color:#e2e8f0;height:100vh;display:flex;flex-direction:column}
/* 自绘标题栏:整条声明为可拖拽区(拖动它移动窗口) */
.titlebar{-kui-app-region:drag;-webkit-app-region:drag;height:40px;background:#1e293b;border-bottom:1px solid #334155;display:flex;align-items:center;padding:0 12px;gap:8px;flex-shrink:0}
.titlebar .title{font-size:13px;font-weight:600;color:#cbd5e1;flex:1}
/* 按钮区设为 no-drag,否则点不到 */
.win-btn{-kui-app-region:no-drag;-webkit-app-region:no-drag;width:28px;height:24px;border:none;border-radius:4px;background:#334155;color:#e2e8f0;font-size:13px;cursor:pointer}
.win-btn:hover{background:#475569}
.win-btn.close:hover{background:#ef4444}
.content{flex:1;padding:20px;overflow:auto}
h2{font-size:16px;margin-bottom:6px}
.hint{font-size:12px;color:#94a3b8;margin-bottom:14px;line-height:1.6}
.badges{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:14px}
.badge{padding:4px 10px;border-radius:999px;font-size:12px;background:#1e293b;border:1px solid #334155}
.badge.active{background:#1e3a8a;border-color:#2563eb;color:#fff}
#log{background:#020617;border:1px solid #334155;border-radius:6px;padding:10px 12px;font:12px "Cascadia Code","Consolas",monospace;color:#cbd5e1;height:280px;overflow-y:auto;white-space:pre-wrap}
.t{color:#64748b}
</style></head><body>
<div class="titlebar">
  <span class="title">KUI Frameless — 拖动此标题栏移动窗口</span>
  <button class="win-btn" id="btn-min" title="最小化">_</button>
  <button class="win-btn close" id="btn-close" title="关闭">x</button>
</div>
<div class="content">
  <h2>窗口级事件演示 (KUI-013 / KUI-015)</h2>
  <p class="hint">
    • 点击其它窗口让本窗口失活 → <b>blur</b>;点回本窗口 → <b>focus</b>。<br>
    • 按住标题栏拖动窗口 → <b>movestart → move… → moveend</b>。<br>
    • 仅按下标题栏(不拖动) → <b>dragregionpointerdown</b>(注意:纯点击不会触发 movestart)。
  </p>
  <div class="badges">
    <span class="badge" id="b-focus">app: focus</span>
    <span class="badge" id="b-move">move: idle</span>
  </div>
  <div id="log"></div>
</div>
</body></html>
    )HTML");

    auto* log = doc->getElementById("log");
    auto* bFocus = doc->getElementById("b-focus");
    auto* bMove  = doc->getElementById("b-move");

    auto logLine = [log](const std::string& s) {
        // 新行追加在前面,最近事件置顶。
        log->setTextContent(s + "\n" + log->textContent());
    };

    // --- KUI-013: 应用激活态 focus/blur(整窗失活才触发,子弹窗不误触发)---
    window->addEventListener("focus", [logLine, bFocus](kui::Event&) {
        bFocus->setTextContent("app: focus");
        bFocus->removeClass("active");
        bFocus->addClass("active");
        logLine("[focus]  application activated");
    });
    window->addEventListener("blur", [logLine, bFocus](kui::Event&) {
        bFocus->setTextContent("app: blur");
        bFocus->removeClass("active");
        logLine("[blur]   application deactivated");
    });

    // --- KUI-013: 键盘焦点 focusin/focusout(区别于应用激活态)---
    window->addEventListener("focusin",  [logLine](kui::Event&) {
        logLine("[focusin]  keyboard focus gained");
    });
    window->addEventListener("focusout", [logLine](kui::Event&) {
        logLine("[focusout] keyboard focus lost");
    });

    // --- KUI-015: 窗口拖动 movestart/move/moveend ---
    window->addEventListener("movestart", [logLine, bMove](kui::Event&) {
        bMove->setTextContent("move: dragging");
        logLine("[movestart] window drag begin (passed OS drag threshold)");
    });
    window->addEventListener("move", [logLine, bMove](kui::Event&) {
        bMove->setTextContent("move: moving");
        // move 在拖动中连续触发,徽章足够,不刷日志以免刷屏。
    });
    window->addEventListener("moveend", [logLine, bMove](kui::Event&) {
        bMove->setTextContent("move: idle");
        logLine("[moveend]  window drag end");
    });

    // --- KUI-015: 按下标题栏 drag 区(按下即广播,无需越过拖拽阈值)---
    window->addEventListener("dragregionpointerdown", [logLine](kui::Event&) {
        logLine("[dragregionpointerdown] title-bar pressed");
    });

    // --- KUI-013: close 可取消(cancelable)---
    // 自绘窗口的 X 走 window->close();OS 关闭请求也会派发 close 事件。
    window->addEventListener("close", [logLine](kui::Event& e) {
        logLine("[close]  close requested (allowed)");
        (void)e; // 此处不取消;如需阻止关闭可 e.preventDefault()
    });

    // 自绘窗口按钮:最小化 / 关闭(no-drag 区,可点击)。
    doc->getElementById("btn-min")->addEventListener("click", [&window](kui::Event&) {
        window->minimize();
    });
    doc->getElementById("btn-close")->addEventListener("click", [&window](kui::Event&) {
        window->close();
    });

    logLine("ready — try dragging the title bar / switching windows");

    return kui::Platform::run();
}
