#include "../../common/demo_runtime.h"

int main() {
    return kui::examples::runLuaAsset(
        "KUI — WebGL Animated Scene", 960, 720,
        "native_samples/69_webgl_galaxy_shader.html");
}
