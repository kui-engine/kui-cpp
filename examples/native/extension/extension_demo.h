#pragma once

#include "_find_extensions_dir.h"

#include <kui/kui.h>
#include <kui/extension/extension_manager.h>

#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace kui::examples {

inline int runHelloExtensionDemo(int id) {
    static const std::array<const char*, 9> titles = {
        "Hello Extension", "Custom Tag", "Style & Attribute Invalidation",
        "DOM Lifecycle", "Extension Toolbar", "Extension Notification",
        "Extension Data Table", "Extension Chart", "Spectrum Visualizer"};
    static const std::array<const char*, 9> descriptions = {
        "Loads kui_ext_hello and renders through its C ABI paint callback.",
        "Creates and replaces a registered <kui-hello> element at runtime.",
        "Changes observed attributes and requests an extension repaint.",
        "Connects, disconnects and reconnects a native custom element.",
        "Toolbar actions call the extension's typed greet() method.",
        "Extension results drive a live notification component.",
        "Native extension calls populate and refresh a real data table.",
        "Frame counts returned across the C ABI update a live bar chart.",
        "Tick values returned by the extension animate a spectrum display."};

    if (id < 1 || id > 9 || !Platform::init()) return 1;
    Platform::setF12DevToolsEnabled(true);
    auto window = Window::create({.title = std::string("KUI — ") + titles[id - 1],
                                  .width = 900, .height = 650});
    if (!window) return 2;
    auto* doc = window->document();
    const std::string extDir = kui_examples_internal::findExtensionsDir("kui_ext_hello");
    doc->initExtensions(extDir);

    std::ostringstream html;
    html << R"HTML(<!doctype html><html><head><style>
*{box-sizing:border-box}body{margin:0;padding:28px;font-family:"Segoe UI",sans-serif;background:#07111f;color:#e2e8f0}
.hero{padding:22px;border-radius:16px;background:linear-gradient(135deg,#0f766e,#2563eb);margin-bottom:18px}h1{margin:0 0 6px;font-size:24px}.hero p{margin:0;color:#dbeafe}
.card{background:#101d30;border:1px solid #29405e;border-radius:14px;padding:20px;margin-top:14px}.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
button,input{padding:9px 13px;border-radius:8px;border:1px solid #3b516d;background:#162840;color:#e2e8f0}button{cursor:pointer;background:#2563eb;border:0;font-weight:700}
kui-hello{display:block;width:100%;height:90px;margin:16px 0;border-radius:10px;overflow:hidden}.status{font:13px Consolas,monospace;color:#93c5fd;white-space:pre-wrap}
.bars{display:flex;align-items:flex-end;height:150px;gap:8px;margin-top:14px}.bar{flex:1;min-height:8px;background:linear-gradient(#22d3ee,#7c3aed);border-radius:6px 6px 2px 2px}
table{width:100%;border-collapse:collapse;margin-top:12px}td,th{padding:8px;border-bottom:1px solid #29405e;text-align:left}.toast{padding:12px;border-radius:9px;background:#064e3b;color:#a7f3d0;margin-top:12px}
</style></head><body><div class="hero"><h1>)HTML" << titles[id - 1]
         << "</h1><p>" << descriptions[id - 1] << R"HTML(</p></div>
<div class="card"><div id="host"><kui-hello id="hello" name="KUI"></kui-hello></div>
<div class="row"><input id="name" value="Codex"><button id="primary">Run live action</button><button id="secondary">Reset / reconnect</button></div>
<div id="output" class="status">Initializing extension…</div><div id="visual"></div></div></body></html>)HTML";
    doc->loadHTML(html.str());

    auto* manager = doc->extensionManager();
    auto setOutput = [doc](const std::string& text) {
        if (auto* out = doc->getElementById("output")) out->setTextContent(text);
    };
    if (!manager || !manager->isExtensionLoaded("kui-hello")) {
        setOutput("ERROR: kui_ext_hello was not loaded from\n" + extDir);
        return Platform::run();
    }
    setOutput("Loaded kui_ext_hello from\n" + extDir);

    auto invokeGreet = [doc, manager, setOutput]() {
        auto* hello = doc->getElementById("hello");
        auto* input = doc->getElementById("name");
        if (!hello || !input) return;
        auto result = manager->invoke(hello, "greet", {ExtensionValue::string(input->getAttribute("value"))});
        setOutput(result && !result.values.empty() ? result.values[0].stringValue
                                                   : "invoke(greet) failed: " + result.error);
    };
    doc->getElementById("primary")->addEventListener("click", [=](Event&) {
        auto* host = doc->getElementById("host");
        auto* hello = doc->getElementById("hello");
        if (id == 2 || id == 4) {
            if (hello) host->removeChild(hello);
            auto replacement = doc->createElement("kui-hello");
            replacement->setAttribute("id", "hello");
            replacement->setAttribute("name", id == 4 ? "Reconnected" : "Dynamic tag");
            host->appendChild(replacement);
            setOutput("Custom element constructed and connected through the registry.");
        } else if (id == 3) {
            hello->setAttribute("name", doc->getElementById("name")->getAttribute("value"));
            setOutput("name attribute changed; extension repaint scheduled.");
        } else if (id == 6) {
            invokeGreet();
            doc->getElementById("visual")->setInnerHTML("<div class='toast'>Notification content came from kui_ext_hello::greet().</div>");
        } else if (id == 7) {
            std::string rows = "<table><tr><th>Input</th><th>Extension result</th></tr>";
            for (const char* name : {"Ada", "Linus", "Grace"}) {
                auto result = manager->invoke(hello, "greet", {ExtensionValue::string(name)});
                rows += "<tr><td>" + std::string(name) + "</td><td>" +
                        (result && !result.values.empty() ? result.values[0].stringValue : result.error) + "</td></tr>";
            }
            doc->getElementById("visual")->setInnerHTML(rows + "</table>");
            setOutput("Three typed C ABI calls completed.");
        } else {
            invokeGreet();
        }
    });
    doc->getElementById("secondary")->addEventListener("click", [=](Event&) {
        if (auto* hello = doc->getElementById("hello")) hello->setAttribute("name", "KUI");
        manager->resetPerfStats();
        setOutput("Extension state reset; paint/tick counters continue live.");
    });

    Platform::setInterval([=]() {
        auto* hello = doc->getElementById("hello");
        if (!hello) return;
        const auto frame = manager->invoke(hello, "getFrameCount");
        const auto perf = manager->getPerfStats("kui-hello");
        const long long count = frame && !frame.values.empty() ? frame.values[0].intValue : -1;
        if (id == 8 || id == 9) {
            std::string bars = "<div class='bars'>";
            for (int i = 0; i < 12; ++i) {
                const int height = 12 + static_cast<int>((count * (i + 3) * 7) % 128);
                bars += "<div class='bar' style='height:" + std::to_string(height) + "px'></div>";
            }
            doc->getElementById("visual")->setInnerHTML(bars + "</div>");
        }
        if (id == 1 || id == 8 || id == 9) {
            setOutput("loaded=1  frameCount=" + std::to_string(count) +
                      "  ticks=" + std::to_string(perf.tickCount) +
                      "  paints=" + std::to_string(perf.paintCount));
        }
    }, 250);
    return Platform::run();
}

} // namespace kui::examples
