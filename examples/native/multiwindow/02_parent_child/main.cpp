#include <kui/kui.h>
int main() {
    kui::Platform::init();
    auto parent = kui::Window::create({.title = "KUI — Parent Window", .width = 600, .height = 400});
    auto* pd = parent->document();
    pd->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc;display:flex;align-items:center;justify-content:center;height:100vh}
.card{background:#1e293b;border-radius:16px;padding:32px;text-align:center;max-width:450px;box-shadow:0 20px 60px rgba(0,0,0,.4)}
.card h2{font-size:22px;margin-bottom:8px}
.card p{font-size:14px;color:#94a3b8;margin-bottom:16px;line-height:1.6}
.badge{display:inline-block;padding:6px 16px;border-radius:20px;font-size:12px;font-weight:600;margin:4px}
.bp{background:#dbeafe;color:#1e40af}.bg{background:#dcfce7;color:#166534}
.info{margin-top:16px;font-size:12px;color:#64748b}
</style></head><body>
<div class="card">
<h2>Parent Window</h2>
<p>This coordinator window creates and controls a second independent KUI window.</p>
<div class="badge bp">Parent</div>
<div class="badge bg">Child spawned</div>
<div class="info">KUI's current public Window API has no native owner handle; this demo does not claim an OS stacking relationship.</div>
</div>
</body></html>)HTML");
    auto child = kui::Window::create({.title = "KUI — Child Window", .width = 350, .height = 250});
    auto* cd = child->document();
    cd->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI",sans-serif;background:#1e293b;color:#f8fafc;display:flex;align-items:center;justify-content:center;height:100vh}
.card{text-align:center;padding:24px}
h2{font-size:18px;margin-bottom:8px}
p{font-size:13px;color:#94a3b8}
.badge{display:inline-block;padding:6px 16px;border-radius:20px;background:#dcfce7;color:#166534;font-size:12px;font-weight:600;margin-top:12px}
</style></head><body>
<div class="card"><h2>Secondary Window</h2><p>This window is kept alive and coordinated by the same application.</p><div class="badge">Secondary</div></div>
</body></html>)HTML");
    const auto parentPos = parent->position();
    child->setPosition(static_cast<kui::i32>(parentPos.x + 620), static_cast<kui::i32>(parentPos.y + 60));
    return kui::Platform::run();
}
