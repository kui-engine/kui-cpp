#include "../../common/demo_runtime.h"

int main() {
    return kui::examples::runLuaAsset(
        "KUI — WebGL Framebuffer", 960, 720,
        "native_samples/native_05_framebuffer.html");
}
