#pragma once

#include <cmath>

#include "../common/demo_runtime.h"

#include <kui/media/html_media_element.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace kui::examples {

inline std::string mediaTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) seconds = 0;
    const int total = static_cast<int>(seconds);
    std::ostringstream out;
    out << total / 60 << ':' << std::setw(2) << std::setfill('0') << total % 60;
    return out.str();
}

inline int runMediaDemo(int id) {
    if (!Platform::init()) return 2;
    const bool audio = id == 1;
    const char* title = id == 1 ? "Audio Player" : (id == 2 ? "Video Player" : "Media Controls");
    auto window = Window::create({.title = std::string("KUI — ") + title,
                                  .width = 900, .height = 680});
    if (!window) return 3;
    auto* doc = window->document();
    const std::string tag = audio ? "audio" : "video";
    doc->loadHTML(std::string(R"HTML(
<!DOCTYPE html><html><head><style>
*{box-sizing:border-box}body{margin:0;background:#0f172a;color:#e2e8f0;font-family:"Segoe UI",sans-serif;display:flex;align-items:center;justify-content:center;height:100vh}.player{width:700px;background:#1e293b;border:1px solid #334155;border-radius:16px;padding:24px;box-shadow:0 20px 55px #020617}h1{margin:0 0 5px;font-size:22px;color:#60a5fa}.desc{font-size:12px;color:#94a3b8;margin-bottom:17px}video{display:block;width:100%;height:370px;background:#020617;border-radius:10px}audio{display:block;width:100%;height:54px;background:#020617;border-radius:10px}.controls{display:flex;gap:8px;align-items:center;margin-top:14px}.btn{padding:8px 13px;background:#2563eb;color:white;border-radius:6px;cursor:pointer;font-size:12px}.btn.alt{background:#475569}.track{height:7px;background:#0f172a;border-radius:5px;overflow:hidden;flex:1}.fill{height:100%;width:0;background:linear-gradient(90deg,#38bdf8,#8b5cf6)}#time,#status{font:11px Consolas,monospace;color:#cbd5e1}#status{margin-top:12px;color:#fbbf24}
</style></head><body><div class="player"><h1>)HTML") + title + R"HTML(</h1><div class="desc">Real media adapter state, playback, seek, volume and mute controls.</div><)HTML" + tag +
        R"HTML( id="media" controls></)HTML" + tag + R"HTML(><div class="controls"><div class="btn" id="play">Play</div><div class="btn alt" id="pause">Pause</div><div class="btn alt" id="back">-10s</div><div class="btn alt" id="forward">+10s</div><div class="btn alt" id="mute">Mute</div><div class="track"><div class="fill" id="fill"></div></div><div id="time">0:00 / 0:00</div></div><div id="status">Initializing media backend...</div></div></body></html>)HTML");

    auto* media = dynamic_cast<HTMLMediaElement*>(doc->getElementById("media"));
    if (!media) return 4;
    const auto asset = findRuntimeAsset(audio ? "native_media/sample.mp3" : "native_media/sample.mp4");
    auto* status = doc->getElementById("status");
    if (asset.empty()) {
        status->setTextContent("Media asset is missing beside the executable.");
    } else {
        media->setSrc(asset.string());
        media->setControls(true);
        media->setVolume(0.75f);
        status->setTextContent(media->isMediaAvailable()
            ? "Backend available · source loaded: " + asset.filename().string()
            : "Media plugin is unavailable. Deploy kui_media_audio/video next to the executable.");
    }

    doc->getElementById("play")->addEventListener("click", [media](Event&) { media->play(); });
    doc->getElementById("pause")->addEventListener("click", [media](Event&) { media->pause(); });
    doc->getElementById("back")->addEventListener("click", [media](Event&) {
        media->setCurrentTime(std::max(0.0, media->currentTime() - 10.0));
    });
    doc->getElementById("forward")->addEventListener("click", [media](Event&) {
        const double target = media->currentTime() + 10.0;
        media->setCurrentTime(media->duration() > 0 ? (std::min)(target, media->duration()) : target);
    });
    doc->getElementById("mute")->addEventListener("click", [media, doc](Event&) {
        media->setMuted(!media->muted());
        doc->getElementById("mute")->setTextContent(media->muted() ? "Unmute" : "Mute");
    });
    Platform::setInterval([media, doc]() {
        const double duration = media->duration();
        const double current = media->currentTime();
        doc->getElementById("time")->setTextContent(mediaTime(current) + " / " + mediaTime(duration));
        const double percent = duration > 0 ? std::clamp(current / duration * 100.0, 0.0, 100.0) : 0.0;
        doc->getElementById("fill")->style()->setProperty("width", std::to_string(percent) + "%");
        doc->getElementById("status")->setTextContent(
            std::string(media->paused() ? "Paused" : "Playing") +
            " · volume=" + std::to_string(media->volume()) +
            " · readyState=" + std::to_string(static_cast<int>(media->readyState())) +
            " · networkState=" + std::to_string(static_cast<int>(media->networkState())));
    }, 200);
    return Platform::run();
}

} // namespace kui::examples
