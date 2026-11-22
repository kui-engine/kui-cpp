// ===========================================================================
// _find_extensions_dir.h
//
// Header-only helper used by all `examples/native/extension/*` demos to
// locate the directory containing the kui_ext_*.dll plugin binaries for
// the current build.
//
// Why this exists:
//
//   The `Document::initExtensions(const std::string&)` SDK call accepts a
//   single directory path which is resolved relative to the process's
//   current working directory. That's perfect when the example is launched
//   from the repo root (`<repo-root>> .\build-x64-dll-md\Release\
//   native_extension_10_kui_editor.exe`), but wrong when the user
//   double-clicks the .exe from Explorer -- the cwd is then the .exe's
//   own directory and the hard-coded relative path can't see the
//   `build/extensions/Release/` sub-tree.
//
//   This helper resolves the path using the executable's own location
//   (via `GetModuleFileNameW` on Win32) and falls back through a handful
//   of well-known layouts so each launch mode "just works".
//
// Why local to examples/ (not promoted to SDK):
//
//   Adding `kui::Platform::executableDir()` to the SDK would be the
//   long-term right answer, but doing so would require re-shipping the
//   3.1.0 SDK binary. For now the helper lives here as a header-only
//   inline static function -- ~25 lines, zero CMake plumbing, zero ODR
//   risk across translation units.
// ===========================================================================
#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace kui_examples_internal {

// Returns the most specific directory that actually contains
// `kui_ext_editor.dll`, falling back to the executable's own directory
// if nothing matches (so the SDK still emits a diagnostic against a
// stable, grep-friendly path).
//
// Probe order is "most specific first":
//   1. The .exe's own directory          (in-tree build: kui_ext_*.dll
//                                          are deposited alongside .exe)
//   2. <exe_dir>/extensions               (SDK deploy convention)
//   3. Walk parents of <exe_dir> and <cwd>, checking each supported repo/
//      SDK layout. This is required when an example is launched with its
//      output directory as cwd (the normal IDE and test-runner behaviour).
inline std::string findExtensionsDir(const std::string& pluginStem = "kui_ext_editor") {
    namespace fs = std::filesystem;

#ifdef _WIN32
    wchar_t exeBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    fs::path exeDir = fs::path(exeBuf).parent_path();
#else
    fs::path exeDir = fs::current_path();
#endif

    const fs::path cwd = fs::current_path();
    std::vector<fs::path> candidates = {
        exeDir,
        exeDir / "extensions",
    };

    const auto appendRepositoryLayouts = [&candidates](fs::path anchor) {
        // The canonical Windows output directory is two levels below the
        // repository, but keep the walk bounded so arbitrary ancestors are
        // never searched indefinitely.
        for (int depth = 0; depth < 8 && !anchor.empty(); ++depth) {
            candidates.push_back(anchor / "extensions" / "kui_ext_3d" / "build" / "Release");
            candidates.push_back(anchor / "build-x64-dll-md" / "extensions" / "Release");
            candidates.push_back(anchor / "build" / "extensions" / "Release");
            candidates.push_back(anchor / "sdk" / "cpp" / "bin");

            const auto parent = anchor.parent_path();
            if (parent == anchor) {
                break;
            }
            anchor = parent;
        }
    };
    appendRepositoryLayouts(exeDir);
    if (cwd != exeDir) {
        appendRepositoryLayouts(cwd);
    }

    for (const auto& c : candidates) {
        std::error_code ec;
#ifdef _WIN32
        const auto pluginName = pluginStem + ".dll";
#elif defined(__APPLE__)
        const auto pluginName = "lib" + pluginStem + ".dylib";
#else
        const auto pluginName = "lib" + pluginStem + ".so";
#endif
        if (fs::exists(c / pluginName, ec)) {
            return c.lexically_normal().string();
        }
    }
    return exeDir.string();
}

} // namespace kui_examples_internal
