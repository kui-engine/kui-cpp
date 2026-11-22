#include <kui/kui.h>
#include <string>

int main() {
    kui::Platform::init();
    auto window = kui::Window::create({.title = "KUI — Timer & Intervals", .width = 600, .height = 450});
    auto* doc = window->document();
    doc->loadHTML(R"(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{padding:24px;font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc}
h2{margin-bottom:16px}
.card{background:#1e293b;border-radius:12px;padding:24px;margin-bottom:16px}
.card h3{font-size:14px;margin-bottom:12px}
.counter{font-size:48px;font-weight:700;color:#3b82f6;text-align:center;padding:16px}
.bar-container{height:8px;background:#334155;border-radius:4px;overflow:hidden;margin:12px 0}
.bar-fill{height:100%;background:#10b981;border-radius:4px;width:0%;transition:width 0.1s}
.log{background:#0f172a;padding:8px 12px;border-radius:6px;font-family:"Consolas",monospace;font-size:12px;color:#94a3b8;max-height:120px;overflow-y:auto;margin-top:8px}
.btn-row{display:flex;gap:8px;justify-content:center;margin-top:12px}
.btn{padding:8px 20px;border:none;border-radius:8px;font-size:13px;cursor:pointer;font-weight:600}
.bp{background:#3b82f6;color:white}.br{background:#ef4444;color:white}.bg{background:#10b981;color:white}
</style></head><body>
<h2>Timer &amp; Intervals</h2>
<div class="card"><h3>setInterval Counter (100ms)</h3>
<div class="counter" id="counter">0</div>
<div class="bar-container"><div class="bar-fill" id="bar"></div></div>
<div class="btn-row">
<button class="btn bp" id="startBtn">Start</button>
<button class="btn br" id="stopBtn">Stop</button>
<button class="btn bg" id="resetBtn">Reset</button>
</div></div>
<div class="card"><h3>Event Log</h3><div class="log" id="log">Ready...</div></div>
</body></html>
    )");

    int count = 0;
    kui::u32 timerId = 0;
    auto* counter = doc->getElementById("counter");
    auto* bar = doc->getElementById("bar");
    auto* log = doc->getElementById("log");

    auto updateUI = [counter, bar, &count]() {
        counter->setTextContent(std::to_string(count));
        int pct = count % 100;
        bar->style()->setProperty("width", std::to_string(pct) + "%");
    };

    doc->getElementById("startBtn")->addEventListener("click", [&timerId, &count, updateUI, log](kui::Event&) {
        if (timerId) return;
        log->setTextContent("Timer started");
        timerId = kui::Platform::setInterval([&count, updateUI]() {
            ++count;
            updateUI();
        }, 100);
    });

    doc->getElementById("stopBtn")->addEventListener("click", [&timerId, log](kui::Event&) {
        if (timerId) {
            kui::Platform::clearTimer(timerId);
            timerId = 0;
            log->setTextContent("Timer stopped");
        }
    });

    doc->getElementById("resetBtn")->addEventListener("click", [&count, &timerId, updateUI, log](kui::Event&) {
        if (timerId) {
            kui::Platform::clearTimer(timerId);
            timerId = 0;
        }
        count = 0;
        updateUI();
        log->setTextContent("Timer reset");
    });

    return kui::Platform::run();
}
