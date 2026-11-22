// ===========================================================================
// 13_kui_editor_m3_demo — Showcase for M3 features.
//
// Demonstrates each M3 milestone in one app:
//   * M3-01 Git gutter        -- inject a hand-crafted diff JSON
//   * M3-02 Inline blame      -- inject blame records, toggle modes
//   * M3-03 Sticky scroll     -- on/off toggle + max-depth slider
//   * M3-04 BPC + Indent guides -- on/off toggle
//   * M3-05 Breadcrumb        -- live "scope path" at caret
//   * M3-06 Snippet           -- expand `for-loop` / `fn` templates
//
// The example is host-driven: no LSP server required.  All test data is
// synthesised in Lua so users can interact with each feature without
// any external setup.
// ===========================================================================

#include <kui/kui.h>
#include <kui/extension/extension_manager.h>
#ifdef KUI_HAS_SCRIPTING
#include <kui/script/script_engine.h>
#endif

#include "../_find_extensions_dir.h"

int main() {
    kui::Platform::init();
    // Bug C fix: F12 launches DevTools (was disabled by default).
    kui::Platform::setF12DevToolsEnabled(true);

    auto window = kui::Window::create({
        .title  = "KUI — kui_ext_editor M3 features demo",
        .width  = 1500,
        .height = 920,
    });

    auto* doc = window->document();
    // Robust across launch modes -- see `../_find_extensions_dir.h`.
    const std::string extDir = kui_examples_internal::findExtensionsDir();
    fprintf(stderr, "[kui-editor M3 demo] loading extensions from: %s\n",
            extDir.c_str());
    doc->initExtensions(extDir);

    doc->loadHTML(R"HTML(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    padding: 16px;
    font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
    background: #0f172a;
    color: #e2e8f0;
    min-height: 100vh;
}
header { display: flex; align-items: center; justify-content: space-between;
         margin-bottom: 12px; }
h1     { font-size: 18px; }
.toolbar { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 10px; }
.toolbar button {
    padding: 6px 12px;
    border-radius: 4px;
    border: 1px solid #334155;
    background: #1e293b;
    color: #e2e8f0;
    font: 13px "Segoe UI";
    cursor: pointer;
}
.toolbar button:hover { background: #334155; }
.toolbar button.primary { background: #1e3a8a; border-color: #2563eb; }
.toolbar button.primary:hover { background: #2563eb; }
.toolbar button.danger  { background: #7f1d1d; border-color: #b91c1c; }
.toolbar button.danger:hover  { background: #b91c1c; }
.toolbar select { padding: 6px 8px; border-radius: 4px;
                  border: 1px solid #334155; background: #1e293b; color: #e2e8f0; }
.toolbar input { padding: 6px 10px; border-radius: 4px; border: 1px solid #334155;
                  background: #1e293b; color: #e2e8f0; width: 260px; }
.editor-wrap { display: flex; height: 600px; }
kui-editor    { flex: 1; height: 100%;
                background: #1e293b; border: 1px solid #334155;
                font-family: "Cascadia Code", "Consolas", monospace; }
.statusbar {
    margin-top: 8px; padding: 6px 12px;
    background: #1e293b;
    border: 1px solid #334155;
    border-radius: 4px;
    font: 12px "Cascadia Code", monospace;
    display: flex; gap: 24px;
}
.statusbar span.label { color: #94a3b8; }
#crumb { color: #38bdf8; }
#snip-state { color: #fbbf24; }
#log {
    margin-top: 8px;
    padding: 8px 12px;
    background: #020617;
    border: 1px solid #334155;
    border-radius: 4px;
    max-height: 140px;
    overflow-y: auto;
    font: 11px "Cascadia Code", monospace;
    color: #cbd5e1;
    white-space: pre;
}
</style></head>
<body>
<header>
    <h1>kui-editor — M3 features showcase</h1>
    <small style="color:#94a3b8;">v2.20.0 • git gutter • blame • sticky • BPC • indent guides • breadcrumb • snippets</small>
</header>

<div class="toolbar">
    <button id="btn-inject-git" class="primary">Inject git diff</button>
    <button id="btn-inject-blame">Inject blame</button>
    <select id="sel-blame-mode">
        <option value="off">blame: off</option>
        <option value="caret" selected>blame: caret line</option>
        <option value="all">blame: all lines</option>
    </select>
    <button id="btn-toggle-sticky">Toggle sticky</button>
    <button id="btn-toggle-bpc">Toggle BPC</button>
    <button id="btn-toggle-guides">Toggle indent guides</button>
    <button id="btn-snippet-fn">Snippet: fn</button>
    <button id="btn-snippet-for">Snippet: for</button>
    <button id="btn-snippet-next">Tab → next stop</button>
    <button id="btn-snippet-cancel" class="danger">Cancel snippet</button>
</div>

<div class="editor-wrap">
    <kui-editor id="ed" line-numbers="true"></kui-editor>
</div>

<div class="statusbar">
    <span><span class="label">scope:</span> <span id="crumb">(top-level)</span></span>
    <span><span class="label">snippet:</span> <span id="snip-state">inactive</span></span>
    <span><span class="label">caret:</span> <span id="caret">0:0</span></span>
    <span><span class="label">version:</span> <span id="ver">0</span></span>
</div>
<div id="log"></div>

<script>
local sampleCode = [[
// M3 features demo.  Try the toolbar buttons:
//   Inject git diff  -> green/blue/red gutter strips
//   Inject blame     -> grey author/date at line end
//   Toggle sticky    -> parent scope pinned on scroll
//   Toggle BPC       -> rainbow brackets
//   Snippet: fn      -> expand `function ${1:name}(${2:args}) { $0 }`
class Demo {
    constructor(config) {
        this.config = config;
        this.entries = [];
    }
    addEntry(name, value) {
        if (!name) {
            throw new Error("name required");
        }
        this.entries.push({ name: name, value: value });
    }
    sum() {
        let total = 0;
        for (let i = 0; i < this.entries.length; i++) {
            total += this.entries[i].value;
        }
        return total;
    }
}

function main() {
    const demo = new Demo({ verbose: true });
    demo.addEntry("alpha", 1);
    demo.addEntry("beta",  2);
    demo.addEntry("gamma", 3);
    console.log("total:", demo.sum());
}

main();
]]

local ed       = document:getElementById("ed")
local crumbEl  = document:getElementById("crumb")
local snipEl   = document:getElementById("snip-state")
local caretEl  = document:getElementById("caret")
local verEl    = document:getElementById("ver")
local logBox   = document:getElementById("log")
local blameSel = document:getElementById("sel-blame-mode")

local function logLine(s)
    logBox.textContent = (logBox.textContent or "") .. tostring(s) .. "\n"
end

-- ---------- M3-01: inject a hand-crafted git diff ----------
-- Three kinds populated in one shot: added lines 8/9/10, modified line 14,
-- deleted wedge after line 21.
document:getElementById("btn-inject-git"):addEventListener("click", function(e)
    ed:setGitAddedLines("[8,9,10]")
    ed:setGitModifiedLines("[14]")
    ed:setGitDeletedAfterLines("[21]")
    logLine("git diff injected (added [8,9,10] / modified [14] / del-wedge after 21)")
    ed:focus()
end)

-- ---------- M3-02: inject a hand-crafted blame ----------
-- Note: use `[=[ ... ]=]` so the closing JSON `]` doesn't collide with
-- Lua's default `[[ ... ]]` multi-line string terminator.
document:getElementById("btn-inject-blame"):addEventListener("click", function(e)
    local body = [=[
[
  { "startLine": 0,  "endLine": 6,  "author": "Alice", "date": "2026-05-10", "commit": "abc1234" },
  { "startLine": 7,  "endLine": 20, "author": "Bob",   "date": "2026-05-12", "commit": "def5678" },
  { "startLine": 21, "endLine": 30, "author": "Carol", "date": "2026-05-14", "commit": "9012abc" }
]
]=]
    if ed:setBlameRecordsJson(body) then
        logLine("blame injected (3 records covering full file)")
    else
        logLine("blame inject failed")
    end
    ed:focus()
end)

blameSel:addEventListener("change", function(e)
    local mode = blameSel.value
    ed:setBlameMode(mode)
    logLine("blame mode -> " .. mode)
    ed:focus()
end)

-- ---------- M3-03: sticky toggle ----------
document:getElementById("btn-toggle-sticky"):addEventListener("click", function(e)
    local on = not ed:getStickyVisible()
    ed:setStickyVisible(on)
    logLine("sticky -> " .. tostring(on))
    ed:focus()
end)

-- ---------- M3-04: bracket pair colorization / indent guides ----------
document:getElementById("btn-toggle-bpc"):addEventListener("click", function(e)
    local on = not ed:getBracketColorsEnabled()
    ed:setBracketColorsEnabled(on)
    logLine("BPC -> " .. tostring(on))
    ed:focus()
end)
document:getElementById("btn-toggle-guides"):addEventListener("click", function(e)
    local on = not ed:getIndentGuidesEnabled()
    ed:setIndentGuidesEnabled(on)
    logLine("indent guides -> " .. tostring(on))
    ed:focus()
end)

-- ---------- M3-06: snippets ----------
document:getElementById("btn-snippet-fn"):addEventListener("click", function(e)
    ed:insertSnippet("function ${1:name}(${2:args}) {\n    $0\n}")
    logLine("snippet 'fn' inserted; Tab to advance")
    ed:focus()
end)
document:getElementById("btn-snippet-for"):addEventListener("click", function(e)
    ed:insertSnippet("for (let ${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n    $0\n}")
    logLine("snippet 'for' inserted (NOTE: no mirror yet -- mirror in M4)")
    ed:focus()
end)
document:getElementById("btn-snippet-next"):addEventListener("click", function(e)
    local ok = ed:snippetNext()
    logLine("snippetNext -> " .. tostring(ok))
    ed:focus()
end)
document:getElementById("btn-snippet-cancel"):addEventListener("click", function(e)
    ed:snippetCancel()
    logLine("snippet cancelled")
    ed:focus()
end)

-- ---------- Initial buffer ----------
ed:setValue(sampleCode)
ed:setLanguage("javascript")
ed:setTheme("mocha")
ed:focus()

-- ---------- Live status bar (caret-driven breadcrumb) ----------
setInterval(function()
    local line = ed:getCaretLine()
    local col  = ed:getCaretCol()
    caretEl.textContent = string.format("%d:%d", line, col)
    verEl.textContent   = tostring(ed:getVersion())

    -- M3-05 Breadcrumb: text path at caret.
    local txt = ed:getBreadcrumbText(line, 30)
    if txt and txt ~= "" then
        crumbEl.textContent = txt
    else
        crumbEl.textContent = "(top-level)"
    end

    -- M3-06 Snippet state.
    if ed:snippetIsActive() then
        snipEl.textContent = "active, stop=" .. tostring(ed:snippetActiveIndex())
    else
        snipEl.textContent = "inactive"
    end
end, 80)

print("[M3 demo] ready -- try toolbar buttons in any order")
</script>

</body></html>
    )HTML");

#ifdef KUI_HAS_SCRIPTING
    auto scriptEngine = kui::ScriptEngine::create();
    if (scriptEngine) {
        scriptEngine->setErrorHandler([](const kui::String& err,
                                         const kui::String& src, int line) {
            fprintf(stderr, "[Lua Error] %s:%d  %s\n",
                     src.c_str(), line, err.c_str());
        });
        scriptEngine->bindDocument(doc);
        scriptEngine->executeScripts();
        // Expose engine to F12 DevTools for live console eval (Bug A fix).
        window->setScriptEngine(scriptEngine.get());
    } else {
        fprintf(stderr, "[M3 demo] ScriptEngine::create() returned null;"
                          " toolbar buttons will not work.\n");
    }
#else
    fprintf(stderr, "[M3 demo] KUI_HAS_SCRIPTING not enabled;"
                      " demo will load but toolbar buttons are no-ops.\n");
#endif

    int rc = kui::Platform::run();

#ifdef KUI_HAS_SCRIPTING
    if (scriptEngine) {
        scriptEngine->unbindDocument();
        scriptEngine.reset();
    }
#endif
    return rc;
}
