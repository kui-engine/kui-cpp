#include "../../common/demo_runtime.h"

int main() {
    return kui::examples::runLuaAsset(
        "KUI — WebGL Triangle", 960, 720,
        "native_samples/60_webgl_triangle.html");
}
