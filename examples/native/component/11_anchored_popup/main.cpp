// ===========================================================================
// 11_anchored_popup — Imperative anchored positioning (popover-style).
//
//   KUI-010  anchored popup positioning API
//   SDK 3.13.0
//
// KUI-010 ships NOT a "Popup class" but a one-shot DOM method that anchors an
// ordinary element (your floating panel) to an anchor element, resolving
// viewport collisions (flip + clamp) like CSS Anchor Positioning / the Popover
// API — without the full CSS surface:
//
//   Element::positionRelativeTo(const Element* anchor,
//                               const String& placement,
//                               const PositioningOptions& opts = {});
//
//   placement = "<side>" or "<side>-<align>"
//     side  : top | bottom | left | right   (default bottom)
//     align : start | center | end          (default center, cross axis)
//   PositioningOptions { flip=true, clampToViewport=true, offset=0, viewportMargin=8 }
//
// Source references:
//   include/kui/dom/element.h:246-274   (API + contract)
//   src/dom/element.cpp:1352-1459        (implementation; forces a sync reflow)
//
// The floating element should be position:fixed (left/top become viewport
// coordinates).  positionRelativeTo forces a synchronous reflow first, so a
// freshly shown popup positions correctly in the same tick.  It is a no-op
// while the popup is display:none (offsetWidth/Height == 0) — show it first,
// then call positionRelativeTo.
// ===========================================================================

#include <kui/kui.h>
#include <kui/event/event.h>
#include <string>

int main() {
    kui::Platform::init();
    auto window = kui::Window::create({
        .title  = "KUI — Anchored Popup (KUI-010)",
        .width  = 760,
        .height = 560,
    });

    auto* doc = window->document();
    doc->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI","Microsoft YaHei",sans-serif;background:#f1f5f9;color:#1e293b;padding:24px;height:100vh}
h2{font-size:17px;margin-bottom:6px}
.hint{font-size:13px;color:#64748b;margin-bottom:20px}
.grid{display:flex;flex-wrap:wrap;gap:14px;max-width:560px}
.anchor{padding:10px 18px;border:none;border-radius:8px;background:#3b82f6;color:#fff;font-size:13px;font-weight:600;cursor:pointer}
.anchor:hover{background:#2563eb}
.spacer{height:160px}
.corner{position:fixed;bottom:24px;right:24px}
/* 浮层:position:fixed,初始隐藏。positionRelativeTo 会写内联 left/top。 */
.popup{position:fixed;display:none;z-index:50;background:#0f172a;color:#e2e8f0;padding:12px 16px;border-radius:10px;box-shadow:0 8px 24px rgba(0,0,0,.25);font-size:13px;max-width:240px;line-height:1.5}
.popup b{color:#93c5fd}
</style></head><body>
<h2>锚定浮层定位 (KUI-010)</h2>
<p class="hint">点击任意按钮,在其旁弹出锚定浮层;越界自动翻转(flip)并夹紧到视口内(clamp)。再次点击同一按钮关闭。</p>
<div class="grid">
  <button class="anchor" id="a-bottom"   data-place="bottom-start">bottom-start ↓</button>
  <button class="anchor" id="a-top"      data-place="top">top ↑</button>
  <button class="anchor" id="a-right"    data-place="right-start">right-start →</button>
  <button class="anchor" id="a-left"     data-place="left">left ←</button>
</div>
<div class="spacer"></div>
<button class="anchor corner" id="a-corner" data-place="bottom-end">右下角按钮(测试 flip/clamp)</button>

<div class="popup" id="popup">
  <b>锚定浮层</b><br>placement: <span id="pp-place">-</span><br>
  通过 <code>positionRelativeTo</code> 一次性定位。
</div>
</body></html>
    )HTML");

    auto* popup    = doc->getElementById("popup");
    auto* ppPlace  = doc->getElementById("pp-place");

    // 当前打开的锚点 id,用于再次点击同一按钮时切换关闭。
    auto openAnchorId = std::make_shared<std::string>("");

    auto hidePopup = [popup, openAnchorId]() {
        popup->style()->setProperty("display", "none");
        *openAnchorId = "";
    };

    auto wireAnchor = [&](const char* id) {
        auto* btn = doc->getElementById(id);
        if (!btn) return;
        std::string anchorId = id;
        btn->addEventListener("click",
            [popup, ppPlace, btn, anchorId, openAnchorId, hidePopup](kui::Event&) {
                // 再次点同一按钮 → 关闭。
                if (*openAnchorId == anchorId) { hidePopup(); return; }

                const std::string place = btn->getAttribute("data-place");
                ppPlace->setTextContent(place);

                // 先显示(否则 offsetWidth/Height==0,positionRelativeTo 为 no-op)。
                popup->style()->setProperty("display", "block");

                kui::Element::PositioningOptions opts;  // flip=true, clamp=true 默认
                opts.offset = 6.0f;
                popup->positionRelativeTo(btn, place, opts);

                *openAnchorId = anchorId;
            });
    };

    wireAnchor("a-bottom");
    wireAnchor("a-top");
    wireAnchor("a-right");
    wireAnchor("a-left");
    wireAnchor("a-corner");

    // 点击空白处(documentElement,且非锚点/浮层)关闭浮层。
    doc->documentElement()->addEventListener("click",
        [popup, openAnchorId, hidePopup](kui::Event& e) {
            if (openAnchorId->empty()) return;
            auto* t = e.target();
            // 命中锚点按钮或浮层内部时不关闭(按钮自身的处理器已处理)。
            for (kui::Node* n = t; n; n = n->parentNode()) {
                if (n == popup) return;
                if (auto* el = n->isElement() ? static_cast<kui::Element*>(n) : nullptr) {
                    if (el->hasClass("anchor")) return;
                }
            }
            hidePopup();
        });

    return kui::Platform::run();
}
