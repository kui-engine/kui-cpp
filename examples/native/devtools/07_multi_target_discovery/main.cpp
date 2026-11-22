// ===========================================================================
// 07_multi_target_discovery — DevTools 统一发现入口 (KUI-039, SDK 3.16.0).
//
// 演示「一个地址发现全部 target」(Chrome `chrome://inspect` / Playwright /
// puppeteer 兼容):本进程开 3 个 KUI 窗口,每个窗口各自 startDevTools() 监听
// 一个独立端口 (9229 / 9230 / 9231)。得益于 KUI-039 的跨进程发现表,连接
// **任意一个**端口的 `GET /json` 都会聚合返回本机全部活跃 KUI target —— 无需
// 预先知道每个端口逐个连。
//
// 真实 API 出处:
//   * kui::Platform::init() / run()           include/kui/platform/platform.h
//   * kui::Window::create(WindowConfig)        include/kui/platform/window.h:61
//   * Window::startDevTools(u16 port)          include/kui/platform/window.h:160
//     -> 返回 DevToolsServer*;源码 src/platform/window_win32.cpp
//   * Window::devToolsUrl()                    include/kui/platform/window.h:162
//   * 聚合 /json 路由                          src/devtools/devtools_server.cpp:561
//     -> buildAggregatedTargetListJson + 跨进程注册表 src/devtools/devtools_registry.*
//
// 验证(headless 无法验,待真机 / MCP):
//   1. 运行本 demo,3 个窗口弹出。
//   2. 浏览器访问 http://localhost:9229/json (或 9230 / 9231 任意一个),
//      应看到含 3 个 target 的聚合 JSON,各 webSocketDebuggerUrl 指向真实端口。
//   3. kui-devtools MCP: kui_targets() 列出 3 个；从本应用日志取得精确 target id，
//      再用 kui_connect(port, target="kui-<实际端口>") 连接自己的窗口。
// ===========================================================================

#include <kui/kui.h>
#include <kui/devtools/devtools_server.h>

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

// 每个窗口的说明页。port 为本窗口实际监听的 DevTools 端口。
kui::String makePage(int appIndex, int total, kui::u16 port, kui::u16 basePort) {
    char buf[4096];
    std::snprintf(buf, sizeof(buf), R"(<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{padding:32px;font-family:"Segoe UI","Microsoft YaHei",sans-serif;
     background:linear-gradient(135deg,#0f172a 0%%,#1e293b 100%%);color:#f8fafc;min-height:100vh}
.tag{display:inline-block;padding:4px 12px;border-radius:12px;background:#1e3a8a;
     color:#bfdbfe;font-size:12px;font-weight:600;letter-spacing:.5px;margin-bottom:16px}
h1{font-size:34px;margin-bottom:8px}
.port{font-family:"Cascadia Code",Consolas,monospace;color:#38bdf8;font-weight:700}
.card{background:#1e293b;border:1px solid #334155;border-radius:12px;
      padding:20px 24px;margin-top:20px}
.card h3{font-size:14px;color:#93c5fd;margin-bottom:10px;text-transform:uppercase;letter-spacing:1px}
.card p{color:#cbd5e1;font-size:15px;line-height:1.7}
code{font-family:"Cascadia Code",Consolas,monospace;background:#0f172a;
     padding:2px 8px;border-radius:6px;color:#fcd34d}
.hl{color:#4ade80;font-weight:700}
.steps{margin-top:8px}
.steps li{list-style:none;padding:8px 0;color:#cbd5e1;font-size:14px;
          border-bottom:1px solid #1e293b}
.num{display:inline-block;width:24px;height:24px;border-radius:50%%;background:#3b82f6;
     color:#fff;text-align:center;line-height:24px;font-size:12px;font-weight:700;margin-right:10px}
</style></head><body>
<span class="tag">KUI-039 · SDK 3.16.0 · DevTools 统一发现</span>
<h1>我是 App %d / %d</h1>
<p style="font-size:17px;color:#94a3b8;">本窗口 DevTools 监听端口 <span class="port">%u</span></p>
<div class="card">
  <h3>一个地址发现全部</h3>
  <p>在浏览器或 MCP 连接 <code>localhost:%u/json</code>,即可<span class="hl">聚合发现</span>
     本机全部 %d 个活跃 KUI target —— 连任意一个端口(%u..%u)都一样。</p>
</div>
<div class="card">
  <h3>试一试</h3>
  <ul class="steps">
    <li><span class="num">1</span>浏览器打开 <code>http://localhost:%u/json</code></li>
    <li><span class="num">2</span>看到 %d 个 target 的聚合 JSON,各 <code>webSocketDebuggerUrl</code> 指向真实端口</li>
    <li><span class="num">3</span>Chrome <code>chrome://inspect</code> 配 <code>localhost:%u</code> 即可发现全部</li>
    <li><span class="num">4</span>本窗口精确 id 是 <code>kui-%u</code>；使用 <code>kui_connect(target="kui-%u")</code></li>
  </ul>
</div>
</body></html>)",
                  appIndex + 1, total, port,
                  port, total, basePort, static_cast<kui::u16>(basePort + total - 1),
                  port, total, basePort, port, port);
    return kui::String(buf);
}

} // namespace

int main() {
    kui::Platform::init();

    constexpr int            kAppCount  = 3;
    constexpr kui::u16       kBasePort  = 9229;

    // 持有所有窗口,使其生命周期延续到 Platform::run() 结束。
    std::vector<kui::Unique<kui::Window>> windows;
    windows.reserve(kAppCount);

    for (int i = 0; i < kAppCount; ++i) {
        const kui::u16 port = static_cast<kui::u16>(kBasePort + i);

        auto window = kui::Window::create({
            .title  = kui::String("KUI App ") + std::to_string(i + 1) +
                      " — DevTools :" + std::to_string(port),
            .width  = 760,
            .height = 560,
        });

        // 每个窗口在独立端口启动 DevTools 服务器。start() 会把本 target 注册到
        // 跨进程发现表(%TEMP%\kui-devtools\<port>.json),故任意端口的 /json 都能聚合。
        kui::DevToolsServer* server = window->startDevTools(port);
        if (server) {
            const kui::u16 actualPort = server->port();
            std::fprintf(stderr,
                         "[multi-target] App %d target=kui-%u port=%u\n",
                         i + 1, actualPort, actualPort);
            window->document()->loadHTML(
                makePage(i, kAppCount, actualPort, kBasePort));
        } else {
            std::fprintf(stderr,
                         "[multi-target] App %d startDevTools(%u) failed "
                         "(请求端口及后续兜底端口均不可用)\n",
                         i + 1, port);
            window->document()->loadHTML(
                "<h1>DevTools 启动失败</h1><p>请求端口及后续兜底端口均不可用。</p>");
        }

        windows.push_back(std::move(window));
    }

    std::fprintf(stderr,
                 "[multi-target] %d 个窗口已启动。浏览器访问 "
                 "http://localhost:%u/json 聚合发现全部 target。\n",
                 kAppCount, kBasePort);

    return kui::Platform::run();
}
