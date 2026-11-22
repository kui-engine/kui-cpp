#include <kui/kui.h>
#include <string>

int main() {
    kui::Platform::init();
    auto window = kui::Window::create({.title = "KUI — Tree View", .width = 500, .height = 500});
    auto* doc = window->document();
    doc->loadHTML(R"(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{padding:24px;font-family:"Segoe UI",sans-serif;background:#f8fafc}
h2{color:#1e293b;margin-bottom:16px}
.tree{background:white;border-radius:12px;box-shadow:0 1px 3px rgba(0,0,0,.1);padding:16px}
.folder{margin-bottom:2px}
.flabel{display:flex;align-items:center;gap:6px;padding:6px 8px;border-radius:6px;cursor:pointer;font-size:13px;color:#475569}
.flabel:hover{background:#f1f5f9}
.flabel.sel{background:#dbeafe;color:#1e40af}
.flabel .arr{font-size:10px;color:#94a3b8;width:14px}
.children{padding-left:20px;display:none}
.children.open{display:block}
.file{display:flex;align-items:center;gap:6px;padding:6px 8px 6px 28px;border-radius:6px;cursor:pointer;font-size:13px;color:#475569}
.file:hover{background:#f1f5f9}
.file.sel{background:#dbeafe;color:#1e40af}
.status{margin-top:12px;padding:12px;background:#f1f5f9;border-radius:8px;font-size:13px;color:#475569}
</style></head><body>
<h2>Tree View</h2>
<div class="tree">
<div class="folder">
<div class="flabel" id="f0"><span class="arr">▶</span> src</div>
<div class="children open" id="c0">
  <div class="file" id="i0">flex_layout.cpp</div>
  <div class="file" id="i1">block_layout.cpp</div>
  <div class="file" id="i2">skia_backend.cpp</div>
</div></div>
<div class="folder">
<div class="flabel" id="f1"><span class="arr">▶</span> include</div>
<div class="children" id="c1">
  <div class="file" id="i3">kui.h</div>
  <div class="file" id="i4">element.h</div>
</div></div>
<div class="folder">
<div class="flabel" id="f2"><span class="arr">▶</span> tests</div>
<div class="children" id="c2">
  <div class="file" id="i5">test_layout.cpp</div>
</div></div>
</div>
<div class="status" id="status">Click to select.</div>
</body></html>
    )");

    // Folder toggle
    for (int f = 0; f < 3; ++f) {
        doc->getElementById("f" + std::to_string(f))->addEventListener("click", [doc, f](kui::Event&) {
            auto* ch = doc->getElementById("c" + std::to_string(f));
            bool isOpen = ch->className().find("open") != std::string::npos;
            ch->setClassName(isOpen ? "children" : "children open");
        });
    }
    // File select
    for (int i = 0; i < 6; ++i) {
        auto* el = doc->getElementById("i" + std::to_string(i));
        if (el) {
            el->addEventListener("click", [doc](kui::Event& e) {
                auto* t = static_cast<kui::Element*>(e.target());
                doc->getElementById("status")->setTextContent("Selected: " + t->textContent());
            });
        }
    }
    return kui::Platform::run();
}
