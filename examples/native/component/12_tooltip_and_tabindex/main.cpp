// ===========================================================================
// 12_tooltip_and_tabindex — Desktop interaction completeness.
//
//   KUI-027  title-attribute tooltip + tabindex keyboard focus ring + events
//   SDK 3.11.0
//
// Two engine-native capabilities (no extra API calls required):
//
//  1) title tooltip — fully automatic.  Any element with a `title` attribute
//     pops a native tooltip after a hover delay; the engine walks ancestors to
//     find a title.  Source:
//       src/event/event_dispatcher.cpp:631-632  (onElementHovered each move)
//       src/popup/popup_manager.cpp:421-507      (read title, delay, showTooltip)
//     -> pure HTML <div title="..."> is enough.
//
//  2) tabindex focus ring — Tab / Shift+Tab walk the focus order honoring
//     tabindex (>0 grouped ascending, 0/absent in document order, <0 skipped).
//       src/event/event_dispatcher.cpp:1324-1332 (Tab -> advanceFocus)
//       src/event/event_dispatcher.cpp:1911-1996 (advanceFocus / tabindex order)
//     Focus ring via CSS :focus { outline... }.  NOTE: native form controls
//     (input/button/select) get UA `outline:none` (border-color instead), so we
//     demo the ring on <div tabindex> elements where outline is visible.
//       src/css/css_selector.cpp:273-280  (:focus / :focus-visible)
//       src/css/ua_stylesheet.cpp:298-308 (UA focus ring + form override)
//
// Element-level focus/blur/focusin/focusout + keydown are dispatched through
// the normal DOM path and can be observed with addEventListener.
//   src/event/event_dispatcher.cpp:1760-1795 (handleFocus dispatch order)
// ===========================================================================

#include <kui/kui.h>
#include <kui/event/event.h>   // KeyboardEvent / EventTypeTag
#include <string>
#include <vector>

int main() {
    kui::Platform::init();
    auto window = kui::Window::create({
        .title  = "KUI — Tooltip & Tabindex (KUI-027)",
        .width  = 780,
        .height = 580,
    });

    auto* doc = window->document();
    doc->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI","Microsoft YaHei",sans-serif;background:#f8fafc;color:#1e293b;padding:24px}
h2{font-size:17px;margin-bottom:4px}
h3{font-size:13px;color:#64748b;margin:18px 0 10px}
.hint{font-size:12px;color:#94a3b8;margin-bottom:6px}
.card{background:#fff;border-radius:12px;padding:18px;box-shadow:0 1px 3px rgba(0,0,0,.1)}
.row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}
.tbtn{padding:9px 16px;border:none;border-radius:8px;background:#3b82f6;color:#fff;font-size:13px;font-weight:600;cursor:pointer}
.tbtn.g{background:#10b981}.tbtn.a{background:#f59e0b;color:#1e293b}
/* tabindex 焦点环演示:用 <div> 才能看到 outline(原生控件会被 UA 改成 border) */
.card[tabindex],.fcard{outline:none;cursor:pointer}
.fcard{padding:14px 18px;border:1px solid #cbd5e1;border-radius:8px;background:#fff;font-size:13px;min-width:150px}
.fcard:focus{outline:3px solid #3b82f6;outline-offset:2px;background:#eff6ff;border-color:#3b82f6}
#log{margin-top:14px;background:#0f172a;color:#cbd5e1;border-radius:6px;padding:10px 12px;font:12px "Cascadia Code","Consolas",monospace;height:150px;overflow:auto;white-space:pre-wrap}
.tag{color:#64748b}
</style></head><body>
<h2>桌面交互完整性 (KUI-027)</h2>

<div class="card">
  <h3>1. title 属性 Tooltip(纯 HTML,hover 约 600ms 自动弹出)</h3>
  <p class="hint">把鼠标悬停在按钮上,稍候出现原生 tooltip。</p>
  <div class="row">
    <button class="tbtn"   title="新建文件 (Ctrl+N)">新建</button>
    <button class="tbtn g" title="打开文件夹 (Ctrl+O)">打开</button>
    <button class="tbtn a" title="保存当前更改 (Ctrl+S)">保存</button>
    <span class="tag" title="这是一个图标的悬停说明,文案取自 title 属性的纯文本">[i] 图标(也有 title)</span>
  </div>
</div>

<div class="card" style="margin-top:16px;">
  <h3>2. tabindex 焦点环 + Tab 键遍历</h3>
  <p class="hint">点击页面后按 Tab / Shift+Tab。遍历顺序:tabindex=1 → 2 → 3 → 0 组(文档序)。蓝色 outline 即焦点环。</p>
  <div class="row">
    <div class="fcard" tabindex="1" data-name="卡片 A (tabindex=1)">卡片 A · tabindex=1</div>
    <div class="fcard" tabindex="3" data-name="卡片 C (tabindex=3)">卡片 C · tabindex=3</div>
    <div class="fcard" tabindex="2" data-name="卡片 B (tabindex=2)">卡片 B · tabindex=2</div>
    <div class="fcard" tabindex="0" data-name="卡片 D (tabindex=0)">卡片 D · tabindex=0</div>
  </div>
  <div id="log">点击窗口后按 Tab 试试…</div>
</div>
</body></html>
    )HTML");

    auto* log = doc->getElementById("log");
    auto logLine = [log](const std::string& s) {
        log->setTextContent(s + "\n" + log->textContent());
    };

    // 元素级 focus/blur 监听:为每张 tabindex 卡片挂处理器。
    // querySelectorAll 返回 RefPtr<NodeList>,元素是 Node*,需向下转型为 Element。
    auto fcards = doc->querySelectorAll(".fcard");
#ifdef KUI_CPP_WRAPPER
    for (const auto& node : fcards) {
#else
    for (const auto& node : *fcards) {
#endif
        if (!node->isElement()) continue;
        auto* el = static_cast<kui::Element*>(node.get());
        std::string name = el->getAttribute("data-name");
        el->addEventListener("focus", [logLine, name](kui::Event&) {
            logLine("[focus] " + name);
        });
        el->addEventListener("blur", [logLine, name](kui::Event&) {
            logLine("[blur]  " + name);
        });
    }

    // 监听 Tab keydown(引擎随后做 advanceFocus,这里仅记录)。
    doc->documentElement()->addEventListener("keydown", [logLine](kui::Event& e) {
        if (e.typeTag() != kui::EventTypeTag::Keyboard) return;
        auto& ke = static_cast<kui::KeyboardEvent&>(e);
        if (ke.key() == "Tab") {
            logLine(ke.shiftKey() ? "[keydown] Shift+Tab -> focus moves back"
                                  : "[keydown] Tab -> focus advances");
        }
    });

    return kui::Platform::run();
}
