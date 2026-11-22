#include <kui/kui.h>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Event Handling",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { padding: 24px; font-family: "Segoe UI", sans-serif; background: #0f172a; color: #f8fafc; }
h2 { margin-bottom: 16px; }
.target { width: 300px; height: 200px; background: #1e293b; border: 2px solid #334155;
          border-radius: 12px; display: flex; align-items: center; justify-content: center;
          font-size: 14px; color: #94a3b8; cursor: pointer; transition: all 0.2s; margin-bottom: 16px; }
.target:hover { border-color: #3b82f6; background: #334155; }
#log { background: #1e293b; border-radius: 8px; padding: 12px; height: 200px;
       overflow-y: auto; font-family: monospace; font-size: 12px; }
.log-entry { padding: 2px 0; color: #94a3b8; }
.log-entry .type { color: #3b82f6; font-weight: 600; }
.log-entry .pos { color: #10b981; }
</style></head><body>
<h2>Event Handling</h2>
<div class="target" id="target">Click, hover, or type here</div>
<div id="log"></div>
</body></html>
    )");

    auto* target = doc->getElementById("target");
    auto* log = doc->getElementById("log");
    int eventCount = 0;

    auto addLog = [doc, log, &eventCount](const std::string& type, const std::string& detail) {
        auto entry = doc->createElement("div");
        entry->setClassName("log-entry");
        entry->setInnerHTML("<span class=\"type\">" + type + "</span> " + detail);
        log->appendChild(entry);
        log->setScrollTop(log->scrollHeight());
        ++eventCount;
    };

    target->addEventListener("click", [addLog](kui::Event& e) {
        auto& me = static_cast<kui::MouseEvent&>(e);
        addLog("click", "x=" + std::to_string((int)me.clientX()) + " y=" + std::to_string((int)me.clientY()));
    });

    target->addEventListener("mouseenter", [addLog](kui::Event& e) {
        addLog("mouseenter", "");
    });

    target->addEventListener("mouseleave", [addLog](kui::Event& e) {
        addLog("mouseleave", "");
    });

    return kui::Platform::run();
}
