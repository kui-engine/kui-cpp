#include <kui/kui.h>

#include <iomanip>
#include <sstream>

int main() {
    if (!kui::Platform::init()) return 1;
    auto window = kui::Window::create({.title = "KUI — System Info", .width = 680, .height = 540});
    if (!window) return 2;
    const auto primary = kui::Platform::primaryMonitor();
    const auto physical = kui::Platform::primaryMonitorPhysicalSize();
    std::ostringstream scale;
    scale << std::fixed << std::setprecision(2) << primary.scale << "×";
    std::ostringstream html;
    html << R"HTML(<!doctype html><html><head><style>
*{box-sizing:border-box}body{margin:0;padding:26px;font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc}h2{margin:0 0 16px}.card{background:#1e293b;border-radius:12px;padding:20px;margin-bottom:14px}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.item{background:#0b1220;padding:13px;border-radius:8px}.label{font-size:11px;color:#94a3b8;text-transform:uppercase}.value{font-size:16px;color:#60a5fa;margin-top:5px;word-break:break-all}</style></head><body><h2>Live System Information</h2><div class="card"><div class="grid">)HTML";
    auto add = [&html](const char* label, const std::string& value) {
        html << "<div class='item'><div class='label'>" << label
             << "</div><div class='value'>" << value << "</div></div>";
    };
    add("Operating system", kui::Platform::osName() + " " + kui::Platform::osVersion());
    add("KUI version", kui::kVersionString);
    add("Primary monitor", primary.name);
    add("Logical resolution", std::to_string(static_cast<int>(primary.bounds.width)) + " × " +
                              std::to_string(static_cast<int>(primary.bounds.height)));
    add("Physical resolution", std::to_string(static_cast<int>(physical.width)) + " × " +
                               std::to_string(static_cast<int>(physical.height)));
    add("DPI scale", scale.str());
    add("Monitor count", std::to_string(kui::Platform::monitors().size()));
    add("Window DPR", std::to_string(window->devicePixelRatio()));
    html << "</div></div><div class='card'>All values above are queried from the current runtime; no hardware or version fields are hard-coded.</div></body></html>";
    window->document()->loadHTML(html.str());
    return kui::Platform::run();
}
