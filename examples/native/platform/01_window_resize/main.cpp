#include <kui/kui.h>
int main() {
    kui::Platform::init();
    auto window = kui::Window::create({.title = "KUI — Window Resize", .width = 600, .height = 400});
    auto* doc = window->document();
    doc->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc;height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#1e293b;border-radius:16px;padding:32px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.4);max-width:400px}
.card h2{font-size:22px;margin-bottom:8px}
.card p{font-size:14px;color:#94a3b8;line-height:1.6;margin-bottom:16px}
.badge{display:inline-block;padding:6px 16px;border-radius:20px;font-size:12px;font-weight:600;margin:4px}
.bb{background:#dbeafe;color:#1e40af}.bg{background:#dcfce7;color:#166534}
.info{margin-top:16px;font-size:12px;color:#64748b;line-height:1.5}
.responsive-demo{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:16px}
@media (max-width:500px){.responsive-demo{grid-template-columns:1fr}}
.r-box{padding:12px;border-radius:8px;text-align:center;font-size:11px;font-weight:600;color:white}
</style></head><body>
<div class="card">
<h2>Window Resize</h2>
<p>Drag the window edges to resize. The layout responds to size changes.</p>
<div class="badge bb">resizable = true</div>
<div class="badge bg">responsive CSS</div>
<div class="responsive-demo">
<div class="r-box" style="background:#3b82f6">Col 1</div>
<div class="r-box" style="background:#10b981">Col 2</div>
<div class="r-box" style="background:#f59e0b">Col 3</div>
</div>
<div class="info" id="size">CSS media queries respond to window size changes in real-time.</div>
</div>
</body></html>)HTML");
    auto updateSize = [doc](kui::u32 width, kui::u32 height) {
        doc->getElementById("size")->setTextContent(
            "Live client size: " + std::to_string(width) + " × " + std::to_string(height) + " CSS px");
    };
    updateSize(window->width(), window->height());
    window->onResize(updateSize);
    return kui::Platform::run();
}
