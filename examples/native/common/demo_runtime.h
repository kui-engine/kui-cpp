#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <kui/kui.h>

#ifdef KUI_HAS_SCRIPTING
#include <kui/script/script_engine.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace kui::examples {

inline std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
    }
#elif defined(__linux__)
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return path.parent_path();
#endif
    return std::filesystem::current_path();
}

inline std::filesystem::path findRuntimeAsset(const std::filesystem::path& relative) {
    const auto exeDir = executableDirectory();
    const auto cwd = std::filesystem::current_path();
    const std::filesystem::path candidates[] = {
        exeDir / relative,
        exeDir / "assets" / relative,
        cwd / relative,
        cwd / "examples" / relative,
        cwd / ".." / relative,
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return std::filesystem::weakly_canonical(candidate, ec);
        }
    }
    return {};
}

inline std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

inline std::vector<std::string> extractLuaScripts(const std::string& html) {
    std::vector<std::string> scripts;
    std::string lower = html;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }

    std::size_t cursor = 0;
    while (true) {
        const auto open = lower.find("<script", cursor);
        if (open == std::string::npos) break;
        const auto tagEnd = lower.find('>', open);
        if (tagEnd == std::string::npos) break;
        const auto close = lower.find("</script>", tagEnd + 1);
        if (close == std::string::npos) break;

        const auto tag = lower.substr(open, tagEnd - open + 1);
        const bool javascript = tag.find("javascript") != std::string::npos;
        if (!javascript) {
            scripts.push_back(html.substr(tagEnd + 1, close - tagEnd - 1));
        }
        cursor = close + 9;
    }
    return scripts;
}

inline int runLuaDocument(const char* title, int width, int height,
                          std::string html, const std::string& sourceName) {
    if (!Platform::init()) return 2;

    auto window = Window::create({.title = title,
                                  .width = static_cast<u32>(width),
                                  .height = static_cast<u32>(height)});
    if (!window) return 3;
    auto* document = window->document();
    document->loadHTML(html);

#ifdef KUI_HAS_SCRIPTING
    auto scriptEngine = ScriptEngine::create();
    if (!scriptEngine) return 4;
    scriptEngine->bindDocument(document);
    window->setScriptEngine(scriptEngine.get());

    const auto scripts = extractLuaScripts(html);
    for (std::size_t index = 0; index < scripts.size(); ++index) {
        scriptEngine->eval(scripts[index], sourceName + "#" + std::to_string(index));
    }
#else
    if (auto* status = document->getElementById("status")) {
        status->setTextContent("Lua scripting is disabled in this KUI build.");
    }
#endif

    return Platform::run();
}

inline int runLuaAsset(const char* title, int width, int height,
                       const std::filesystem::path& relative) {
    const auto path = findRuntimeAsset(relative);
    if (path.empty()) {
        if (!Platform::init()) return 2;
        auto window = Window::create({.title = title,
                                      .width = static_cast<u32>(width),
                                      .height = static_cast<u32>(height)});
        if (!window) return 3;
        window->document()->loadHTML(
            "<html><body style='padding:32px;background:#111827;color:#fca5a5;"
            "font-family:sans-serif'><h2>Example asset is missing</h2><p>" +
            relative.generic_string() + "</p></body></html>");
        return Platform::run();
    }
    auto html = readTextFile(path);
    return runLuaDocument(title, width, height, std::move(html), path.string());
}

} // namespace kui::examples
