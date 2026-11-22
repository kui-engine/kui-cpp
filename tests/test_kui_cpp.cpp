// =============================================================================
// test_kui_cpp.cpp — wrapper regression tests
//
// These assert on the things the wrapper adds on top of the C ABI: RAII, value
// semantics, closure ownership, and the absent-vs-error distinction. The C layer
// itself is covered by sdk/c/tests/test_kui_c_abi.c.
//
// Headless: no window is created, so this runs in CI.
// =============================================================================
#include <kui_cpp.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool cond, const char* what, int line) {
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("  FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(cond, what) check((cond), (what), __LINE__)

const char* kHtml =
    "<html><body>"
    "  <h1 id='title' class='greeting'>Hello</h1>"
    "  <p id='para'>text</p>"
    "</body></html>";

kui::Document makeDoc() {
    kui::Document doc = kui::Document::create();
    doc.loadHTML(kHtml);
    return doc;
}

// --- handles and value semantics ---------------------------------------------
void testHandleSemantics() {
    std::printf("[handle semantics]\n");

    kui::Document doc = makeDoc();
    CHECK(static_cast<bool>(doc), "document handle truthy");

    kui::Element title = doc.getElementById("title");
    CHECK(static_cast<bool>(title), "found element is truthy");
    CHECK(title.tagName() == "h1", "tagName reads through");

    // A lookup that finds nothing is an empty handle, not an exception.
    kui::Element missing = doc.getElementById("nope");
    CHECK(!missing, "absent element is falsy");
    CHECK(!static_cast<bool>(missing), "absent element explicitly falsy");

    // Copy shares the underlying element (reference counted).
    kui::Element copy = title;
    CHECK(static_cast<bool>(copy), "copy is truthy");
    CHECK(copy.raw() == title.raw(), "copy refers to the same element");
    copy.setAttribute("data-copy", "1");
    CHECK(title.getAttribute("data-copy") == "1", "mutation through the copy is visible");

    // Move leaves the source empty and does not disturb the target.
    kui::Element moved = std::move(copy);
    CHECK(static_cast<bool>(moved), "moved-to is truthy");
    CHECK(!copy, "moved-from is empty");
    CHECK(moved.tagName() == "h1", "moved handle still usable");

    // Self-assignment must not destroy the handle.
    kui::Element& ref = title;
    title = ref;
    CHECK(static_cast<bool>(title), "survives self-assignment");
    CHECK(title.tagName() == "h1", "usable after self-assignment");

    // reset() releases explicitly.
    moved.reset();
    CHECK(!moved, "reset clears the handle");

    // Elements outlive the document handle, because each holds a reference.
    kui::Element keep = doc.getElementById("para");
    doc.reset();
    CHECK(!doc, "document handle cleared");
    CHECK(keep.tagName() == "p", "element outlives the document handle");
}

// --- strings ------------------------------------------------------------------
void testStrings() {
    std::printf("[strings]\n");

    kui::Document doc = makeDoc();
    kui::Element  title = doc.getElementById("title");

    CHECK(title.textContent() == "Hello", "text round-trips");
    title.setTextContent("Goodbye");
    CHECK(title.textContent() == "Goodbye", "text after set");

    // Empty is distinguishable from absent via hasAttribute, matching the native
    // API's shape.
    CHECK(title.getAttribute("class") == "greeting", "attribute value");
    CHECK(title.getAttribute("nope").empty(), "absent attribute reads empty");
    CHECK(!title.hasAttribute("nope"), "absent attribute reports absent");
    title.setAttribute("data-empty", "");
    CHECK(title.hasAttribute("data-empty"), "empty-valued attribute is present");
    CHECK(title.getAttribute("data-empty").empty(), "empty-valued attribute reads empty");

    // A long value exercises the two-call sizing path past any small-buffer
    // shortcut, and the exact-fit boundary inside it.
    const std::string longText(5000, 'x');
    title.setTextContent(longText);
    CHECK(title.textContent() == longText, "5000-byte text round-trips");
    CHECK(title.textContent().size() == 5000, "length preserved");

    // UTF-8 passes through byte for byte.
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87 ok";
    title.setTextContent(utf8);
    CHECK(title.textContent() == utf8, "UTF-8 preserved");

    // Inline style values come back serialized, not as written.
    title.setStyleProperty("color", "red");
    CHECK(title.styleProperty("color") == "#ff0000", "style value is serialized");
    CHECK(title.styleProperty("width").empty(), "unset style property reads empty");
}

// --- listener ownership -------------------------------------------------------
void testListeners() {
    std::printf("[listeners]\n");

    kui::Document doc = makeDoc();
    kui::Element  title = doc.getElementById("title");

    int  calls = 0;
    std::string seenType;
    std::string seenTag;

    {
        kui::ListenerHandle h = title.addEventListener(
            "click", [&](kui::Event& ev) {
                ++calls;
                seenType = ev.type();
                kui::Element t = ev.target();
                if (t) seenTag = t.tagName();
            });
        CHECK(static_cast<bool>(h), "listener handle truthy");

        title.dispatchEvent("click");
        CHECK(calls == 1, "closure invoked");
        CHECK(seenType == "click", "event type visible in closure");
        CHECK(seenTag == "h1", "event target resolved in closure");

        title.dispatchEvent("click");
        CHECK(calls == 2, "invoked again");
    }
    // Handle destroyed: the listener must be gone and the closure freed.
    title.dispatchEvent("click");
    CHECK(calls == 2, "listener unregistered when its handle died");

    // Moving a handle transfers the registration rather than cancelling it.
    {
        int moved_calls = 0;
        kui::ListenerHandle a = title.addEventListener(
            "custom", [&](kui::Event&) { ++moved_calls; });
        kui::ListenerHandle b = std::move(a);
        CHECK(!a, "moved-from handle is inert");
        CHECK(static_cast<bool>(b), "moved-to handle is armed");
        title.dispatchEvent("custom");
        CHECK(moved_calls == 1, "registration survived the move");
    }
    {
        int after = 0;
        kui::ListenerHandle h = title.addEventListener(
            "custom", [&](kui::Event&) { ++after; });
        h.reset();
        title.dispatchEvent("custom");
        CHECK(after == 0, "reset unregisters");
    }

    // preventDefault is observable through the event.
    {
        bool sawPrevented = false;
        kui::ListenerHandle h = title.addEventListener("cancelme", [&](kui::Event& ev) {
            ev.preventDefault();
            sawPrevented = ev.defaultPrevented();
        });
        title.dispatchEvent("cancelme");
        CHECK(sawPrevented, "preventDefault visible via defaultPrevented");
    }

    // A throwing closure must not escape into the C ABI.
    {
        int reached = 0;
        kui::ListenerHandle h = title.addEventListener("boom", [&](kui::Event&) {
            ++reached;
            throw std::runtime_error("from a listener");
        });
        title.dispatchEvent("boom");   // must not propagate
        CHECK(reached == 1, "throwing closure ran");
        CHECK(true, "exception did not escape the trampoline");
    }

    // Probing a non-mouse event returns false rather than throwing.
    {
        bool mouseReported = true;
        kui::ListenerHandle h = title.addEventListener("plain", [&](kui::Event& ev) {
            float x = 0, y = 0;
            mouseReported = ev.mousePosition(x, y);
        });
        title.dispatchEvent("plain");
        CHECK(!mouseReported, "mousePosition declines a non-mouse event");
    }

    // Many listeners at once, each with its own closure state.
    {
        std::vector<kui::ListenerHandle> handles;
        std::vector<int>                 counters(10, 0);
        for (std::size_t i = 0; i < counters.size(); ++i) {
            handles.push_back(title.addEventListener(
                "multi", [&counters, i](kui::Event&) { ++counters[i]; }));
        }
        title.dispatchEvent("multi");
        bool all = true;
        for (int c : counters) {
            if (c != 1) all = false;
        }
        CHECK(all, "ten independent closures each fired once");
    }
}

// --- geometry and errors ------------------------------------------------------
void testGeometryAndErrors() {
    std::printf("[geometry & errors]\n");

    kui::Document doc = makeDoc();
    kui::Element  title = doc.getElementById("title");

    // Headless: zeroes are correct, so only assert the call and sane values.
    const KuiRect r = title.boundingRect();
    CHECK(r.width >= 0.0f && r.height >= 0.0f, "bounding rect not negative");

    // Operating on an empty handle is a programming error and throws, rather
    // than silently doing nothing.
    bool threw = false;
    KuiStatus status = KUI_OK;
    try {
        kui::Element empty;
        (void)empty.tagName();
    } catch (const kui::Error& e) {
        threw = true;
        status = e.status();
    }
    CHECK(threw, "empty handle throws kui::Error");
    CHECK(status == KUI_ERR_INVALID_ARG, "error carries the KuiStatus");
    CHECK(std::string(kui_status_message(status)) == "invalid argument",
          "status message available");
}

// --- timers -------------------------------------------------------------------
// Callbacks only fire while the event loop runs, so this covers registration and
// handle lifetime rather than firing. Timer firing is exercised by
// examples/wrapper_demo.cpp, which runs a real loop.
void testTimers() {
    std::printf("[timers]\n");

    if (kui_platform_init() != KUI_OK) {
        std::printf("    (platform unavailable here -- skipping)\n");
        return;
    }

    {
        int fired = 0;
        kui::TimerHandle t = kui::Platform::setInterval([&fired]() { ++fired; }, 10000);
        CHECK(static_cast<bool>(t), "timer handle armed");
        t.reset();
        CHECK(!t, "reset disarms");
    }
    {
        kui::TimerHandle a = kui::Platform::setTimeout([]() {}, 10000);
        kui::TimerHandle b = std::move(a);
        CHECK(!a, "moved-from timer inert");
        CHECK(static_cast<bool>(b), "moved-to timer armed");
    }
    {
        // release() intentionally leaks the closure so a fire-and-forget timeout
        // can outlive its handle; the alternative is a use-after-free.
        kui::TimerHandle t = kui::Platform::setTimeout([]() {}, 10000);
        t.release();
        CHECK(!t, "released handle no longer owns the timer");
    }

    kui_platform_shutdown();
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("KUI C++ wrapper tests\n\n");

    try {
        testHandleSemantics();
        testStrings();
        testListeners();
        testGeometryAndErrors();
        testTimers();
    } catch (const kui::Error& e) {
        std::printf("\nUNCAUGHT kui::Error(%d): %s\n", static_cast<int>(e.status()), e.what());
        ++g_failed;
    }

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
