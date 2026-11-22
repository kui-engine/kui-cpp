#include <kui/kui.h>
#include <kui/component/custom_element.h>

int main() {
    kui::Platform::init();

    auto window = kui::Window::create({
        .title  = "KUI — Custom Element",
        .width  = 800,
        .height = 600,
    });

    auto* doc = window->document();

    int connected = 0;
    int disconnected = 0;
    int attributes = 0;
    auto registerElement = [doc, &connected, &disconnected, &attributes](const char* tag) {
        kui::CustomElementDefinition def;
        def.tagName = tag;
        def.factory = [tag](kui::Document* owner) { return kui::Element::create(owner, tag); };
        def.connectedCallback = [&connected](kui::Element*) { ++connected; };
        def.disconnectedCallback = [&disconnected](kui::Element*) { ++disconnected; };
        def.attributeChangedCallback = [&attributes](kui::Element*, const kui::String&,
                                                     const kui::String&, const kui::String&) { ++attributes; };
        doc->customElements()->define(tag, std::move(def));
    };
    registerElement("kui-card");
    registerElement("kui-badge");

    doc->loadHTML(R"(
<!DOCTYPE html>
<html><head><style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { padding: 24px; font-family: "Segoe UI", sans-serif; background: #f8fafc; }
h2 { margin-bottom: 16px; color: #1e293b; }
kui-card { display: block; background: white; padding: 20px; border-radius: 12px;
           box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 16px; }
kui-card h3 { color: #1e293b; margin-bottom: 8px; }
kui-card p { color: #64748b; font-size: 14px; }
kui-badge { display: inline-block; padding: 4px 12px; border-radius: 12px;
            font-size: 12px; font-weight: 600; }
kui-badge[type="success"] { background: #dcfce7; color: #166534; }
kui-badge[type="warning"] { background: #fef3c7; color: #92400e; }
kui-badge[type="error"] { background: #fef2f2; color: #991b1b; }
</style></head><body>
<h2>Custom Elements</h2>
<p id="lifecycle" style="margin-bottom:12px;color:#475569"></p>
<kui-card>
  <h3>Server Status</h3>
  <p>All systems operational <kui-badge type="success">Online</kui-badge></p>
</kui-card>
<kui-card>
  <h3>Database</h3>
  <p>High latency detected <kui-badge type="warning">Warning</kui-badge></p>
</kui-card>
<kui-card>
  <h3>API Gateway</h3>
  <p>Service unavailable <kui-badge type="error">Error</kui-badge></p>
</kui-card>
</body></html>
    )");

    auto* badge = doc->querySelector("kui-badge");
    if (badge) badge->setAttribute("data-live", "registered");
    doc->getElementById("lifecycle")->setTextContent(
        "Registry definitions: kui-card, kui-badge · connected=" + std::to_string(connected) +
        " · disconnected=" + std::to_string(disconnected) +
        " · attribute callbacks=" + std::to_string(attributes));

    return kui::Platform::run();
}
