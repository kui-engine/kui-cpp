#include "../../common/demo_runtime.h"

int main() {
    return kui::examples::runLuaAsset(
        "KUI — WebGL Textured Quad", 960, 720,
        "native_samples/native_02_textured_quad.html");
}
