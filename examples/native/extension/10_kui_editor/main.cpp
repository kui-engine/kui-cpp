// ===========================================================================
// 10_kui_editor — Showcase for the kui_ext_editor extension (M1-TS-A 1.1.1)
//
// Demonstrates the full M0 + M1-TS-A feature surface:
//   * 7-language <select> dropdown (JS / TS / TSX / C / C++ / Python / Lua)
//     — one-pick switch drives BOTH setValue() and setLanguage() so the
//     tree-sitter真高亮 lights up immediately
//   * 3 themes (Mocha / Latte / High-contrast) with one-click switch
//   * Live font-size +/- control (range 10–24 px)
//   * **Live AST panel** (right column) polls `ed:getAstSummary(80)` at
//     ~5 Hz to show what tree-sitter is seeing — first-class diagnostic
//     for the M1-TS sub-stage
//   * Stats overlay (lines / version / theme / language / font)
//   * All wiring via inline Lua hitting the extension's lua surface
//     (setTheme / setFontSize / setLanguage / setValue / getLineCount /
//      getVersion / getTheme / getLanguage / getAstSummary)
//
// Launch modes (all supported via the _find_extensions_dir.h helper):
//   * Double-click from Explorer  -- cwd == .exe directory
//   * `cmake --build` then run from repo root
//   * Run from a deployed SDK install
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
        .title  = "KUI — kui_ext_editor 1.1.1 showcase (7-lang AST panel)",
        .width  = 1400,
        .height = 800,
    });

    auto* doc = window->document();
    const std::string extDir = kui_examples_internal::findExtensionsDir();
    fprintf(stderr, "[kui-editor showcase] loading extensions from: %s\n",
            extDir.c_str());
    doc->initExtensions(extDir);

    doc->loadHTML(R"HTML(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    padding: 18px;
    font-family: "Segoe UI", sans-serif;
    background: #0f172a;
    color: #e2e8f0;
    min-height: 100vh;
}
header {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    margin-bottom: 8px;
}
h2 { color: #f1f5f9; font-size: 20px; }
.subtitle { color: #94a3b8; font-size: 13px; }
.toolbar {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    align-items: center;
    padding: 10px 14px;
    margin: 12px 0 0 0;
    background: #1e293b;
    border: 1px solid #334155;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
}
.toolbar .group { display: flex; gap: 4px; align-items: center; }
.toolbar .label { color: #94a3b8; font-size: 12px;
                  margin: 0 6px 0 0; }
.toolbar button {
    background: #334155;
    color: #e2e8f0;
    border: none;
    padding: 5px 11px;
    border-radius: 5px;
    font-size: 12px;
    cursor: pointer;
    font-family: inherit;
}
.toolbar button:hover { background: #475569; }
.toolbar button.active { background: #3b82f6; color: white; }
.toolbar select {
    background: #334155;
    color: #e2e8f0;
    border: 1px solid #475569;
    padding: 5px 9px;
    border-radius: 5px;
    font-size: 12px;
    font-family: inherit;
    cursor: pointer;
    min-width: 130px;
}
.toolbar select:hover { background: #475569; }
.toolbar .sep { width: 1px; height: 20px; background: #334155; margin: 0 6px; }
.toolbar .stats {
    margin-left: auto;
    color: #94a3b8;
    font-size: 11px;
    font-family: "Consolas", monospace;
}
/* ---------- two-column work area (editor | AST panel) -------------------- */
.work {
    display: flex;
    gap: 0;
    background: #1e1e2e;
    border-bottom-left-radius: 8px;
    border-bottom-right-radius: 8px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    overflow: hidden;
    border: 1px solid #334155;
    border-top: none;
}
.editor-host {
    flex: 1 1 60%;
    min-width: 0;
    border-right: 1px solid #334155;
}
kui-editor {
    display: block;
    width: 100%;
    height: 520px;
    outline: none;
}
.ast-panel {
    flex: 0 0 40%;
    background: #181825;
    display: flex;
    flex-direction: column;
    min-width: 0;
}
.ast-header {
    padding: 8px 12px;
    background: #1e293b;
    border-bottom: 1px solid #334155;
    font-size: 12px;
    color: #94a3b8;
    display: flex;
    align-items: center;
    justify-content: space-between;
}
.ast-header .title { color: #cdd6f4; font-weight: 600; }
.ast-header .meta  { font-family: "Consolas", monospace; font-size: 11px; }
.ast-body {
    flex: 1 1 auto;
    padding: 10px 12px;
    overflow: auto;
    font-family: "Consolas", "Cascadia Mono", monospace;
    font-size: 11px;
    line-height: 1.5;
    color: #a6adc8;
    white-space: pre;
    height: 480px;
}
.hint {
    margin-top: 16px;
    color: #94a3b8;
    font-size: 12px;
    font-family: "Consolas", monospace;
    line-height: 1.6;
}
.hint kbd {
    background: #313244;
    padding: 2px 6px;
    border-radius: 4px;
    color: #cdd6f4;
    margin: 0 2px;
}
</style></head><body>

<header>
  <h2>kui-editor — M1-TS-A 1.1.1 showcase</h2>
  <span class="subtitle">7 grammars · 33 lua methods · live tree-sitter AST panel</span>
</header>

<div class="toolbar">
  <span class="label">Language:</span>
  <select id="lang">
    <option value="js"     >JavaScript</option>
    <option value="ts"      selected>TypeScript</option>
    <option value="tsx"    >TSX (React)</option>
    <option value="c"      >C</option>
    <option value="cpp"    >C++</option>
    <option value="py"     >Python</option>
    <option value="lua"    >Lua</option>
  </select>
  <span class="sep"></span>
  <span class="label">Theme:</span>
  <div class="group">
    <button id="thm-mocha" class="active">Mocha</button>
    <button id="thm-latte" >Latte</button>
    <button id="thm-hc"    >HC</button>
  </div>
  <span class="sep"></span>
  <span class="label">Size:</span>
  <div class="group">
    <button id="font-dec">A−</button>
    <button id="font-inc">A+</button>
  </div>
  <span class="stats" id="stats">…</span>
</div>

<div class="work">
  <div class="editor-host">
    <kui-editor id="ed"></kui-editor>
  </div>
  <div class="ast-panel">
    <div class="ast-header">
      <span class="title">Tree-sitter AST</span>
      <span class="meta" id="ast-meta">— nodes</span>
    </div>
    <div class="ast-body" id="ast">…</div>
  </div>
</div>

<p class="hint">
  <kbd>←</kbd><kbd>→</kbd><kbd>↑</kbd><kbd>↓</kbd> nav ·
  <kbd>Ctrl</kbd>+<kbd>←</kbd>/<kbd>→</kbd> word ·
  <kbd>Home</kbd>/<kbd>End</kbd> · <kbd>PageUp</kbd>/<kbd>PageDown</kbd> ·
  <kbd>Shift</kbd>+move extends · double-click word · triple-click line ·
  <kbd>Ctrl</kbd>+<kbd>A</kbd>/<kbd>C</kbd>/<kbd>X</kbd>/<kbd>V</kbd> ·
  <kbd>Ctrl</kbd>+<kbd>Z</kbd>/<kbd>Y</kbd> · <kbd>Tab</kbd>/<kbd>Enter</kbd>
</p>

<script type="text/lua">
-- ===========================================================================
-- kui-editor 1.1.1 showcase — wires up:
--   * 7-language <select> driver  → setValue + setLanguage
--   * 3-theme buttons              → setTheme
--   * font ± buttons               → setFontSize
--   * stats poll (10 Hz, version-gated, only redraws on change)
--   * AST panel poll (5 Hz, version-gated, ed:getAstSummary)
-- ===========================================================================

-- ----- 7 short language samples (one per grammar) --------------------------
local samples = {}

samples.js =
[[// JavaScript example
function fibonacci(n) {
    if (n < 2) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

const memo = new Map();
function fib(n) {
    if (memo.has(n)) return memo.get(n);
    const v = n < 2 ? n : fib(n - 1) + fib(n - 2);
    memo.set(n, v);
    return v;
}

console.log(fib(40));
]]

samples.ts =
[[// TypeScript example
import { useState, useEffect } from 'react';

interface Greeting { name: string; }

export function Hello({ name }: Greeting) {
    // Simple state hook demo
    const [count, setCount] = useState(0);

    useEffect(() => {
        const id = setInterval(() => setCount(c => c + 1), 1000);
        return () => clearInterval(id);
    }, []);

    return `Hello, ${name}!  count = ${count}`;
}
]]

samples.tsx =
[[// TSX (React JSX + TypeScript) example
import React from "react";

interface Props { items: string[]; }

export const ItemList: React.FC<Props> = ({ items }) => (
    <ul className="items">
        {items.map((it, idx) => (
            <li key={idx} data-idx={idx}>
                <span>{it}</span>
            </li>
        ))}
    </ul>
);
]]

samples.c =
[[// C example
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t len;
} Buf;

int main(int argc, char** argv) {
    Buf b = { .data = NULL, .len = 0 };
    for (int i = 1; i < argc; ++i) {
        b.len += strlen(argv[i]);
    }
    b.data = malloc(b.len + 1);
    printf("total = %zu\n", b.len);
    free(b.data);
    return 0;
}
]]

samples.cpp =
[[// C++ example
#include <iostream>
#include <vector>
#include <algorithm>

class Editor {
public:
    Editor() = default;

    void insert(const std::string& s) {
        // Append to the buffer; O(1) amortised.
        lines_.push_back(s);
    }

    size_t lineCount() const noexcept { return lines_.size(); }

private:
    std::vector<std::string> lines_;
};

int main() {
    Editor e;
    e.insert("hello");
    e.insert("world");
    std::cout << e.lineCount() << "\n";   // 2
    return 0;
}
]]

samples.py =
[[# Python example
import math
from dataclasses import dataclass

@dataclass
class Point:
    x: float
    y: float

    def distance(self, other: "Point") -> float:
        # Euclidean distance
        dx = self.x - other.x
        dy = self.y - other.y
        return math.sqrt(dx * dx + dy * dy)

if __name__ == "__main__":
    p, q = Point(0, 0), Point(3, 4)
    print(f"d = {p.distance(q):.2f}")  # 5.00
]]

samples.lua =
[[-- Lua example
local function fib(n, memo)
    memo = memo or {}
    if memo[n] then return memo[n] end
    if n < 2 then return n end
    local v = fib(n - 1, memo) + fib(n - 2, memo)
    memo[n] = v
    return v
end

for i = 1, 10 do
    io.write(string.format("fib(%d) = %d\n", i, fib(i)))
end
]]

local ed       = document:getElementById("ed")
local stats    = document:getElementById("stats")
local astBody  = document:getElementById("ast")
local astMeta  = document:getElementById("ast-meta")
local langSel  = document:getElementById("lang")
local fontSize = 14

local function setActive(groupIds, activeId)
    for _, id in ipairs(groupIds) do
        local b = document:getElementById(id)
        if b then
            if id == activeId then b:addClass("active")
            else                   b:removeClass("active") end
        end
    end
end

-- Map our short option-value to tree-sitter grammar name (most are
-- identical aliases the extension recognises directly, but TS is the
-- exception — its short form `ts` is also fine).
local tsName = {
    js   = "javascript",
    ts   = "typescript",
    tsx  = "tsx",
    c    = "c",
    cpp  = "cpp",
    py   = "python",
    lua  = "lua",
}

local function applyLanguage(lang)
    if not samples[lang] then return end       -- defensive: unknown option
    ed:setValue(samples[lang])
    ed:setLanguage(tsName[lang])               -- M1-TS-A: enable real AST highlighter
    ed:focus()
end

local function applyTheme(name)
    ed:setTheme(name)
    ed:focus()
    setActive({"thm-mocha", "thm-latte", "thm-hc"},
              "thm-"..(name == "hc" and "hc" or name))
end

local function bumpFont(delta)
    fontSize = math.max(10, math.min(24, fontSize + delta))
    ed:setFontSize(fontSize)
    ed:focus()
end

-- ----- Wire toolbar -------------------------------------------------------
langSel:addEventListener("change", function(e)
    applyLanguage(langSel.value)
end)

document:getElementById("thm-mocha"):addEventListener("click",
    function(e) applyTheme("mocha") end)
document:getElementById("thm-latte"):addEventListener("click",
    function(e) applyTheme("latte") end)
document:getElementById("thm-hc")   :addEventListener("click",
    function(e) applyTheme("hc")    end)

document:getElementById("font-inc"):addEventListener("click",
    function(e) bumpFont( 2) end)
document:getElementById("font-dec"):addEventListener("click",
    function(e) bumpFont(-2) end)

-- ----- Initial load -------------------------------------------------------
applyLanguage("ts")
applyTheme("mocha")

-- ----- Stats poll (10 Hz, redraw only on version bump) --------------------
local lastStatsVersion = -1
setInterval(function()
    local v = ed:getVersion()
    if v == lastStatsVersion then return end
    lastStatsVersion = v
    stats.textContent = string.format(
        "  lines=%d  v=%d  font=%s  theme=%s  lang=%s",
        ed:getLineCount(), v,
        tostring(ed:getFontSize()),
        ed:getTheme(),
        ed:getLanguage())
end, 100)

-- ----- AST panel poll (5 Hz, redraw only on version bump) -----------------
-- Tree-sitter parse is sub-millisecond for our tiny samples and the dump
-- itself is O(maxNodes), so polling at 5 Hz costs essentially nothing.
local lastAstVersion = -2
setInterval(function()
    local v = ed:getVersion()
    if v == lastAstVersion then return end
    lastAstVersion = v
    local dump = ed:getAstSummary(80)
    astBody.textContent = dump
    -- Count lines for the meta header.
    local n = 0
    for _ in dump:gmatch("\n") do n = n + 1 end
    astMeta.textContent = string.format("%d node%s", n, n == 1 and "" or "s")
end, 200)

print("[kui-editor showcase] ready — 7 grammars + AST panel live")
</script>

</body></html>
    )HTML");

#ifdef KUI_HAS_SCRIPTING
    auto scriptEngine = kui::ScriptEngine::create();
    if (scriptEngine) {
        scriptEngine->setErrorHandler([](const kui::String& err,
                                         const kui::String& src, int line) {
            fprintf(stderr, "[Lua Error] %s:%d — %s\n",
                     src.c_str(), line, err.c_str());
        });
        scriptEngine->bindDocument(doc);
        scriptEngine->executeScripts();
        // Expose engine to F12 DevTools for live console eval (Bug A fix).
        window->setScriptEngine(scriptEngine.get());
    } else {
        fprintf(stderr, "[showcase] ScriptEngine::create() returned null;"
                          " buttons + theme switching will not work.\n");
    }
#else
    fprintf(stderr, "[showcase] KUI_HAS_SCRIPTING not enabled;"
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
