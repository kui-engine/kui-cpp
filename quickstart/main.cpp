#include <kui/kui.h>

#include <cstdio>
#include <stdexcept>
#include <string>

int main() {
    if (!kui::Platform::init()) return 1;
    int exitCode = 0;
    int clicks = 0; // declared before the window, so it outlives the listener

    try {
        kui::WindowConfig config;
        config.title = "KUI Quick Start";
        config.width = 560; // client area
        config.height = 640;
        config.resizable = false;
        auto window = kui::Window::create(config); // Unique<Window>: sole owner

        kui::Document* doc = window->document(); // borrowed; lives as long as the window
        doc->loadFile("app.html");               // relative to the current directory
        kui::Element* count = doc->getElementById("count"); // borrowed; nullptr if missing
        kui::Element* button = doc->getElementById("inc");
        if (!count || !button) throw std::runtime_error("app.html not found in the current directory");

        button->addEventListener("click", [&clicks, count](kui::Event&) {
            count->setTextContent(std::to_string(++clicks)); // the DOM update
        });

        exitCode = kui::Platform::run(); // blocks until the window is closed
    } catch (const std::exception& e) { // KUI failures arrive as kui::Error
        std::fprintf(stderr, "%s\n", e.what());
        exitCode = 1;
    } // window destroyed here, together with its document, elements and listener

    kui::Platform::shutdown();
    return exitCode;
}
