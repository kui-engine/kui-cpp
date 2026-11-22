#include <kui/kui.h>
#include <kui/component/virtual_list.h>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Virtual List",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { padding: 24px; font-family: "Segoe UI", sans-serif; background: #f8fafc; }
h2 { margin-bottom: 8px; color: #1e293b; }
.info { font-size: 14px; color: #64748b; margin-bottom: 16px; }
#listContainer { height: 500px; background: white; border-radius: 8px;
                 box-shadow: 0 1px 3px rgba(0,0,0,0.1); overflow-y: auto; }
.row { padding: 12px 16px; border-bottom: 1px solid #f1f5f9; font-size: 14px; color: #475569; }
.row:hover { background: #f8fafc; }
.row .idx { color: #3b82f6; font-weight: 600; margin-right: 12px; }
</style></head><body>
<h2>Virtual List</h2>
<p class="info" id="stats">Rendering 10,000 items efficiently using KUI VirtualList.</p>
<p class="info"><button id="topBtn">Top</button> <button id="middleBtn">Item 5,000</button> <button id="bottomBtn">Bottom</button></p>
<div id="listContainer"></div>
</body></html>
    )");

    auto* container = doc->getElementById("listContainer");
    kui::VirtualList list;
    kui::VirtualListConfig config;
    config.totalItems = 10000;
    config.itemHeight = 42.0f;
    config.overscan = 5;
    config.renderItem = [](kui::Document* itemDoc, kui::u32 index) {
        auto row = itemDoc->createElement("div");
        row->setClassName("row");
        row->style()->setProperty("height", "42px");
        row->setInnerHTML("<span class=\"idx\">#" + std::to_string(index + 1) +
                          "</span>Virtualized item " + std::to_string(index + 1));
        return row;
    };
    list.setup(container, config);
    auto updateStats = [doc, &list]() {
        doc->getElementById("stats")->setTextContent(
            "10,000 logical items · live DOM range [" + std::to_string(list.visibleStart()) + ", " +
            std::to_string(list.visibleEnd()) + ") · " + std::to_string(list.renderedCount()) + " rows rendered");
    };
    doc->getElementById("topBtn")->addEventListener("click", [&list, updateStats](kui::Event&) { list.scrollToTop(); updateStats(); });
    doc->getElementById("middleBtn")->addEventListener("click", [&list, updateStats](kui::Event&) { list.scrollToIndex(4999); updateStats(); });
    doc->getElementById("bottomBtn")->addEventListener("click", [&list, updateStats](kui::Event&) { list.scrollToBottom(); updateStats(); });
    kui::Platform::setInterval(updateStats, 250);

    return kui::Platform::run();
}
