#include <kui/kui.h>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — DOM Manipulation",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { padding: 24px; font-family: "Segoe UI", sans-serif; background: #f8fafc; }
h2 { margin-bottom: 16px; color: #1e293b; }
.toolbar { display: flex; gap: 8px; margin-bottom: 16px; }
.btn { padding: 8px 16px; border-radius: 6px; font-size: 13px; cursor: pointer;
       font-weight: 600; border: none; color: white; }
.btn-add { background: #10b981; }
.btn-remove { background: #ef4444; }
.btn-shuffle { background: #3b82f6; }
.btn-clear { background: #6b7280; }
#container { display: flex; flex-wrap: wrap; gap: 8px; min-height: 100px;
             padding: 16px; background: white; border-radius: 8px;
             box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.card { padding: 12px 16px; background: #dbeafe; color: #1e40af; border-radius: 6px;
        font-size: 13px; font-weight: 600; }
#counter { margin-top: 12px; font-size: 14px; color: #64748b; }
</style></head><body>
<h2>Dynamic DOM Manipulation</h2>
<div class="toolbar">
  <button class="btn btn-add" id="addBtn">Add Element</button>
  <button class="btn btn-remove" id="removeBtn">Remove Last</button>
  <button class="btn btn-clear" id="clearBtn">Clear All</button>
</div>
<div id="container"></div>
<div id="counter">Elements: 0</div>
</body></html>
    )");

    auto* container = doc->getElementById("container");
    auto* counter = doc->getElementById("counter");
    int nextId = 1;

    auto updateCounter = [container, counter]() {
        counter->setTextContent("Elements: " + std::to_string(container->childElementCount()));
    };

    doc->getElementById("addBtn")->addEventListener("click", [doc, container, &nextId, updateCounter](kui::Event& e) {
        auto card = doc->createElement("div");
        card->setClassName("card");
        card->setTextContent("Item #" + std::to_string(nextId++));
        container->appendChild(card);
        updateCounter();
    });

    doc->getElementById("removeBtn")->addEventListener("click", [container, updateCounter](kui::Event& e) {
        auto* last = container->lastElementChild();
        if (last) { container->removeChild(last); updateCounter(); }
    });

    doc->getElementById("clearBtn")->addEventListener("click", [container, updateCounter](kui::Event& e) {
        container->setInnerHTML("");
        updateCounter();
    });

    return kui::Platform::run();
}
