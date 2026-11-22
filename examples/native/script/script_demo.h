#pragma once

#include "../common/demo_runtime.h"

#include <string>

namespace kui::examples {

inline int runScriptDemo(int id) {
    static const char* titles[] = {
        "Lua Hello", "Lua DOM", "Lua Events", "Lua Animation", "Lua Data"
    };
    static const char* descriptions[] = {
        "Lua executes and writes a computed value into the DOM.",
        "Lua creates, styles and removes DOM nodes.",
        "Lua event listeners handle real pointer input.",
        "requestAnimationFrame drives a DOM animation from Lua.",
        "Lua tables are rendered into a live data view."
    };
    const int index = id < 1 ? 0 : (id > 5 ? 4 : id - 1);

    static const char* scripts[] = {
R"LUA(
local values = {2, 3, 5, 7, 11, 13}
local total = 0
for _, value in ipairs(values) do total = total + value end
document:getElementById("output").textContent =
  "Hello from Lua 5.x — prime sum = " .. tostring(total)
document:getElementById("status").textContent = "PASS · Lua code executed"
)LUA",
R"LUA(
local list = document:getElementById("output")
list.textContent = ""
for i = 1, 6 do
  local row = document:createElement("div")
  row.className = "row"
  row.textContent = "Lua-created DOM node #" .. tostring(i)
  row.style:setProperty("opacity", tostring(0.45 + i * 0.09))
  list:appendChild(row)
end
document:getElementById("action"):addEventListener("click", function()
  local row = list.lastElementChild
  if row then list:removeChild(row) end
end)
document:getElementById("status").textContent = "PASS · 6 nodes created"
)LUA",
R"LUA(
local count = 0
local button = document:getElementById("action")
button.textContent = "Click me"
button:addEventListener("click", function(event)
  count = count + 1
  document:getElementById("output").textContent =
    "Lua handled click #" .. tostring(count) .. " (" .. event.type .. ")"
  document:getElementById("status").textContent = "PASS · event dispatched"
end)
document:getElementById("output").textContent = "Waiting for a real click..."
)LUA",
R"LUA(
local box = document:getElementById("box")
local output = document:getElementById("output")
local frameOutput = document:createElement("div")
frameOutput.style:setProperty("position", "relative")
frameOutput.style:setProperty("z-index", "1")
output:appendChild(frameOutput)
local running = true
local frame = 0
document:getElementById("action").textContent = "Pause / resume"
document:getElementById("action"):addEventListener("click", function()
  running = not running
end)
local function animate(now)
  if running then
    frame = frame + 1
    local x = 170 + math.sin(now / 500) * 150
    local y = 55 + math.cos(now / 700) * 35
    box.style:setProperty("transform", "translate(" .. tostring(x) .. "px," .. tostring(y) .. "px) rotate(" .. tostring(now / 12) .. "deg)")
    frameOutput.textContent = "Frame " .. tostring(frame) .. " · timestamp " .. tostring(math.floor(now)) .. " ms"
    document:getElementById("status").textContent = "PASS · rAF is active"
  end
  requestAnimationFrame(animate)
end
requestAnimationFrame(animate)
)LUA",
R"LUA(
local records = {
  {name="Parser", value=97, color="#60a5fa"},
  {name="Layout", value=82, color="#34d399"},
  {name="Paint", value=74, color="#fbbf24"},
  {name="Input", value=91, color="#c084fc"}
}
local output = document:getElementById("output")
output.textContent = ""
for _, record in ipairs(records) do
  local row = document:createElement("div")
  row.className = "metric"
  row.innerHTML = "<span>" .. record.name .. "</span><b>" .. tostring(record.value) .. "%</b>"
  row.style:setProperty("border-left-color", record.color)
  output:appendChild(row)
end
document:getElementById("status").textContent = "PASS · " .. tostring(#records) .. " records rendered"
)LUA"
    };

    std::string html = std::string(R"HTML(
<!DOCTYPE html><html><head><style>
*{box-sizing:border-box}body{margin:0;padding:36px;background:#0f172a;color:#e2e8f0;font-family:"Segoe UI",sans-serif}
.card{max-width:720px;margin:0 auto;background:#1e293b;border:1px solid #334155;border-radius:14px;padding:24px}h1{margin:0 0 7px;font-size:23px;color:#7dd3fc}.desc{color:#94a3b8;font-size:13px}
#output{margin-top:18px;min-height:170px;padding:16px;background:#020617;border-radius:9px;color:#cbd5e1;font:13px Consolas,monospace;position:relative;overflow:hidden}.row,.metric{padding:8px 10px;margin:5px 0;background:#111827;border-radius:5px}.metric{display:flex;justify-content:space-between;border-left:4px solid #64748b}
#box{position:absolute;width:46px;height:46px;border-radius:9px;background:linear-gradient(135deg,#38bdf8,#8b5cf6);left:0;top:0}.tools{margin-top:14px;display:flex;align-items:center;gap:12px}.btn{padding:8px 13px;background:#2563eb;border-radius:6px;color:white;cursor:pointer;font-size:12px}#status{color:#86efac;font-size:12px}
</style></head><body><div class="card"><h1>)HTML") + titles[index] + "</h1><div class=\"desc\">" + descriptions[index] +
        R"HTML(</div><div id="output"><div id="box"></div></div><div class="tools"><div class="btn" id="action">Run action</div><div id="status">Running Lua...</div></div></div><script type="text/lua">)HTML" +
        scripts[index] + "</script></body></html>";
    return runLuaDocument((std::string("KUI — ") + titles[index]).c_str(),
                          820, 620, std::move(html), "native_script_demo.lua");
}

} // namespace kui::examples
