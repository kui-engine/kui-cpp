#include <kui/kui.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <chrono>
#include <iostream>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
int passed = 0;
int failed = 0;
#define CHECK(condition, message)                                              \
    do {                                                                       \
        if (condition) ++passed;                                               \
        else { ++failed; std::cerr << "FAIL: " << message << '\n'; }          \
    } while (false)
}

int main() {
    static_assert(std::is_same_v<decltype(kui::Window::create()),
                                 kui::Unique<kui::Window>>);
    static_assert(std::is_same_v<kui::HTMLMediaElement, kui::Element>);
    static_assert(std::is_same_v<kui::HTMLLottieElement, kui::Element>);

    auto document = kui::Document::create();
    int connected = 0;
    int changed = 0;
    kui::CustomElementDefinition definition;
    definition.tagName = "x-wrapper-full";
    definition.factory = [](kui::Document* owner) {
        return kui::Element::create(owner, "x-wrapper-full");
    };
    definition.connectedCallback = [&](kui::Element*) { ++connected; };
    definition.attributeChangedCallback =
        [&](kui::Element*, const kui::String&, const kui::String&, const kui::String&) {
            ++changed;
        };
    definition.observedAttributes = {"watched"};
    CHECK(document->customElements()->define("x-wrapper-full", std::move(definition)),
          "custom element definition");
    CHECK(document->customElements()->isDefined("x-wrapper-full"),
          "custom element lookup");

    document->loadHTML(
        "<html><body><x-wrapper-full id='custom'></x-wrapper-full>"
        "<audio id='media'></audio><lottie-player id='lottie'></lottie-player>"
        "<div id='list'></div></body></html>");
    CHECK(connected > 0, "custom connected callback");
    auto* custom = document->getElementById("custom");
    custom->setAttribute("ignored", "x");
    CHECK(changed == 0, "observed attribute filtering");
    custom->setAttribute("watched", "yes");
    CHECK(changed == 1, "custom attribute callback");

    auto engine = kui::ScriptEngine::create();
    CHECK(engine != nullptr, "Lua engine create");
    if (engine) {
        engine->bindDocument(document.get());
        CHECK(engine->eval("wrapper_full_value = 6 * 7", "wrapper-full").success,
              "Lua eval");
        const auto invalid = engine->eval("this is not valid lua !!!", "wrapper-full-invalid");
        CHECK(!invalid.success && !invalid.error.empty(), "Lua diagnostic");
        engine->unbindDocument();
    }

    auto* media = dynamic_cast<kui::HTMLMediaElement*>(document->getElementById("media"));
    CHECK(media != nullptr, "media alias cast");
    if (media) {
        CHECK(media->readyState() >= 0 && media->readyState() <= 4, "media readyState");
        CHECK(media->networkState() >= 0 && media->networkState() <= 3,
              "media networkState");
    }
    auto* lottie = dynamic_cast<kui::HTMLLottieElement*>(document->getElementById("lottie"));
    CHECK(lottie != nullptr, "Lottie alias cast");
    if (lottie) (void)lottie->complete();

    kui::VirtualList list;
    kui::VirtualListConfig listConfig;
    listConfig.totalItems = 10;
    listConfig.itemHeight = 24;
    listConfig.renderItem = [](kui::Document* owner, kui::u32 index) {
        auto item = owner->createElement("div");
        item->setTextContent(std::to_string(index));
        return item;
    };
    list.setup(document->getElementById("list"), listConfig);
    CHECK(list.renderedCount() > 0, "VirtualList renders a visible range");

    document->initExtensions("__kui_missing_extensions_for_test__");
    CHECK(document->extensionManager() != nullptr, "extension manager bridge");
    CHECK(!document->extensionManager()->isExtensionLoaded("x-missing"),
          "missing extension query");

    kui::HttpClient http;
    http.setUserAgent("kui-wrapper-full-test");
    http.cancelAll();

    // Main-thread hand-off: posts from other threads run on this thread, inside
    // the event loop; fetchAsyncOnMainThread delivers here as well.
    CHECK(kui::Platform::init(), "platform init for main-thread hand-off");
    const auto mainThread = std::this_thread::get_id();
    std::atomic<int> posted{0};
    std::atomic<bool> offThread{false};
    std::vector<std::thread> posters;
    for (int t = 0; t < 4; ++t) {
        posters.emplace_back([&] {
            for (int i = 0; i < 16; ++i) {
                kui::Platform::postToMainThread([&] {
                    if (std::this_thread::get_id() != mainThread) offThread = true;
                    ++posted;
                });
            }
        });
    }
    for (auto& poster : posters) poster.join();
    for (int i = 0; i < 500 && posted < 64; ++i) kui::Platform::waitEvents(0.01);
    CHECK(posted == 64, "postToMainThread runs every cross-thread post");
    const kui::f64 before = kui::Platform::now();
    kui::Platform::waitEvents(0.01);
    CHECK(kui::Platform::now() >= before && before > 0, "Platform::now is monotonic");
    const auto primary = kui::Platform::primaryMonitor();
    CHECK(kui::Platform::monitorFromPoint(primary.bounds.x + 1, primary.bounds.y + 1).isPrimary,
          "monitorFromPoint finds the primary monitor");
    CHECK(!offThread, "postToMainThread runs on the main thread");

    std::thread::id httpThread;
    bool httpCalled = false;
    http.fetchAsyncOnMainThread("not-a-url", [&](kui::HttpClient::Response response) {
        httpCalled = !response.error.empty();
        httpThread = std::this_thread::get_id();
    }, 1);
    for (int i = 0; i < 500 && !httpCalled; ++i) kui::Platform::waitEvents(0.01);
    CHECK(httpCalled && httpThread == mainThread,
          "fetchAsyncOnMainThread calls back on the main thread");

    // A request cancelled by the client's destructor still completes afterwards;
    // its callback state must outlive the client (it used to live inside it).
    std::atomic<bool> lateCallback{false};
    {
        kui::HttpClient shortLived;
        shortLived.fetchAsync("not-a-url", [&](kui::HttpClient::Response) { lateCallback = true; }, 1);
    }
    for (int i = 0; i < 500 && !lateCallback; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(lateCallback, "fetchAsync completes after its client is destroyed");

    // Desktop integration (C ABI section 10): menu bar, tray, shortcuts, toasts, screenshot.
    {
        kui::WindowConfig desktopConfig;
        desktopConfig.title = "wrapper-desktop";
        desktopConfig.width = 320;
        desktopConfig.height = 240;
        auto window = kui::Window::create(desktopConfig);
        CHECK(window != nullptr, "desktop window");
        kui::MenuBarDef menu;
        int opened = 0;
        kui::MenuItemDef openItem("&Open", [&] { ++opened; }, "Ctrl+O");
        openItem.id = 7;
        openItem.checkable = true;
        menu.addMenu("&File", {openItem,
                               kui::MenuItemDef::separator(),
                               kui::MenuItemDef("Quit", [] {})});
        window->setMenuBar(menu);
        window->setMenuItemChecked(7, true);
        window->setMenuItemEnabled(7, false);
        KuiStatus unknownItem = KUI_OK;
        try { window->setMenuItemChecked(4242, true); } catch (const kui::Error& thrown) { unknownItem = thrown.status(); }
        CHECK(unknownItem == KUI_ERR_NOT_FOUND, "an unknown menu item id is NOT_FOUND");
        window->clearMenuBar();

        auto& tray = window->trayIcon();
        kui::TrayIconConfig trayConfig;
        trayConfig.tooltip = "wrapper tray";
        if (tray.create(window.get(), trayConfig)) {
            int clicks = 0, balloons = 0;
            tray.onClick([&] { ++clicks; });
            tray.onBalloonClick([&] { ++balloons; });
            tray.simulateClick();
            tray.simulateBalloonClick();
            CHECK(clicks == 1 && balloons == 1, "tray callbacks");
            tray.setTooltip("updated");
            CHECK(tray.tooltip() == "updated", "tray tooltip");
            tray.setContextMenu(menu);
            CHECK(tray.hasContextMenu(), "tray context menu");
            tray.destroy();
            CHECK(!tray.isActive(), "tray destroyed");
        }

        auto& shortcuts = window->shortcutManager();
        const auto save = shortcuts.bind("Ctrl+S", [] {});
        CHECK(save != 0, "bind shortcut");
        CHECK(shortcuts.bind(kui::Shortcut{"S", kui::KeyModifier::Ctrl}, [] {}) == save,
              "rebinding the same combination keeps its id");
        CHECK(shortcuts.bind("", [] {}) == 0, "empty shortcut rejected");
        shortcuts.unbindAll();

        auto* desktopDoc = window->document();
        desktopDoc->loadHTML("<html><body><p>desktop</p></body></html>");
        for (int i = 0; i < 10; ++i) kui::Platform::pollEvents();
        const auto toast = desktopDoc->toastManager().success("Saved", 0);
        CHECK(toast != 0 && desktopDoc->toastManager().count() == 1, "toast shown");
        desktopDoc->toastManager().dismiss(toast);

        const std::string shot = "kui_wrapper_desktop_shot.png";
        std::remove(shot.c_str());
        CHECK(window->renderer()->saveScreenshot(shot), "saveScreenshot");
        std::remove(shot.c_str());
    }

    // Document style profile (C ABI section 19).
    {
        auto plain = kui::Document::create();
        CHECK(plain->styleProfile() == kui::Document::StyleProfile::NativeGui, "NativeGui by default");
        plain->setStyleProfile(kui::Document::StyleProfile::BrowserCompat);
        CHECK(plain->styleProfile() == kui::Document::StyleProfile::BrowserCompat, "setStyleProfile");
        auto browser = kui::Document::create(kui::Document::StyleProfile::BrowserCompat);
        CHECK(browser->styleProfile() == kui::Document::StyleProfile::BrowserCompat,
              "Document::create(StyleProfile)");
    }

    // Components (C ABI section 18): built-in elements, themes, VirtualList, ListView, TreeView.
    {
        kui::WindowConfig componentConfig;
        componentConfig.title = "wrapper-components";
        componentConfig.width = 640;
        componentConfig.height = 480;
        auto window = kui::Window::create(componentConfig);
        auto* doc = window->document();
        kui::registerBuiltinComponents(doc);
        CHECK(doc->customElements()->isDefined("kui-tabs"), "registerBuiltinComponents");

        kui::ThemeManager themes;
        const auto names = themes.themeNames();
        CHECK(names.size() == 2, "light and dark themes are built in");
        CHECK(!kui::ThemeManager::darkTheme().variables.empty(), "ThemeManager::darkTheme");
        kui::Theme brand;
        brand.variables["--brand-bg"] = "#123456";
        themes.registerTheme("brand", brand);
        doc->loadHTML("<html><body style='margin:0'>"
                      "<div id='vl' style='width:200px;height:100px'></div>"
                      "<div style='position:absolute;left:300px;top:0'><div id='lv' style='width:200px;height:120px'></div></div>"
                      "<div style='position:absolute;left:0;top:200px'><div id='tv' style='width:200px;height:120px'></div></div>"
                      "</body></html>");
        themes.apply("brand", doc);
        themes.apply("missing", doc);  // ignored, as in native
        CHECK(themes.activeTheme() == "brand", "ThemeManager::activeTheme");
        const auto* found = themes.getTheme("brand");
        CHECK(found && found->variables.at("--brand-bg") == "#123456" && !themes.getTheme("nope"),
              "ThemeManager::getTheme");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);

        int focusChanges = 0;
        kui::VirtualList vlist;
        kui::VirtualListConfig vconfig;
        vconfig.totalItems = 1000;
        vconfig.itemHeight = 20;
        vconfig.renderItem = [](kui::Document* owner, kui::u32 index) {
            auto row = owner->createElement("div");
            row->setTextContent("row " + std::to_string(index));
            return row;
        };
        vconfig.onFocusChange = [&](kui::i32, kui::i32) { ++focusChanges; };
        vlist.setup(doc->getElementById("vl"), vconfig);
        CHECK(vlist.renderedCount() > 0 && vlist.renderedCount() < 50 && vlist.totalHeight() == 20000,
              "VirtualList renders only the rows in view");
        vlist.scrollToIndex(500);
        CHECK(vlist.visibleStart() <= 500 && 500 < vlist.visibleEnd(), "VirtualList::scrollToIndex");
        vlist.focusFirst();
        vlist.moveFocus(3);
        CHECK(vlist.focusedIndex() == 3 && focusChanges == 2, "VirtualList focus");
        vlist.insertItems(0, 10);
        CHECK(vlist.totalItems() == 1010, "VirtualList::insertItems");
        vlist.setScrollUnit(kui::ScrollUnit::Line);
        CHECK(vlist.scrollUnit() == kui::ScrollUnit::Line, "VirtualList::setScrollUnit");

        int listSelected = -1;
        kui::ListView list;
        kui::ListViewConfig lconfig;
        lconfig.itemCount = 100;
        lconfig.itemHeight = 30;
        lconfig.selectionMode = kui::ListViewConfig::Multi;
        lconfig.itemProvider = [](kui::u32 index) {
            kui::ListViewItem item;
            item.text = "Item " + std::to_string(index);
            return item;
        };
        lconfig.onSelect = [&](kui::u32 index) { listSelected = static_cast<int>(index); };
        list.setup(doc->getElementById("lv"), lconfig);
        list.select(5);
        list.selectRange(10, 13);
        CHECK(list.selectedIndices() == (std::vector<kui::u32>{5, 10, 11, 12}), "ListView selection");
        list.clearSelection();
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);
        kui_devtools_dispatch_mouse_event(doc->cHandle(), KUI_C_MOUSE_PRESSED, 350, 70, KUI_C_MOUSE_BUTTON_LEFT, 0, 1, 0, 0);
        kui_devtools_dispatch_mouse_event(doc->cHandle(), KUI_C_MOUSE_RELEASED, 350, 70, KUI_C_MOUSE_BUTTON_LEFT, 0, 1, 0, 0);
        CHECK(listSelected == 2 && list.isSelected(2), "ListView click selects through onSelect");
        CHECK(list.totalItems() == 100 && list.itemHeight() == 30.0f && list.totalHeight() == 3000.0f &&
                  list.visibleEnd() > list.visibleStart() && list.visibleEnd() <= 100,
              "ListView visible range, total and row height");

        int toggles = 0;
        std::size_t selectionSize = 0;
        kui::TreeView tree;
        const auto a = tree.addNode("A", "+");
        const auto a1 = tree.addChild(a, "A1");
        tree.addChild(a, "A2");
        const auto b = tree.addNode("B");
        CHECK(tree.node(a).children.size() == 2 && tree.node(a1).parentIndex == static_cast<kui::i32>(a),
              "TreeView::node");
        auto data = tree.node(b);
        data.text = "B2";
        data.userData = &toggles;
        tree.setNode(b, data);
        CHECK(tree.node(b).text == "B2" && tree.node(b).userData == &toggles, "TreeView::setNode");
        tree.setNode(b, [&] { auto copy = tree.node(b); copy.disabled = true; return copy; }());
        kui::TreeView::TreeNodeUpdate update;
        update.text = "B3";
        tree.updateNode(b, update);
        CHECK(tree.node(b).text == "B3" && tree.node(b).disabled, "TreeView::updateNode keeps the fields left unset");
        kui::TreeView::TreeNodeUpdate enable;
        enable.disabled = false;
        tree.updateNode(a, enable);
        CHECK(tree.node(a).text == "A" && tree.node(a).icon == "+" && !tree.node(a).disabled,
              "TreeView::updateNode keeps text and icon");
        update.text = "B2";
        update.disabled = false;
        tree.updateNode(b, update);
        kui::TreeViewConfig tconfig;
        tconfig.multiSelect = true;
        tconfig.onToggle = [&](kui::u32, bool) { ++toggles; };
        tconfig.onSelectionChanged = [&](const std::set<kui::u32>& selected) { selectionSize = selected.size(); };
        tree.setup(doc->getElementById("tv"), tconfig);
        CHECK(tree.visibleRowCount() == 2, "TreeView collapsed rows");
        tree.expand(a);
        CHECK(tree.visibleRowCount() == 4 && toggles == 1, "TreeView::expand");
        tree.select(a1);
        tree.toggleSelect(b);
        CHECK(tree.selectedIndices() == (std::set<kui::u32>{a1, b}) && selectionSize == 2,
              "TreeView multi-select");
        tree.selectAll();
        CHECK(tree.selectionCount() == 4, "TreeView::selectAll");
    }

    // Custom element paint, tick and input (C ABI section 17).
    {
        kui::WindowConfig canvasConfig;
        canvasConfig.title = "wrapper-custom-paint";
        canvasConfig.width = 640;
        canvasConfig.height = 480;
        auto window = kui::Window::create(canvasConfig);
        auto* doc = window->document();
        int paints = 0;
        int ticks = 0;
        bool animating = true;
        bool drew = false;
        bool unsupportedThrows = false;
        kui::f32 paintWidth = 0;
        kui::f32 paintHeight = 0;
        int mouseDowns = 0;
        kui::f32 mouseX = -1;
        kui::f32 mouseY = -1;
        kui::String key;
        kui::String committed;
        kui::f32 wheelDy = 0;
        kui::CustomElementDefinition canvas;
        canvas.tagName = "x-wrapper-canvas";
        canvas.factory = [](kui::Document* owner) { return kui::Element::create(owner, "x-wrapper-canvas"); };
        canvas.paintCallback = [&](kui::CustomElementDefinition::PaintContext& ctx) {
            ++paints;
            paintWidth = ctx.contentWidth;
            paintHeight = ctx.contentHeight;
            auto* dl = ctx.displayList;
            dl->fillRect({0, 0, ctx.contentWidth, ctx.contentHeight}, kui::Color(255, 0, 0));
            dl->save();
            dl->clipRoundedRect({0, 0, 50, 50}, kui::CornerRadii(6));
            dl->translate(2, 2);
            dl->setOpacity(0.5f);
            dl->fillRoundedRect({0, 0, 20, 20}, kui::CornerRadii(4), kui::Color(0, 0, 255));
            dl->strokeRoundedRect({0, 0, 20, 20}, kui::CornerRadii(4), kui::Color(0, 0, 0), 1);
            dl->drawText("hi", {4, 40}, "sans-serif", 12, 700, 0, kui::Color(0, 0, 0));
            dl->drawDecorationLine(0, 45, 40, 1, 4, kui::Color(0, 255, 0));
            dl->restore();
            drew = ctx.element && ctx.element->getAttribute("id") == "wc";
            try {
                dl->drawText("x", {0, 10}, "sans-serif", 12, 400, 0, kui::Color(0, 0, 0), 100);
            } catch (const kui::Error& error) {
                unsupportedThrows = error.status() == KUI_ERR_NOT_SUPPORTED;
            }
        };
        canvas.tickCallback = [&](kui::Element* element, kui::f64) {
            ++ticks;
            if (animating) element->markNeedsPaint();
        };
        canvas.needsTickCallback = [&](kui::Element*) { return animating; };
        canvas.mouseEventCallback = [&](kui::Element*, const kui::CustomElementDefinition::MouseEventData& e) {
            if (e.eventType == 0) {
                ++mouseDowns;
                mouseX = e.x;
                mouseY = e.y;
            }
            return false;
        };
        canvas.wheelEventCallback = [&](kui::Element*, const kui::CustomElementDefinition::WheelEventData& e) {
            wheelDy = e.deltaY;
            return true;
        };
        canvas.keyEventCallback = [&](kui::Element*, const kui::CustomElementDefinition::KeyEventData& e) {
            if (e.eventType == 0) key = e.key;
            return true;
        };
        canvas.imeEventCallback = [&](kui::Element*, const kui::CustomElementDefinition::ImeEventData& e) {
            if (e.eventType == 2) committed = e.committedText;
            return true;
        };
        CHECK(doc->customElements()->define("x-wrapper-canvas", std::move(canvas)),
              "define a custom element with paint / tick / input");
        doc->loadHTML("<html><body style='margin:0'><x-wrapper-canvas id='wc' tabindex='0' "
                      "style='display:block;position:absolute;left:10px;top:10px;width:120px;height:60px'>"
                      "</x-wrapper-canvas></body></html>");
        for (int i = 0; i < 30 && (paints == 0 || ticks < 3); ++i) kui::Platform::waitEvents(0.02);
        CHECK(paints > 0 && paintWidth == 120 && paintHeight == 60, "paintCallback with the content size");
        CHECK(drew, "paint context carries the element");
        CHECK(unsupportedThrows, "drawText layout options are reported as not supported");
        CHECK(ticks >= 3, "tickCallback");
        animating = false;
        for (int i = 0; i < 3; ++i) kui::Platform::waitEvents(0.02);
        const int ticksAfterStop = ticks;
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.02);
        CHECK(ticks == ticksAfterStop, "needsTickCallback false stops the ticks");

        auto* element = doc->getElementById("wc");
        const auto metrics = element->measureText("Hello");
        CHECK(metrics.width > 0 && metrics.ascent > 0, "Element::measureText");
        const int paintsBefore = paints;
        element->invalidateRect({0, 0, 10, 10});
        for (int i = 0; i < 10 && paints == paintsBefore; ++i) kui::Platform::waitEvents(0.02);
        CHECK(paints > paintsBefore, "Element::invalidateRect repaints");

        KuiDocument* raw = doc->cHandle();
        kui_devtools_dispatch_mouse_event(raw, KUI_C_MOUSE_PRESSED, 40, 30, KUI_C_MOUSE_BUTTON_LEFT, 0, 1, 0, 0);
        kui_devtools_dispatch_mouse_event(raw, KUI_C_MOUSE_RELEASED, 40, 30, KUI_C_MOUSE_BUTTON_LEFT, 0, 1, 0, 0);
        CHECK(mouseDowns == 1 && mouseX == 30 && mouseY == 20, "mouseEventCallback in content-box coordinates");
        kui_devtools_dispatch_mouse_event(raw, KUI_C_MOUSE_WHEEL, 40, 30, KUI_C_MOUSE_BUTTON_NONE, 0, 0, 0, 25);
        CHECK(wheelDy == 25, "wheelEventCallback");
        element->focus();
        kui_devtools_dispatch_key_event(raw, KUI_C_KEY_DOWN, "b", 0, "KeyB", 0, 0, "b", 0, 0);
        CHECK(key == "b", "keyEventCallback on the focused element");
        kui_devtools_dispatch_ime_event(raw, KUI_C_IME_START, nullptr, 0);
        kui_devtools_dispatch_ime_event(raw, KUI_C_IME_COMMIT, "ok", 0);
        CHECK(committed == "ok", "imeEventCallback");
    }

    // Script engine control (C ABI section 16): values, functions both ways, globals, print, limits.
    {
        auto lua = kui::ScriptEngine::create();
        CHECK(lua != nullptr, "engine for section 16");
        if (lua) {
            const auto npos = kui::String::npos;
            auto r = lua->eval("return 40 + 2");
            CHECK(r.success && r.value.type == kui::ScriptValue::Int && r.value.asInt() == 42,
                  "eval returns its value");
            r = lua->eval("error('boom')");
            CHECK(!r.success && r.error.find("boom") != npos, "eval error text");
            lua->registerFunction("cxx_add", [](const std::vector<kui::ScriptValue>& args) {
                return kui::ScriptValue::from(args.at(0).asInt() + args.at(1).asInt());
            });
            r = lua->eval("return cxx_add(2, 3)");
            CHECK(r.success && r.value.asInt() == 5, "registerFunction");
            lua->registerFunction("cxx_throw", [](const std::vector<kui::ScriptValue>&) -> kui::ScriptValue {
                throw std::runtime_error("cxx says no");
            });
            r = lua->eval("cxx_throw()");
            CHECK(!r.success && r.error.find("cxx says no") != npos,
                  "a throwing C++ function raises a Lua error");
            lua->registerFunction("cxx_apply", [](const std::vector<kui::ScriptValue>& args) {
                return args.at(0).asFunction()({args.at(1)});
            });
            r = lua->eval("return cxx_apply(function(x) return x .. '!' end, 'hi')");
            CHECK(r.success && r.value.asString() == "hi!", "Lua function called from C++");
            lua->setGlobal("cxx_twice", kui::ScriptValue::fromFunction(
                [](const std::vector<kui::ScriptValue>& args) {
                    return kui::ScriptValue::from(args.at(0).asInt() * 2);
                }));
            r = lua->eval("return cxx_twice(21)");
            CHECK(r.success && r.value.asInt() == 42, "C++ function value as a global");
            lua->registerFunction("cxx_make", [](const std::vector<kui::ScriptValue>&) {
                return kui::ScriptValue::fromFunction([](const std::vector<kui::ScriptValue>&) {
                    return kui::ScriptValue::from("made");
                });
            });
            r = lua->eval("return cxx_make()()");
            CHECK(r.success && r.value.asString() == "made", "C++ function returned from a callback");
            lua->setGlobal("g_text", kui::ScriptValue::from("text"));
            lua->setGlobal("g_ud", kui::ScriptValue::fromUserData(&passed));
            CHECK(lua->getGlobal("g_text").asString() == "text" &&
                  lua->getGlobal("g_ud").asUserData() == &passed, "setGlobal / getGlobal");
            lua->eval("function mul(a, b) return a * b end");
            r = lua->callFunction("mul", {kui::ScriptValue::from(6), kui::ScriptValue::from(7)});
            CHECK(r.success && r.value.asInt() == 42, "callFunction");
            r = lua->callFunction("no_such_fn");
            CHECK(!r.success && r.error.find("no_such_fn") != npos, "callFunction on a missing function");
            const auto mul = lua->getGlobal("mul");
            CHECK(mul.type == kui::ScriptValue::Function &&
                  mul.asFunction()({kui::ScriptValue::from(3), kui::ScriptValue::from(4)}).asInt() == 12,
                  "Lua function value from getGlobal");
            {
                std::ofstream("wrapper_eval_file.lua", std::ios::binary) << "return 'from file'";
                r = lua->evalFile("wrapper_eval_file.lua");
                CHECK(r.success && r.value.asString() == "from file", "evalFile");
                std::remove("wrapper_eval_file.lua");
            }
            kui::String printed;
            lua->setPrintHandler([&](const kui::String& message) { printed = message; });
            lua->eval("print('hi', 1)");
            CHECK(printed == "hi\t1", "setPrintHandler");
            lua->setPrintHandler(nullptr);
            lua->eval("print('quiet')");
            CHECK(printed == "hi\t1", "setPrintHandler(nullptr)");
            CHECK(lua->memoryUsageBytes() > 0 && lua->memoryLimitBytes() == 256u * 1024 * 1024 &&
                  lua->instructionLimit() == 100000000u, "default limits");
            lua->setInstructionLimit(100000);
            r = lua->eval("while true do end");
            CHECK(!r.success && r.error.find("instruction limit") != npos, "setInstructionLimit");
            lua->setMemoryLimit(0);
            CHECK(lua->memoryLimitBytes() == SIZE_MAX && lua->instructionLimit() == 100000,
                  "limits read back (no memory limit reads as SIZE_MAX)");
        }
    }

    // Drag-and-drop data (C ABI section 15): an in-page drag driven by injected mouse input.
    {
        kui::WindowConfig dragConfig;
        dragConfig.title = "wrapper-drag";
        dragConfig.width = 640;
        dragConfig.height = 480;
        auto window = kui::Window::create(dragConfig);
        auto* doc = window->document();
        doc->loadHTML(
            "<html><body style='margin:0'>"
            "<div id='src' draggable='true' style='position:absolute;left:10px;top:10px;width:80px;height:80px'>s</div>"
            "<div id='dst' style='position:absolute;left:200px;top:10px;width:80px;height:80px'>d</div>"
            "</body></html>");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);
        auto* source = doc->getElementById("src");
        auto* target = doc->getElementById("dst");
        int started = 0;
        int drops = 0;
        source->addEventListener("dragstart", [&](kui::Event& e) {
            ++started;
            auto* dt = static_cast<kui::DragEvent&>(e).dataTransfer();
            dt->setData("text/plain", "hello drag");
            dt->setData("application/x-kui", "42");
            dt->setEffectAllowed(kui::EffectAllowed::Copy);
        });
        target->addEventListener("dragover", [](kui::Event& e) {
            e.preventDefault();
            e.dataTransfer()->setDropEffect(kui::DropEffect::Copy);
        });
        kui::String data;
        kui::String missing = "x";
        bool hasMissing = true;
        std::vector<kui::String> types;
        std::vector<kui::String> afterOne;
        std::vector<kui::String> afterAll = {"x"};
        kui::EffectAllowed allowed = kui::EffectAllowed::None;
        kui::DropEffect effect = kui::DropEffect::None;
        target->addEventListener("drop", [&](kui::Event& e) {
            ++drops;
            auto* dt = e.dataTransfer();
            data = dt->getData("text/plain");
            missing = dt->getData("missing/type");
            hasMissing = dt->hasData("missing/type");
            types = dt->types();
            allowed = dt->effectAllowed();
            effect = dt->dropEffect();
            dt->clearData("application/x-kui");
            afterOne = dt->types();
            dt->clearData();
            afterAll = dt->types();
        });
        bool plainHasTransfer = true;
        target->addEventListener("mousedown", [&](kui::Event& e) {
            plainHasTransfer = e.dataTransfer() != nullptr;
        });
        KuiDocument* raw = doc->cHandle();
        auto mouse = [raw](KuiCMouseEventType kind, float x, int clicks) {
            return kui_devtools_dispatch_mouse_event(raw, kind, x, 50, KUI_C_MOUSE_BUTTON_LEFT, 0,
                                                     clicks, 0, 0);
        };
        mouse(KUI_C_MOUSE_PRESSED, 50, 1);
        for (int x = 60; x <= 240; x += 30) mouse(KUI_C_MOUSE_MOVED, static_cast<float>(x), 0);
        mouse(KUI_C_MOUSE_RELEASED, 240, 1);
        CHECK(started == 1 && drops == 1, "in-page drag: dragstart and drop fired");
        CHECK(data == "hello drag" && missing.empty() && !hasMissing, "DataTransfer::getData / hasData");
        CHECK(types == (std::vector<kui::String>{"text/plain", "application/x-kui"}), "DataTransfer::types");
        CHECK(allowed == kui::EffectAllowed::Copy && effect == kui::DropEffect::Copy,
              "effectAllowed / dropEffect read back in drop");
        CHECK(afterOne == std::vector<kui::String>{"text/plain"} && afterAll.empty(),
              "DataTransfer::clearData(format) / clearData()");
        mouse(KUI_C_MOUSE_PRESSED, 240, 1);
        mouse(KUI_C_MOUSE_RELEASED, 240, 1);
        CHECK(!plainHasTransfer, "a plain mouse event has no DataTransfer");
    }

    // Application framework (C ABI section 11): router, data binding, animation, observers.
    {
        kui::WindowConfig frameworkConfig;
        frameworkConfig.title = "wrapper-framework";
        frameworkConfig.width = 640;
        frameworkConfig.height = 480;
        auto window = kui::Window::create(frameworkConfig);
        auto* doc = window->document();
        doc->loadHTML("<html><body><main id='view'></main><p id='label'>old</p></body></html>");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);

        auto& router = doc->router();
        router.registerView(doc->getElementById("view"));
        kui::RouteDefinition user;
        user.path = "/users/:id";
        user.factory = [](kui::Document* owner, const kui::RouteParams& params) -> kui::RefPtr<kui::Node> {
            auto p = owner->createElement("p");
            p->setAttribute("id", "routed");
            p->setTextContent("user " + params.at("id"));
            return p;
        };
        user.beforeLeave = [](const kui::RouteContext&, const kui::RouteContext& to) {
            return to.path != "/locked";
        };
        user.meta["title"] = "User";
        CHECK(router.addRoute(user), "addRoute(RouteDefinition)");
        CHECK(router.addRoute("/locked", [](kui::Document*, const kui::RouteParams&) { return kui::RefPtr<kui::Node>(); }),
              "addRoute(path, factory)");
        CHECK(!router.addRoute("/locked", [](kui::Document*, const kui::RouteParams&) { return kui::RefPtr<kui::Node>(); }),
              "duplicate route rejected");
        int navigations = 0;
        const auto after = router.onAfterNavigate([&](const kui::RouteContext&) { ++navigations; });
        CHECK(router.push("/users/7?tab=a") && navigations == 1, "push");
        CHECK(router.currentRoute().pattern == "/users/:id" && router.currentParams().at("id") == "7", "currentRoute");
        CHECK(router.currentRoute().queryParams.at("tab") == "a", "query params");
        CHECK(router.currentPath() == "/users/7?tab=a", "Router::currentPath");
        CHECK(router.param("id") == std::optional<kui::String>("7") && !router.param("missing"), "Router::param");
        auto* routed = doc->getElementById("routed");
        CHECK(routed && routed->textContent() == "user 7", "view built by the factory");
        CHECK(!router.push("/locked"), "the route's own beforeLeave blocks");
        CHECK(router.findRoute("/users/:id") && router.findRoute("/users/:id")->meta.at("title") == "User", "findRoute meta");
        CHECK(router.removeRoute("/users/:id"), "removeRoute drops its guards too");
        CHECK(router.push("/locked") && router.historySize() == 2, "guard gone with the route");
        CHECK(router.back() && router.historyIndex() == 0, "back");
        router.removeAfterNavigate(after);
        router.clearRoutes();
        CHECK(router.routeCount() == 0, "clearRoutes");

        auto* label = doc->getElementById("label");
        kui::Observable<int> count(3);
        int changes = 0;
        const auto sub = count.subscribe([&] { ++changes; });
        auto& bindings = doc->bindingManager();
        const auto binding = bindings.bind(label, "textContent", &count);
        CHECK(binding != 0 && label->textContent() == "3", "bind writes right away");
        count = 4;
        count.modify([](int& value) { value += 1; });
        CHECK(count.get() == 5 && changes == 2, "Observable<int> set / modify / get");
        bindings.flush();
        CHECK(label->textContent() == "5", "binding follows");
        CHECK(bindings.bindingCount() == 1, "bindingCount");
        bindings.unbindElement(label);
        CHECK(bindings.bindingCount() == 0, "unbindElement");
        CHECK(count.unsubscribe(sub), "unsubscribe of a live id");
        CHECK(!count.unsubscribe(sub), "unsubscribe of an id that is not live returns false");
        count = 6;
        CHECK(changes == 2, "unsubscribed");
        const auto rebound = bindings.bind(label, "textContent", &count);
        CHECK(rebound != 0 && label->textContent() == "6", "rebind");
        count.unbindAll();
        count = 7;
        bindings.flush();
        CHECK(label->textContent() == "6", "Observable::unbindAll removed the binding");
        bindings.unbind(rebound);
        kui::Observable<kui::String> name("x");
        CHECK(bindings.bind(label, "title", &name) != 0, "bind attribute");
        bindings.unbindAll();

        // Capture-phase listeners and the in-process DevTools calls.
        doc->loadHTML("<html><body><div id='o' style='width:100px;height:50px'><button id='i'>x</button></div></body></html>");
        for (int i = 0; i < 3; ++i) kui::Platform::waitEvents(0.01);
        auto* outer = doc->getElementById("o");
        auto* inner = doc->getElementById("i");
        std::string order;
        outer->addEventListener("probe", [&](kui::Event&) { order += "op "; });
        inner->addEventListener("probe", [&](kui::Event&) { order += "ip "; });
        inner->addEventListener("probe", [&](kui::Event&) { order += "ic "; }, true);
        const auto captureId = outer->addEventListener("probe", [&](kui::Event&) { order += "oc "; }, true);
        inner->dispatchEvent("probe");
        CHECK(order == "oc ic ip op ", "capture listeners run first on the way down");
        outer->removeEventListener("probe", captureId);
        order.clear();
        inner->dispatchEvent("probe");
        CHECK(order == "ic ip op ", "a removed capture listener no longer runs");
        bool handleOk = false;
        outer->addEventListener("handle-probe", [&](kui::Event& event) {
            std::size_t length = 0;
            handleOk = event.cHandle() != nullptr && kui_event_get_type(event.cHandle(), nullptr, 0, &length) == KUI_OK &&
                       length == std::string("handle-probe").size();
        });
        outer->dispatchEvent("handle-probe");
        CHECK(handleOk, "Event::cHandle reaches the kui_event_* functions");

        const auto model = doc->boxModel("#o");
        CHECK(model && model->size.width >= 0, "Document::boxModel");
        CHECK(!doc->boxModel("#missing"), "boxModel of a missing selector is empty");
        const auto reply = doc->dispatchDevToolsCommand("{\"id\":1,\"method\":\"DOM.getDocument\"}");
        CHECK(reply.find("\"id\":1") != std::string::npos, "Document::dispatchDevToolsCommand");
        doc->dispatchKey(KUI_C_KEY_DOWN, "a", "KeyA");
        doc->dispatchKey(KUI_C_KEY_UP, "a");
        doc->dispatchIme(KUI_C_IME_START);
        doc->dispatchIme(KUI_C_IME_CANCEL);
        int downs = 0;
        inner->addEventListener("mousedown", [&](kui::Event&) { ++downs; });
        const auto innerBox = doc->boxModel("#i");
        if (innerBox) {
            const float x = innerBox->content[0].x + 1, y = innerBox->content[0].y + 1;
            doc->dispatchMouse(KUI_C_MOUSE_PRESSED, x, y, KUI_C_MOUSE_BUTTON_LEFT, 0, 1);
            doc->dispatchMouse(KUI_C_MOUSE_RELEASED, x, y, KUI_C_MOUSE_BUTTON_LEFT, 0, 1);
            CHECK(downs == 1, "Document::dispatchMouse reaches the element");
        }

        doc->loadHTML("<html><head><style>@keyframes p{from{opacity:0}to{opacity:1}}"
                      "#box{animation:p 1s infinite}</style></head><body style='margin:0'>"
                      "<div id='box' title='a' style='width:120px;height:40px'></div></body></html>");
        auto* boxEl = doc->getElementById("box");
        bool animating = false;
        for (int i = 0; i < 100 && !animating; ++i) {
            kui::Platform::waitEvents(0.01);
            animating = doc->animationManager()->hasActiveAnimations(boxEl);
        }
        CHECK(animating && doc->animationManager()->activeAnimationCount() >= 1, "animation state");
        CHECK(!doc->transitionManager()->hasActiveTransitions(boxEl), "no transition");

        std::string oldTitle;
        auto mutation = kui::MutationObserver::create(
            [&](const std::vector<kui::MutationRecord>& records, kui::MutationObserver*) {
                for (const auto& record : records) {
                    if (record.type == kui::MutationRecord::Type::Attributes && record.target.get() == boxEl)
                        oldTitle = record.oldValue;
                }
            });
        kui::MutationObserverInit init;
        init.attributes = true;
        init.attributeOldValue = true;
        mutation->observe(boxEl, init);
        doc->mutationObserverRegistry().registerObserver(mutation.get());
        boxEl->setAttribute("title", "b");
        CHECK(oldTitle == "a", "MutationObserver record");
        doc->mutationObserverRegistry().unregisterObserver(mutation.get());
        int addedType = -1;
        auto childObserver = kui::MutationObserver::create(
            [&](const std::vector<kui::MutationRecord>& records, kui::MutationObserver*) {
                for (const auto& record : records)
                    if (record.type == kui::MutationRecord::Type::ChildList && record.addedNodeType != 0)
                        addedType = record.addedNodeType;
            });
        kui::MutationObserverInit childInit;
        childInit.childList = true;
        childObserver->observe(boxEl, childInit);
        doc->mutationObserverRegistry().registerObserver(childObserver.get());
        boxEl->setTextContent("hello");
        CHECK(addedType == 3, "MutationRecord::addedNodeType reports a text node");
        doc->mutationObserverRegistry().unregisterObserver(childObserver.get());

        float width = 0;
        auto resize = kui::ResizeObserver::create(
            [&](const std::vector<kui::ResizeObserverEntry>& entries, kui::ResizeObserver*) {
                width = entries.back().contentBoxSize.inlineSize;
            });
        resize->observe(boxEl);
        for (int i = 0; i < 100 && width == 0; ++i) kui::Platform::waitEvents(0.01);
        CHECK(width == 120.0f, "ResizeObserver first report");

        int intersecting = -1;
        auto intersection = kui::IntersectionObserver::create(
            [&](const std::vector<kui::IntersectionObserverEntry>& entries, kui::IntersectionObserver*) {
                intersecting = entries.back().isIntersecting ? 1 : 0;
            });
        intersection->observe(boxEl);
        for (int i = 0; i < 100 && intersecting == -1; ++i) kui::Platform::waitEvents(0.01);
        CHECK(intersecting == 1, "IntersectionObserver");

        // Dropping the last reference from inside its own callback is safe.
        auto self = kui::MutationObserver::create(
            [&](const std::vector<kui::MutationRecord>&, kui::MutationObserver*) { mutation.reset(); });
        mutation = self;
        self.reset();
        mutation->observe(boxEl, init);
        boxEl->setAttribute("title", "c");
        CHECK(!mutation, "observer released inside its callback");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);
    }

    // RefPtr<Document>(window->document()) keeps the document after the window is gone.
    {
        kui::RefPtr<kui::Document> kept;
        {
            kui::WindowConfig keepConfig;
            keepConfig.title = "wrapper-keep-document";
            auto window = kui::Window::create(keepConfig);
            window->document()->loadHTML("<html><body><p id='k'>kept</p></body></html>");
            kept = kui::RefPtr<kui::Document>(window->document());
            CHECK(kept.get() == window->document(), "RefPtr<Document>(raw) shares the window's document");
        }
        CHECK(kept && kept->getElementById("k") && kept->getElementById("k")->textContent() == "kept",
              "document outlives its window");
    }

    // Popups, input picker and game audio (C ABI section 12).
    {
        kui::WindowConfig widgetConfig;
        widgetConfig.title = "wrapper-widgets";
        widgetConfig.width = 640;
        widgetConfig.height = 480;
        auto window = kui::Window::create(widgetConfig);
        auto* doc = window->document();
        doc->loadHTML("<html><body><input id='d' type='date' value='2026-11-16'><input id='c' type='color'>"
                      "<audio id='a'></audio><lottie-player id='l'></lottie-player></body></html>");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);

        auto& popups = doc->popupManager();
        const auto tip = popups.showTooltip("hello", kui::Rect{10, 10, 50, 20}, kui::PopupPosition::Bottom);
        CHECK(tip != 0 && popups.hasPopupOfType(kui::PopupType::Tooltip), "showTooltip");
        int picked = 0;
        const auto menu = popups.showContextMenu(20, 30, {kui::MenuItem{"Copy", [&] { ++picked; }},
                                                          kui::MenuItem{"", nullptr, false, true}});
        CHECK(menu != 0 && popups.hasPopupOfType(kui::PopupType::ContextMenu), "showContextMenu");
        popups.dismissAll();
        CHECK(!popups.hasActivePopups(), "dismissAll");
        kui::TooltipTheme theme;
        theme.bg = kui::Color(1, 2, 3, 4);
        theme.radius = 5;
        popups.setTooltipTheme(theme);
        CHECK(popups.tooltipTheme().bg == kui::Color(1, 2, 3, 4) && popups.tooltipTheme().radius == 5,
              "tooltip theme");
        popups.setTooltipDelay(200);
        CHECK(popups.tooltipDelay() == 200, "tooltip delay");
        auto* events = doc->eventDispatcher();
        CHECK(events && events == window->eventDispatcher(), "window and document share the event dispatcher");
        CHECK(!events->isDefaultContextMenuEnabled(), "the built-in context menu is off by default");
        events->setDefaultContextMenuEnabled(true);
        CHECK(events->isDefaultContextMenuEnabled(), "default context menu enabled");
        events->setDefaultContextMenuEnabled(false);

        // <kui-context-menu>: right-click opens it; menu-open's relatedTarget is the right-clicked element.
        // Appended to the current page (the picker checks below still use its inputs).
        doc->body()->insertAdjacentHTML("beforeend",
            "<div id='ctx-row' style='position:fixed;left:400px;top:300px;width:120px;height:40px'>row</div>"
            "<kui-context-menu id='ctx-menu' for='ctx-row'><kui-menu-item text='Open'></kui-menu-item></kui-context-menu>");
        for (int i = 0; i < 5; ++i) kui::Platform::waitEvents(0.01);
        auto* ctxMenu = doc->getElementById("ctx-menu");
        std::string related;
        ctxMenu->addEventListener("menu-open", [&](kui::MouseEvent& e) {
            if (auto* element = e.relatedTarget()) related = element->id();
        });
        kui_devtools_dispatch_mouse_event(doc->cHandle(), KUI_C_MOUSE_PRESSED, 420, 310, KUI_C_MOUSE_BUTTON_RIGHT, 0, 1, 0, 0);
        kui_devtools_dispatch_mouse_event(doc->cHandle(), KUI_C_MOUSE_RELEASED, 420, 310, KUI_C_MOUSE_BUTTON_RIGHT, 0, 1, 0, 0);
        CHECK(ctxMenu->hasAttribute("open") && related == "ctx-row", "declarative context menu + relatedTarget");
        ctxMenu->removeAttribute("open");

        auto& picker = doc->inputPicker();
        auto* date = doc->getElementById("d");
        CHECK(picker.open(date), "InputPicker::open returns true");
        CHECK(picker.isOpen() && picker.mode() == kui::PickerMode::Date && picker.targetInput() == date,
              "InputPicker::open by type");
        CHECK(!picker.open(nullptr), "InputPicker::open(nullptr) returns false");
        CHECK(picker.visibleYear() == 2026 && picker.visibleMonth() == 11, "calendar page");
        picker.setVisibleYearMonth(2027, 3);
        CHECK(picker.visibleMonth() == 3, "setVisibleYearMonth");
        picker.close();
        picker.openColor(doc->getElementById("c"));
        picker.setColorHsv(0, 1, 1);
        CHECK(picker.colorHex() == "#ff0000", "colorHex from HSV");
        picker.commit();
        CHECK(!picker.isOpen(), "commit closes");

        auto& audio = doc->audioManager();
        if (!audio.isAvailable()) {
            CHECK(audio.play("missing.wav") == kui::kInvalidSoundHandle, "no device: play returns 0");
        }
        audio.setGroupVolume(kui::AudioGroup::Music, 0.5f);
        audio.setGroupMuted(kui::AudioGroup::Music, true);
        CHECK(audio.groupVolume(kui::AudioGroup::Music) == 0.5f && audio.isGroupMuted(kui::AudioGroup::Music),
              "audio group state");
        CHECK(audio.activeCount() == 0 && !audio.isPlaying(12345), "audio counts");
        audio.stopAll();

        // Section 13: media / Lottie playback control with the native member names.
        auto* media = doc->getElementById("a");
        media->setLoop(true);
        media->setAutoplay(true);
        CHECK(media->loop() && media->autoplay() && !media->ended(), "media loop / autoplay / ended");
        media->load();
        auto* lottie = doc->getElementById("l");
        lottie->lottieSetSpeed(2.0f);
        lottie->setLoop(true);
        lottie->seek(0.25);
        CHECK(lottie->speed() == 2.0f && lottie->loop() && lottie->currentPosition() == 0.25 && !lottie->isPlaying(),
              "Lottie speed / loop / seek");
        lottie->stop();
        CHECK(lottie->currentPosition() == 0.0, "Lottie stop rewinds");

        // Section 14: extension and DevTools lifecycle.
        doc->initExtensions("no-such-extension-dir");
        auto* extensions = doc->extensionManager();
        CHECK(extensions && !extensions->loadExtension("./no-such-extension.dll"), "loadExtension of a missing DLL");
        CHECK(extensions && !extensions->unloadExtension("kui-no-such"), "unloadExtension of an unknown tag");
        if (extensions) {
            extensions->startWatching(".");
#if defined(_WIN32)
            CHECK(extensions->isWatching(), "startWatching");
#else
            // Tripwire for T-DEBT-POSIX-EXTENSION-HOT-RELOAD-NOT-IMPLEMENTED: the Linux / macOS core has no
            // directory watcher yet and reports "not watching". When it does, this fails; switch back to the
            // Windows assertion in the same change.
            CHECK(!extensions->isWatching(), "startWatching: POSIX core has no directory watcher yet");
#endif
            extensions->stopWatching();
            CHECK(!extensions->isWatching() && extensions->count() == 0, "stopWatching");
        }
        if (auto* devtools = window->startDevTools(0)) {
            CHECK(devtools->isRunning() && devtools->url().find("token=") != kui::String::npos, "DevTools running");
            devtools->stop();
            CHECK(!devtools->isRunning() && window->devToolsServer() == devtools, "DevTools stopped");
        }
    }
    {
        // Before shutdown(): once the platform is down, Linux's waitEvents has no X connection and returns at
        // once without running timers (requestIdle is a zero-delay timer there).
        bool idleRan = false;
        kui::Platform::requestIdle([&] { idleRan = true; });
        for (int i = 0; i < 20 && !idleRan; ++i) kui::Platform::waitEvents(0.01);
        CHECK(idleRan, "Platform::requestIdle runs once the loop is idle");
    }
    kui::Platform::shutdown();

    kui::PDFOptions pdf;
    pdf.title = "compile-time PDF surface";
    CHECK(pdf.pageWidth_pt > 0 && pdf.pageHeight_pt > 0, "PDF defaults");

    // Section 9 (3.61.8): tree editing, selectors, serialization, pointer capture.
    auto tree = kui::Document::create();
    tree->loadHTML("<html><body><div id='list' class='box'><p id='a'>A</p><p id='b'>B</p></div>"
                   "<span id='free'>f</span></body></html>");
    auto* box = tree->getElementById("list");
    {
        // Native-style RefPtr(raw) adds a reference: it must share the document's
        // ownership, not start a second owner (T-DEBT-SDK-WRAPPER-REFPTR-RAW-POINTER-DOUBLE-OWNERSHIP).
        kui::RefPtr<kui::Element> held(box);
        kui::RefPtr<kui::Node> asNode(tree->getElementById("a"));
        CHECK(held.get() == box && held.use_count() >= 2, "RefPtr(raw) shares ownership");
        CHECK(asNode && asNode->id() == "a", "RefPtr<Node>(raw)");
    }
    auto* b = tree->getElementById("b");
    auto freebie = tree->querySelectorAll("#free").front();  // shared_ptr held by the document state
    CHECK(box->insertBefore(freebie, b) == freebie.get(), "insertBefore returns the child");
    CHECK(b->previousElementSibling() == freebie.get(), "insertBefore order");
    auto replacement = tree->createElement("em");
    CHECK(box->replaceChild(replacement, b) == replacement.get(), "replaceChild");
    CHECK(box->childElementCount() == 3, "replaceChild keeps the count");
    auto copy = box->cloneNode(true);
    CHECK(copy && copy->childElementCount() == 3 && copy->parentElement() == nullptr,
          "deep cloneNode is detached with children");
    CHECK(box->matches("div.box") && !box->matches("span"), "matches");
    CHECK(tree->getElementById("a")->closest(".box") == box, "closest");
    CHECK(tree->getElementById("a")->outerHTML().find("id=\"a\"") != kui::String::npos, "outerHTML");
    CHECK(box->computedStylePropertyValue("display") == "block", "computedStylePropertyValue");
    CHECK(box->computedStylePropertyValue("no-such-property").empty(), "unknown property is empty");
    box->insertAdjacentHTML("beforeend", "<i id='tail'>t</i>");
    CHECK(box->lastElementChild()->id() == "tail", "insertAdjacentHTML");
    tree->getElementById("a")->appendText(" more");
    CHECK(tree->getElementById("a")->textContent() == "A more", "appendText");
    auto* a = tree->getElementById("a");
    a->setPointerCapture(1);
    CHECK(a->hasPointerCapture(1), "setPointerCapture");
    a->releasePointerCapture(1);
    CHECK(!a->hasPointerCapture(1), "releasePointerCapture");
    CHECK(!a->hasFocus(), "element hasFocus");
    CHECK(tree->getElementsByTagName("p").size() == 1, "getElementsByTagName");
    CHECK(tree->getElementsByClassName("box").size() == 1, "getElementsByClassName");

    // 表单控件的当前状态（与内容属性 value="…" 的初值区分开）
    tree->loadHTML(
        "<html><body><input id='name' type='text' value='initial'>"
        "<select id='size'><option value='s'>S</option><option value='m' selected>M</option>"
        "<option value='l'>L</option></select><input id='agree' type='checkbox'>"
        "<progress id='prog' value='3' max='10'></progress><div id='plain' value='attr'></div></body></html>");
    auto* name = tree->getElementById("name");
    CHECK(name->value() == "initial", "form value starts at the initial value");
    name->setValue("Ada");
    CHECK(name->value() == "Ada", "form value is the current value");
    CHECK(name->getAttribute("value") == "initial", "content attribute keeps the initial value");
    auto* size = tree->getElementById("size");
    CHECK(size->selectedIndex() == 1, "select starts at the selected option");
    size->setSelectedIndex(2);
    CHECK(size->value() == "l", "select value follows the selected index");
    auto* agree = tree->getElementById("agree");
    CHECK(!agree->checked(), "checkbox starts unchecked");
    agree->setChecked(true);
    CHECK(agree->checked(), "checkbox checked");
    auto* prog = tree->getElementById("prog");
    prog->setValue("7.5");
    CHECK(prog->value() == "7.5", "progress value");
    bool rejected = false;
    try { prog->setValue("abc"); } catch (const kui::Error&) { rejected = true; }
    CHECK(rejected, "non-numeric progress value rejected");
    auto* plain = tree->getElementById("plain");
    CHECK(plain->value() == "attr", "elements without a value state fall back to the attribute");
    plain->setValue("x");
    CHECK(plain->getAttribute("value") == "x", "setValue on such elements writes the attribute");
    bool notSupported = false;
    try { (void)name->selectedIndex(); } catch (const kui::Error&) { notSupported = true; }
    CHECK(notSupported, "a text input has no selected index");

    // 宿主集成：基准路径、脚本定时器、控制台与诊断（没有窗口的文档：渲染 DPR 没有渲染器）
    tree->setBasePath("assets/pages/");
    CHECK(tree->basePath() == "assets/pages/", "base path read back");
    KuiStatus dpr_status = KUI_OK;
    try { tree->setRenderDevicePixelRatio(1.0f); } catch (const kui::Error& thrown) { dpr_status = thrown.status(); }
    CHECK(dpr_status == KUI_ERR_NOT_SUPPORTED, "no renderer without a window");
    if (auto timers = kui::ScriptEngine::create()) {
        timers->bindDocument(tree.get());
        CHECK(timers->eval("host_timer_fired = false; setTimeout(function() host_timer_fired = true end, 50)", "timers").success,
              "schedule a timer");
        timers->tickTimers(0.0);
        CHECK(!timers->getGlobal("host_timer_fired").asBool(), "the timer has not fired yet");
        timers->tickTimers(100.0);
        CHECK(timers->getGlobal("host_timer_fired").asBool(), "the timer fires on the later tick");
        CHECK(timers->lastError().empty(), "lastError is empty after a successful run");
        CHECK(!timers->eval("error('boom')", "failing").success, "a failing script");
        CHECK(timers->lastError().find("boom") != std::string::npos, "ScriptEngine::lastError");
        timers->unbindDocument();
    }
    {
        const KuiVersion version = kui::Platform::runtimeVersion();
        CHECK(version.major == KUI_C_API_VERSION_MAJOR && version.minor >= KUI_C_API_VERSION_MINOR, "Platform::runtimeVersion");
    }
    {
        auto& console = kui::Console::instance();
        console.setEnabled(true);
        console.setStderrOutput(false);
        console.setMinLevel(kui::LogLevel::Debug);
        kui::StackTrace::Config stack;
        stack.outputMode = kui::StackTrace::WriteToStderr;
        (void)kui::StackTrace::install(stack);
        kui::StackTrace::uninstall();
        (void)kui::CrashReporter::install();  // false when crashpad_handler is missing
        kui::CrashReporter::uninstall();
        KuiStatus ime = KUI_OK;
        try { kui::Platform::disableIme(); } catch (const kui::Error& thrown) { ime = thrown.status(); }
        CHECK(ime == KUI_OK || ime == KUI_ERR_NOT_SUPPORTED, "disableIme");
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
