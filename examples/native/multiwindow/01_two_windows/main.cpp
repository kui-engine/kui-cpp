#include <kui/kui.h>
int main() {
    kui::Platform::init();
    auto w1 = kui::Window::create({.title = "KUI — Primary", .width = 500, .height = 350});
    auto* d1 = w1->document();
    d1->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc;display:flex;align-items:center;justify-content:center;height:100vh}
.card{background:#1e293b;border-radius:16px;padding:32px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.4);max-width:400px}
.card h2{font-size:22px;margin-bottom:8px}
.card p{font-size:14px;color:#94a3b8;margin-bottom:16px}
.badge{display:inline-block;padding:6px 16px;border-radius:20px;background:#dbeafe;color:#1e40af;font-size:12px;font-weight:600}
</style></head><body>
<div class="card"><h2>Primary Window</h2>
<p>Two independent windows, each with its own document and styles.</p>
<div class="badge">Window 1 of 2</div></div>
</body></html>)HTML");
    auto w2 = kui::Window::create({.title = "KUI — Secondary", .width = 400, .height = 300});
    auto* d2 = w2->document();
    d2->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI",sans-serif;background:#064e3b;color:#f8fafc;display:flex;align-items:center;justify-content:center;height:100vh}
.card{background:rgba(255,255,255,.1);border-radius:16px;padding:32px;text-align:center;border:1px solid rgba(255,255,255,.15)}
.card h2{font-size:20px;margin-bottom:8px}
.card p{font-size:13px;color:#a7f3d0;margin-bottom:12px}
.badge{display:inline-block;padding:6px 16px;border-radius:20px;background:#dcfce7;color:#166534;font-size:12px;font-weight:600}
</style></head><body>
<div class="card"><h2>Secondary Window</h2>
<p>Each window runs independently with its own content.</p>
<div class="badge">Window 2 of 2</div></div>
</body></html>)HTML");
    return kui::Platform::run();
}
