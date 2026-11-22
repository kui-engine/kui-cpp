# KUI C++ Wrapper examples

`wrapper_demo.cpp` 是最小入门程序。C++ Wrapper 发布包（`kui-cpp-wrapper-sdk-<版本>-<平台>.zip`）还会在本目录交付：

- `native/`：189 个与原生 C++ SDK 共用源码的 examples，以及 `native/api/` 下的 C++ API 巡览示例；
- `native_compat.cmake`：189 个同源 target 的显式清单和数量硬门，末尾生成块登记 C++ API 巡览；
- `assets/`：Canvas/WebGL、Lottie 和 media 示例所需的运行素材。

仓库开发态不复制 `examples/native`，而是直接引用权威源码；Wrapper 包打包时生成上述
source snapshot（Windows / Linux / macOS 相同）。解压后可独立构建：

```powershell
cmake -S cpp_wrapper -B build-wrapper `
  -DKUI_C_ROOT=<解压后的 Wrapper 包根目录> `
  -DKUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES=ON
cmake --build build-wrapper --config Release
```

CMake 配置必须报告：

```text
kui_cpp: 189 native example sources use the C ABI facade
```
