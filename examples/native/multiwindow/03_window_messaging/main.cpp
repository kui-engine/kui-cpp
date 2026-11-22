#include <kui/kui.h>
int main() {
    kui::Platform::init();
    auto w1 = kui::Window::create({.title = "KUI — Window A", .width = 500, .height = 350});
    auto* d1 = w1->document();
    d1->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{padding:24px;font-family:"Segoe UI",sans-serif;background:#0f172a;color:#f8fafc}
h2{margin-bottom:16px}
.card{background:#1e293b;border-radius:12px;padding:20px;margin-bottom:12px}
.card h3{font-size:14px;margin-bottom:8px}
.card p{font-size:13px;color:#94a3b8;margin-bottom:12px}
.btn{padding:10px 20px;border:none;border-radius:8px;background:#3b82f6;color:white;font-size:13px;font-weight:600;cursor:pointer}
.log{background:#0f172a;border-radius:8px;padding:12px;font-size:12px;color:#94a3b8;font-family:"Consolas",monospace;min-height:60px}
</style></head><body>
<h2>Window A — Sender</h2>
<div class="card"><h3>Send Message</h3><p>Click to send a message to Window B.</p>
<button class="btn" id="sendBtn">Send Message</button></div>
<div class="card"><h3>Log</h3><div class="log" id="log">Ready...</div></div>
</body></html>)HTML");
    auto w2 = kui::Window::create({.title = "KUI — Window B", .width = 500, .height = 350});
    auto* d2 = w2->document();
    d2->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{padding:24px;font-family:"Segoe UI",sans-serif;background:#064e3b;color:#f8fafc}
h2{margin-bottom:16px}
.card{background:rgba(255,255,255,.1);border-radius:12px;padding:20px;border:1px solid rgba(255,255,255,.1)}
.card h3{font-size:14px;margin-bottom:8px}
.log{background:rgba(0,0,0,.3);border-radius:8px;padding:12px;font-size:12px;color:#a7f3d0;font-family:"Consolas",monospace;min-height:60px}
</style></head><body>
<h2>Window B — Receiver</h2>
<div class="card"><h3>Messages</h3><div class="log" id="log2">Waiting for messages...</div></div>
</body></html>)HTML");

    int count = 0;
    d1->getElementById("sendBtn")->addEventListener("click", [d1, d2, &count](kui::Event&) {
        ++count;
        d1->getElementById("log")->setTextContent("Sent message #" + std::to_string(count));
        d2->getElementById("log2")->setTextContent("Received message #" + std::to_string(count) + " from Window A");
    });
    return kui::Platform::run();
}
