// ===========================================================================
// 14_kui_editor_large_file_demo — Showcase for M5 perf optimisations.
//
// Demonstrates how the editor handles synthetic large buffers under
// repeated find / typing pressure.  Pre M5-05 the find engine
// rebuilt the joined buffer on every keystroke / findNext (910 ms for
// 1000x findNext on a 50K-line buffer); post M5-05 the same workload
// completes in ~16 ms (57x speedup).
//
// Toolbar surfaces:
//   * Buffer size selector  -- 10K / 50K / 100K lines (regenerate)
//   * Find <needle> + repeat-N -- runs findNext N times and reports the
//                                  total wall-clock + per-call avg
//   * Cache stats           -- shows current bufferContentVersion +
//                              cachedJoinedVersion + their match status
//
// All workload generation is host-side Lua so users can tweak buffer
// size / repeat count without rebuilding the executable.
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
        .title  = "KUI — kui_ext_editor M5 large-file perf demo",
        .width  = 1500,
        .height = 940,
    });

    auto* doc = window->document();
    // Robust across launch modes -- see `../_find_extensions_dir.h`.
    const std::string extDir = kui_examples_internal::findExtensionsDir();
    fprintf(stderr, "[kui-editor large-file demo] loading extensions from: %s\n",
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
.subtitle { color: #94a3b8; font-size: 12px; }
.toolbar { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 10px;
           align-items: center; }
.toolbar > * { font: 13px "Segoe UI"; }
.toolbar label { color: #94a3b8; font-size: 12px; }
.toolbar button {
    padding: 6px 12px;
    border-radius: 4px;
    border: 1px solid #334155;
    background: #1e293b;
    color: #e2e8f0;
    cursor: pointer;
}
.toolbar button:hover { background: #334155; }
.toolbar button.primary { background: #1e3a8a; border-color: #2563eb; }
.toolbar button.primary:hover { background: #2563eb; }
.toolbar button.danger  { background: #7f1d1d; border-color: #b91c1c; }
.toolbar button.danger:hover  { background: #b91c1c; }
.toolbar select, .toolbar input {
    padding: 6px 10px; border-radius: 4px; border: 1px solid #334155;
    background: #1e293b; color: #e2e8f0;
}
.toolbar input.needle { width: 200px; }
.toolbar input.numfield { width: 90px; }
.editor-wrap { display: flex; height: 580px; }
kui-editor    { flex: 1; height: 100%;
                background: #1e293b; border: 1px solid #334155;
                font-family: "Cascadia Code", "Consolas", monospace; }
.statusbar {
    margin-top: 8px; padding: 8px 12px;
    background: #1e293b;
    border: 1px solid #334155;
    border-radius: 4px;
    font: 12px "Cascadia Code", monospace;
    display: flex; gap: 24px; flex-wrap: wrap;
}
.statusbar span.label { color: #94a3b8; }
.metric-good { color: #4ade80; }
.metric-warn { color: #fbbf24; }
#log {
    margin-top: 8px;
    padding: 8px 12px;
    background: #020617;
    border: 1px solid #334155;
    border-radius: 4px;
    max-height: 200px;
    overflow-y: auto;
    font: 11px "Cascadia Code", monospace;
    color: #cbd5e1;
    white-space: pre;
}
#log .perf-line { color: #4ade80; }
#log .err-line { color: #f87171; }
</style></head>
<body>
<header>
    <div>
        <h1>kui-editor — M5 large-file perf demo</h1>
        <div class="subtitle">v2.32.0 • shared joined-buffer cache (M5-05) • 57× find speedup</div>
    </div>
</header>

<div class="toolbar">
    <label>Buffer:</label>
    <select id="sel-size">
        <option value="1000">1K lines (~30KB)</option>
        <option value="10000" selected>10K lines (~300KB)</option>
        <option value="50000">50K lines (~1.5MB)</option>
        <option value="100000">100K lines (~3MB)</option>
        <option value="500000">500K lines (~15MB) — slow generation</option>
    </select>
    <button id="btn-regen" class="primary">Regenerate buffer</button>

    <label style="margin-left:16px;">Find:</label>
    <input id="needle" class="needle" type="text" value="row42" />
    <button id="btn-find">findSet</button>

    <label>Repeat:</label>
    <input id="repeat-n" class="numfield" type="number" value="1000" min="1" max="100000"/>
    <button id="btn-repeat" class="primary">Repeat findNext × N</button>
    <button id="btn-clear-log" class="danger">Clear log</button>
</div>

<div class="editor-wrap">
    <kui-editor id="ed" line-numbers="true"></kui-editor>
</div>

<div class="statusbar">
    <span><span class="label">caret:</span> <span id="caret">0:0</span></span>
    <span><span class="label">version:</span> <span id="ver">0</span></span>
    <span><span class="label">buffer ver:</span> <span id="bver">0</span></span>
    <span><span class="label">lines:</span> <span id="lines">0</span></span>
    <span><span class="label">bytes:</span> <span id="bytes">0</span></span>
    <span><span class="label">find matches:</span> <span id="matches">-</span></span>
    <span><span class="label">last op:</span> <span id="lastop">(none)</span></span>
</div>

<div id="log"></div>

<script>
local ed       = document:getElementById("ed")
local caretEl  = document:getElementById("caret")
local verEl    = document:getElementById("ver")
local bverEl   = document:getElementById("bver")
local linesEl  = document:getElementById("lines")
local bytesEl  = document:getElementById("bytes")
local matchEl  = document:getElementById("matches")
local lastopEl = document:getElementById("lastop")
local logBox   = document:getElementById("log")
local sizeSel  = document:getElementById("sel-size")
local needle   = document:getElementById("needle")
local repeatN  = document:getElementById("repeat-n")

local function logLine(s, cls)
    local prefix = ""
    if cls == "perf" then prefix = "[perf] "
    elseif cls == "err" then prefix = "[err]  "
    else prefix = "[info] " end
    logBox.textContent = (logBox.textContent or "") .. prefix .. tostring(s) .. "\n"
    -- Keep log scrolled to bottom.
    logBox.scrollTop = logBox.scrollHeight
end

-- ---------- generate a synthetic buffer of N lines ----------
-- Each line: "row<i>: lorem ipsum line filler ..." -- ~30 bytes.  This
-- gives the find engine many candidate matches for needles like
-- "row42", "lorem", "filler", etc.
local function generateBuffer(n)
    local t0 = window.performance.now()
    local parts = {}
    for i = 0, n - 1 do
        parts[#parts + 1] = string.format(
            "row%d: lorem ipsum dolor sit amet, line filler %d", i, i)
    end
    local body = table.concat(parts, "\n")
    local t1 = window.performance.now()
    logLine(string.format("generated %d lines (%d bytes) in %.1f ms",
        n, #body, t1 - t0), "perf")
    return body
end

local function refreshStatusBar()
    caretEl.textContent  = string.format("%d:%d",
                                          ed:getCaretLine(),
                                          ed:getCaretCol())
    verEl.textContent    = tostring(ed:getVersion())
    bverEl.textContent   = tostring(ed:getBufferContentVersion())
    linesEl.textContent  = tostring(ed:getLineCount())
    bytesEl.textContent  = tostring(#ed:getValue())
    matchEl.textContent  = tostring(ed:findCount())
end

-- ---------- buffer regeneration ----------
local function regenerate()
    local n = tonumber(sizeSel.value) or 10000
    local body = generateBuffer(n)
    local t0 = window.performance.now()
    ed:setValue(body)
    local t1 = window.performance.now()
    logLine(string.format("setValue(%d bytes) -> %.1f ms",
        #body, t1 - t0), "perf")
    lastopEl.textContent = string.format("regen %d lines", n)
    refreshStatusBar()
    ed:focus()
end

document:getElementById("btn-regen"):addEventListener("click", function(e)
    regenerate()
end)

-- ---------- single findSet ----------
document:getElementById("btn-find"):addEventListener("click", function(e)
    local pat = needle.value or ""
    local t0 = window.performance.now()
    local count = ed:findSet(pat)
    local t1 = window.performance.now()
    logLine(string.format("findSet(%q) -> %d matches in %.2f ms",
        pat, count, t1 - t0), "perf")
    lastopEl.textContent = string.format("findSet '%s' = %d", pat, count)
    refreshStatusBar()
    ed:focus()
end)

-- ---------- repeat findNext N times (the M5-05 hot path) ----------
document:getElementById("btn-repeat"):addEventListener("click", function(e)
    local n = tonumber(repeatN.value) or 1000
    if n < 1 then return end
    -- Make sure we have an active query first.
    local pat = needle.value or ""
    if pat == "" then
        logLine("set a needle first", "err")
        return
    end
    if ed:findCount() == 0 then
        ed:findSet(pat)
    end
    local t0 = window.performance.now()
    for i = 1, n do
        ed:findNext()
    end
    local t1 = window.performance.now()
    local ms = t1 - t0
    local avgUs = (ms * 1000) / n
    logLine(string.format("findNext × %d -> %.1f ms total / %.2f µs avg",
        n, ms, avgUs), "perf")
    lastopEl.textContent = string.format("findNext × %d = %.1fms", n, ms)
    refreshStatusBar()
    ed:focus()
end)

-- ---------- clear log ----------
document:getElementById("btn-clear-log"):addEventListener("click", function(e)
    logBox.textContent = ""
end)

-- ---------- bootstrap ----------
ed:setLanguage("plain")
ed:setTheme("mocha")
regenerate()

setInterval(function()
    -- caret + version live update
    caretEl.textContent = string.format("%d:%d",
                                         ed:getCaretLine(),
                                         ed:getCaretCol())
    verEl.textContent  = tostring(ed:getVersion())
    bverEl.textContent = tostring(ed:getBufferContentVersion())
end, 100)

logLine("ready.  M5-05 cache guarantees: rebuild only on bufferContentVersion changes.", "info")
logLine("Try: select 50K lines -> Regenerate -> findSet 'row42' -> Repeat findNext × 1000.", "info")
</script>
</body></html>
)HTML");

    // ---- Bug B fix: bring up the script engine so the inline <script>
    //                 Lua block actually runs (toolbar buttons, status bar,
    //                 logLine, regenerate buffer, etc.).  Without this the
    //                 entire demo would be a static empty editor.
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
        // Expose engine to F12 DevTools for live console eval.
        window->setScriptEngine(scriptEngine.get());
    } else {
        fprintf(stderr, "[large-file demo] ScriptEngine::create() returned null;"
                          " toolbar buttons + log will not work.\n");
    }
#else
    fprintf(stderr, "[large-file demo] KUI_HAS_SCRIPTING not enabled;"
                      " demo will load but toolbar buttons are no-ops.\n");
#endif

    window->show();
    int rc = kui::Platform::run();

#ifdef KUI_HAS_SCRIPTING
    if (scriptEngine) {
        scriptEngine->unbindDocument();
        scriptEngine.reset();
    }
#endif
    return rc;
}
