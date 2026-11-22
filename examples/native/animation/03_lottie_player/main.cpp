#include <kui/kui.h>
#include <kui/animation/lottie_player.h>
#include <kui/dom/html_lottie_element.h>
#include "../../common/demo_runtime.h"

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Lottie Player",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { display: flex; align-items: center; justify-content: center; height: 100vh;
       background: #0f172a; font-family: "Segoe UI", sans-serif; }
.card { background: #1e293b; padding: 32px; border-radius: 16px; text-align: center;
        box-shadow: 0 20px 60px rgba(0,0,0,0.5); }
.card h2 { color: #f8fafc; margin-bottom: 8px; }
.card p { color: #94a3b8; font-size: 14px; margin-bottom: 16px; }
lottie-player { display: block; width: 200px; height: 200px; margin: 0 auto; }
</style></head><body>
<div class="card">
  <h2>Lottie Animation</h2>
  <p>Renders Lottie JSON animations via KUI's built-in player.</p>
  <lottie-player id="lottie" loop></lottie-player>
  <p id="status">Loading bundled animation…</p>
</div>
</body></html>
    )");

    auto* player = dynamic_cast<kui::HTMLLottieElement*>(doc->getElementById("lottie"));
    const auto asset = kui::examples::findRuntimeAsset("native_animation/pulse.json");
    if (player && !asset.empty()) {
        player->setSrc(asset.string());
        player->play();
        doc->getElementById("status")->setTextContent(
            player->complete()
                ? "Playing bundled pulse.json · " + std::to_string(player->naturalWidth()) + " × " +
                      std::to_string(player->naturalHeight())
                : "Lottie decoder rejected pulse.json");
    } else {
        doc->getElementById("status")->setTextContent("Bundled pulse.json was not found.");
    }

    return kui::Platform::run();
}
