#pragma once

#include <kui/kui.h>

namespace kui::examples {

inline void startLiveDevTools(Window* window) {
    if (!window) return;
    if (window->startDevTools(0)) {
        window->setTitle(window->title() + " — " + window->devToolsUrl());
    } else {
        window->setTitle(window->title() + " — DevTools failed to start");
    }
}

} // namespace kui::examples
