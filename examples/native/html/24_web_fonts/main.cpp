// ===========================================================================
// 24_web_fonts — CSS 字体子系统 (KUI-038, SDK 3.15.0).
//
// 演示 KUI 对 CSS @font-face 与 CSS Font Loading API 的实用子集:
//   * @font-face { src: local(...) } —— 复用系统已装字体(零文件 IO),
//     未装则 fallback 到 url()。本 demo 用系统常见字体 Consolas / Segoe UI
//     / Georgia 注册不同 family。
//   * document.fonts.check / load / ready —— CSS Font Loading API 子集。
//   * "loadingdone" 事件 —— @font-face 加载完成时派发(KUI 同步加载,即时触发)。
//   * font-display / unicode-range —— descriptor 已解析存储(诚实标注:当前
//     因 KUI 同步加载模型 / TypefaceFontProvider 按 family 注册,这两者尚不
//     产生可观察渲染差异;详见 sdk/cpp/docs/css-fonts.md)。
//
// 真实 API 出处:
//   * @font-face local() / font-display / unicode-range 解析  src/css/style_resolver.cpp
//   * @font-face 消费(local→url fallback)+ loadingdone 派发  src/dom/document.cpp
//   * document.fonts.check/load/ready (Lua, 点号语法)         src/script/lua_dom_bindings.cpp:1789
//   * RenderBackend::isFontFamilyRegistered (backs check)      include/kui/render/render_backend.h
//   * ScriptEngine 装配                                        examples/native/extension/15_*/main.cpp
//
// 注意:document.fonts.* 在 Lua 侧是 sol2 set_function(无 self 参数),
//       故调用用**点号** document.fonts.check("X"),不是冒号(与 css-fonts.md 一致)。
//
// headless 无法验证字体光栅化与事件流,待真机复核(见 work_log_KUI-037-038)。
// ===========================================================================

#include <kui/kui.h>
#ifdef KUI_HAS_SCRIPTING
#include <kui/script/script_engine.h>
#endif

