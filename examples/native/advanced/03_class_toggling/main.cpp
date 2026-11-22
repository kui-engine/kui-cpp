#include <kui/kui.h>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Class Toggling",
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
.panel { background: white; padding: 20px; border-radius: 12px;
         box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 16px;
         transition: all 0.3s ease; }
.panel.expanded { padding: 32px; }
.panel.highlight { border-left: 4px solid #3b82f6; background: #eff6ff; }
.panel.danger { border-left: 4px solid #ef4444; background: #fef2f2; }
.toolbar { display: flex; gap: 8px; margin-bottom: 16px; }
.btn { padding: 8px 16px; border-radius: 6px; font-size: 13px; cursor: pointer;
       border: none; color: white; font-weight: 600; }
.btn-blue { background: #3b82f6; }
.btn-green { background: #10b981; }
.btn-red { background: #ef4444; }
.btn-purple { background: #8b5cf6; }
#classes { font-family: monospace; font-size: 13px; background: #f1f5f9;
           padding: 8px 12px; border-radius: 6px; color: #475569; margin-top: 8px; }
</style></head><body>
<h2>Class Toggling</h2>
<div class="toolbar">
  <button class="btn btn-blue" id="toggleExpand">Toggle Expanded</button>
  <button class="btn btn-green" id="toggleHighlight">Toggle Highlight</button>
  <button class="btn btn-red" id="toggleDanger">Toggle Danger</button>
</div>
<div class="panel" id="panel">
  <h3 style="margin-bottom:8px;">Target Panel</h3>
  <p style="color:#64748b;">Click buttons above to toggle CSS classes on this panel.</p>
  <div id="classes">Classes: panel</div>
</div>
</body></html>
    )");

    auto* panel = doc->getElementById("panel");
    auto* classes = doc->getElementById("classes");
    auto updateClasses = [panel, classes]() {
        classes->setTextContent("Classes: " + panel->className());
    };
    doc->getElementById("toggleExpand")->addEventListener("click", [panel, updateClasses](kui::Event&) {
        panel->toggleClass("expanded"); updateClasses();
    });
    doc->getElementById("toggleHighlight")->addEventListener("click", [panel, updateClasses](kui::Event&) {
        panel->toggleClass("highlight"); updateClasses();
    });
    doc->getElementById("toggleDanger")->addEventListener("click", [panel, updateClasses](kui::Event&) {
        panel->toggleClass("danger"); updateClasses();
    });

    return kui::Platform::run();
}
