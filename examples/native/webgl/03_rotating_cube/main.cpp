#include "../../common/demo_runtime.h"

int main() {
    return kui::examples::runLuaAsset(
        "KUI — WebGL Rotating Cube", 960, 720,
        "native_samples/61_webgl_cube.html");
}