#include <cstdio>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — CSS Web Fonts (@font-face / document.fonts)",
        .width  = 880,
        .height = 720,
    });

    auto* doc = window->document();

    doc->loadHTML(R"HTML(<!DOCTYPE html>
<html><head><style>
/* @font-face src: local(...) —— 复用系统已装字体,免文件 IO。
   多源时按 CSS 标准 fallback:先 local() 命中系统字体,未装再走 url()。 */
@font-face {
  font-family: "DemoMono";
  src: local("Consolas"), local("Courier New");
  font-display: swap;          /* descriptor 已解析存储(当前不改变渲染) */
}
@font-face {
  font-family: "DemoUI";
  src: local("Segoe UI"), local("Arial");
}
@font-face {
  font-family: "DemoSerif";
  src: local("Georgia"), local("Times New Roman");
  unicode-range: U+0000-00FF;  /* descriptor 已解析存储(当前不做码点级选择) */
}

* { margin: 0; padding: 0; box-sizing: border-box; }
body { padding: 28px; background: #0f172a; color: #e2e8f0;
       font-family: "DemoUI", "Segoe UI", sans-serif; }
.tag { display:inline-block; padding:4px 12px; border-radius:12px;
       background:#1e3a8a; color:#bfdbfe; font-size:12px; font-weight:600;
       letter-spacing:.5px; margin-bottom:14px; }
h1 { font-size: 28px; margin-bottom: 6px; }
.sub { color:#94a3b8; font-size:14px; margin-bottom:22px; }

.specimen { background:#1e293b; border:1px solid #334155; border-radius:12px;
            padding:18px 22px; margin-bottom:16px; }
.specimen .lbl { font-size:12px; color:#93c5fd; text-transform:uppercase;
                 letter-spacing:1px; margin-bottom:8px; }
.s-mono  { font-family:"DemoMono", monospace; font-size:22px; }
.s-ui    { font-family:"DemoUI", sans-serif;  font-size:22px; }
.s-serif { font-family:"DemoSerif", serif;    font-size:22px; }

#status { background:#020617; border:1px solid #334155; border-radius:10px;
          padding:14px 18px; margin-top:18px;
          font-family:"DemoMono","Cascadia Code",Consolas,monospace;
          font-size:13px; color:#86efac; white-space:pre-wrap; line-height:1.7; }
.note { margin-top:16px; color:#64748b; font-size:12px; line-height:1.6; }
code { background:#1e293b; padding:1px 6px; border-radius:4px; color:#fcd34d;
       font-family:"DemoMono",Consolas,monospace; }
</style></head>
<body>
<span class="tag">KUI-038 · SDK 3.15.0 · CSS 字体子系统</span>
<h1>@font-face <code>src: local()</code> + document.fonts</h1>
<p class="sub">三个 @font-face 用 <code>local()</code> 复用系统字体(免文件 IO),
   下方用 Lua 调用 CSS Font Loading API 子集并监听 loadingdone。</p>

<div class="specimen">
  <div class="lbl">DemoMono — local(Consolas)</div>
  <div class="s-mono">The quick brown fox 0123456789 {[()]}</div>
</div>
<div class="specimen">
  <div class="lbl">DemoUI — local(Segoe UI)</div>
  <div class="s-ui">The quick brown fox jumps over the lazy dog</div>
</div>
<div class="specimen">
  <div class="lbl">DemoSerif — local(Georgia)</div>
  <div class="s-serif">The quick brown fox jumps over the lazy dog</div>
</div>

<div id="status">document.fonts: 等待脚本执行...</div>

<p class="note">
  诚实标注:<code>font-display</code> 与 <code>unicode-range</code> descriptor 已解析存储,
  但因 KUI @font-face 为同步加载(无 FOUT/FOIT)、TypefaceFontProvider 按 family 注册,
  二者当前不产生可观察渲染差异。详见 <code>sdk/cpp/docs/css-fonts.md</code>。
</p>

<script>
-- CSS Font Loading API 子集(KUI-038)。@font-face 同步加载,故 check 立即反映
-- 注册状态、load/ready 立即就绪、loadingdone 在加载完成后即时派发。
-- 注意:document.fonts.* 是无 self 的函数,用点号调用(非冒号)。
local statusEl = document:getElementById("status")
local lines = {}
local function line(s) lines[#lines + 1] = s; statusEl.textContent = table.concat(lines, "\n") end

line("[document.fonts] CSS Font Loading API 子集探测:")

-- check(family):该字体是否已注册(@font-face 加载完成)。
for _, fam in ipairs({ "DemoMono", "DemoUI", "DemoSerif", "NotInstalledFont" }) do
    local ok = document.fonts.check(fam)
    line(string.format("  check(%-18s) = %s", '"' .. fam .. '"', tostring(ok)))
end

-- load(family, cb):主动加载 + 回调(同步立即触发)。
document.fonts.load("DemoMono", function(ok)
    line("  load(\"DemoMono\") 回调: ok = " .. tostring(ok))
end)

-- ready(cb):所有字体就绪回调(同步立即触发)。
document.fonts.ready(function()
    line("  ready() 回调: 全部字体就绪")
end)

-- loadingdone:@font-face 加载完成事件(document 级)。
document:addEventListener("loadingdone", function(e)
    line("  事件 loadingdone 已派发")
end)

print("[web-fonts demo] document.fonts 探测完成")
</script>
</body></html>
    )HTML");

#ifdef KUI_HAS_SCRIPTING
    auto scriptEngine = kui::ScriptEngine::create();
    if (scriptEngine) {
        scriptEngine->setErrorHandler([](const kui::String& err,
                                         const kui::String& src, int line) {
            std::fprintf(stderr, "[Lua Error] %s:%d  %s\n",
                         src.c_str(), line, err.c_str());
        });
        scriptEngine->bindDocument(doc);
        scriptEngine->executeScripts();
        window->setScriptEngine(scriptEngine.get());
    } else {
        std::fprintf(stderr, "[web-fonts demo] ScriptEngine::create() failed;"
                             " document.fonts 探测不会运行。\n");
    }
#else
    std::fprintf(stderr, "[web-fonts demo] KUI_HAS_SCRIPTING 未启用,"
                         " document.fonts 探测不会运行(@font-face 渲染仍生效)。\n");
#endif

    return kui::Platform::run();
}
