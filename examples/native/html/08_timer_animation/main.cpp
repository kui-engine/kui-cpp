#include <kui/kui.h>
#include <chrono>
#include <ctime>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Timer Animation",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { display: flex; flex-direction: column; align-items: center; justify-content: center;
       height: 100vh; background: #0f172a; font-family: "Segoe UI", sans-serif; }
#clock { font-size: 72px; font-weight: 700; color: #3b82f6; margin-bottom: 16px; }
#date { font-size: 18px; color: #94a3b8; margin-bottom: 32px; }
.progress-bar { width: 400px; height: 8px; background: #1e293b; border-radius: 4px; overflow: hidden; }
.progress-fill { height: 100%; background: linear-gradient(90deg, #3b82f6, #8b5cf6);
                 border-radius: 4px; transition: width 0.3s ease; }
#counter { margin-top: 24px; font-size: 16px; color: #64748b; }
</style></head><body>
<div id="clock">00:00:00</div>
<div id="date">Loading...</div>
<div class="progress-bar"><div class="progress-fill" id="progress" style="width:0%"></div></div>
<div id="counter">Ticks: 0</div>
</body></html>
    )");

    auto* clock = doc->getElementById("clock");
    auto* date = doc->getElementById("date");
    auto* progress = doc->getElementById("progress");
    auto* counter = doc->getElementById("counter");
    int ticks = 0;

    kui::Platform::setInterval([clock, date, progress, counter, &ticks]() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        char timeBuf[16], dateBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_buf);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %A", &tm_buf);

        clock->setTextContent(timeBuf);
        date->setTextContent(dateBuf);

        int sec = tm_buf.tm_sec;
        int pct = (sec * 100) / 59;
        progress->style()->setProperty("width", std::to_string(pct) + "%");

        ++ticks;
        counter->setTextContent("Ticks: " + std::to_string(ticks));
    }, 1000);

    return kui::Platform::run();
}
