// =============================================================================
// wrapper_demo.cpp — the C++ wrapper driving a real window
//
// The point of this file is what is NOT in its build settings. It is compiled
// with the STATIC DEBUG CRT (/MTd) against a kui.dll built with the dynamic
// Release CRT (/MD) -- a combination the native C++ SDK rejects outright, since
// its ABI guard raises C1189 for a static CRT and a /MDd consumer segfaults on
// the first std::string that crosses.
//
// Here it just works, and the code still looks like ordinary C++: RAII handles,
// std::string, std::function callbacks. The C ABI sits underneath, invisible.
//
// Quits itself after a few seconds so it can run unattended.
// =============================================================================
#include <kui_cpp.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

const char* kHtml =
"<!DOCTYPE html><html><head><style>"
"  * { margin: 0; padding: 0; box-sizing: border-box; }"
"  body { display: flex; flex-direction: column; align-items: center;"
"         justify-content: center; height: 100vh;"
"         background: linear-gradient(135deg, #123 0%, #2b1a40 100%);"
"         font-family: 'Segoe UI', sans-serif; color: #eef; }"
"  h1 { font-size: 30px; }"
"  #status { margin-top: 18px; font-size: 14px; color: #8fa; }"
"  #hint { margin-top: 8px; font-size: 12px; color: #789; }"
"</style></head><body>"
"  <h1 id='title'>C++ wrapper over a C ABI</h1>"
"  <div id='status'>waiting</div>"
"  <div id='hint'>compiled with a CRT the DLL does not use</div>"
"</body></html>";

void reportBuildConfig() {
#if defined(_DLL)
    std::printf("  CRT linkage : dynamic (/MD or /MDd)\n");
#else
    std::printf("  CRT linkage : STATIC (/MT or /MTd)\n");
#endif
#if defined(_DEBUG)
    std::printf("  CRT flavour : DEBUG\n");
#else
    std::printf("  CRT flavour : release\n");
#endif
#if defined(_ITERATOR_DEBUG_LEVEL)
    std::printf("  _ITERATOR_DEBUG_LEVEL = %d\n", _ITERATOR_DEBUG_LEVEL);
#endif
    std::printf("  sizeof(std::string) = %zu\n", sizeof(std::string));
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    int seconds = 4;
    if (argc > 1) seconds = std::atoi(argv[1]);

    const KuiVersion ver = kui_get_version();
    std::printf("KUI %d.%d.%d through the C++ wrapper\n", ver.major, ver.minor, ver.patch);
    reportBuildConfig();

    try {
        kui::Platform::init();
        // Shut the platform down only after every window / document handle below has been
        // destroyed (they are declared later, so they unwind first) -- the tidy order on every
        // platform. Before 3.61.8 it was also required on Linux: shutdown closed the X connection
        // that a live window's GL context still needed for its own teardown (fixed by
        // T-DEBT-LINUX-PLATFORM-SHUTDOWN-WITH-LIVE-WINDOW-CRASH; any order is safe now).
        struct PlatformShutdown {
            ~PlatformShutdown() {
                try {
                    kui::Platform::shutdown();
                } catch (const kui::Error&) {
                }
            }
        } platformShutdown;

        kui::WindowConfig cfg;
        cfg.title    = "KUI C++ wrapper demo";
        cfg.width    = 720;
        cfg.height   = 460;
        cfg.centered = true;

        kui::Window window = kui::Window::create(cfg);

        // The window owns its document; this handle holds a reference to it.
        kui::Document doc = window.document();
        doc.loadHTML(kHtml);

        kui::Element title  = doc.getElementById("title");
        kui::Element status = doc.getElementById("status");
        if (!title || !status) {
            std::printf("FAILED: expected elements missing\n");
            return 1;
        }
        std::printf("\ntitle text  : \"%s\"\n", title.textContent().c_str());
        std::printf("title tag   : <%s>\n", title.tagName().c_str());

        // A std::function listener. The closure lives in the returned handle;
        // let the handle die and the listener is gone.
        int clicks = 0;
        kui::ListenerHandle listener = title.addEventListener(
            "click", [&clicks, &status](kui::Event& ev) {
                ++clicks;
                kui::Element target = ev.target();
                status.setTextContent("clicked " + std::to_string(clicks) + " time(s) on <" +
                                      target.tagName() + ">");
            });

        // Drive it programmatically so the demo needs no human.
        title.dispatchEvent("click");
        title.dispatchEvent("click");
        std::printf("listener saw : %d clicks\n", clicks);
        std::printf("status text  : \"%s\"\n", status.textContent().c_str());

        // Inline style through the wrapper.
        title.setStyleProperty("color", "#7fd");
        std::printf("title color  : %s\n", title.styleProperty("color").c_str());

        window.show();

        // Repeating timer updating the DOM, closure owned by the handle.
        int ticks = 0;
        kui::TimerHandle tick = kui::Platform::setInterval([&ticks, &status]() {
            ++ticks;
            status.setTextContent("tick " + std::to_string(ticks));
        }, 1000);

        kui::TimerHandle quitTimer;
        if (seconds > 0) {
            std::printf("\nwill quit on its own after %d seconds\n", seconds);
            quitTimer = kui::Platform::setTimeout(
                []() { kui::Platform::quit(0); },
                static_cast<unsigned>(seconds) * 1000u);
        } else {
            std::printf("\nclose the window to quit\n");
        }

        const int code = kui::Platform::run();
        std::printf("event loop returned %d, ticks=%d\n", code, ticks);

        // Everything unwinds in reverse: timers cleared, listener unregistered,
        // document reference dropped, window destroyed -- all by destructors --
        // and finally the platform itself (platformShutdown above).
    } catch (const kui::Error& e) {
        std::printf("FAILED: kui::Error(%d) %s\n", static_cast<int>(e.status()), e.what());
        return 1;
    }

    std::printf("\nPASS\n");
    return 0;
}
