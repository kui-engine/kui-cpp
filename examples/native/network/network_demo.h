#pragma once

#include <kui/kui.h>
#include <kui/dom/html_input_element.h>
#include <kui/net/http_client.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace kui::examples {

struct NetworkDemoState {
    std::shared_ptr<HttpClient> client = std::make_shared<HttpClient>();
    std::mutex mutex;
    std::optional<HttpClient::Response> completed;
    bool pending = false;
};

inline String networkDemoUrl(Document* doc) {
    auto* input = dynamic_cast<HTMLInputElement*>(doc->getElementById("url"));
    return input ? input->value() : String("https://example.com/");
}

inline void showNetworkResponse(Document* doc, const HttpClient::Response& response) {
    auto* status = doc->getElementById("status");
    auto* output = doc->getElementById("response");
    if (!response.error.empty()) {
        status->setTextContent("Request failed");
        status->style()->setProperty("color", "#fca5a5");
        output->setTextContent(response.error);
        return;
    }
    status->setTextContent("HTTP " + std::to_string(response.statusCode) + " · " +
                           std::to_string(response.body.size()) + " bytes · " + response.contentType);
    status->style()->setProperty("color", "#86efac");
    const std::size_t count = std::min<std::size_t>(response.body.size(), 1800);
    output->setTextContent(std::string(response.body.begin(), response.body.begin() + count));
}

inline int runNetworkDemo(bool asynchronous) {
    if (!Platform::init()) return 2;
    const char* title = asynchronous ? "KUI — Async HTTP Request" : "KUI — HTTP Fetch";
    auto window = Window::create({.title = title, .width = 860, .height = 620});
    if (!window) return 3;
    auto* doc = window->document();
    doc->loadHTML(std::string(R"HTML(
<!DOCTYPE html><html><head><style>
*{box-sizing:border-box}body{margin:0;padding:32px;background:#0f172a;color:#e2e8f0;font-family:"Segoe UI",sans-serif}.card{max-width:760px;margin:auto;background:#1e293b;border:1px solid #334155;border-radius:14px;padding:22px}h1{font-size:22px;margin:0 0 7px;color:#60a5fa}.desc{color:#94a3b8;font-size:13px;margin-bottom:16px}.row{display:flex;gap:8px}input{flex:1;padding:9px 11px;background:#020617;border:1px solid #475569;border-radius:7px;color:#e2e8f0}.btn{padding:9px 15px;background:#2563eb;border-radius:7px;color:white;cursor:pointer;font-size:12px}#status{margin:14px 0 8px;color:#fbbf24;font-size:12px}#response{height:340px;overflow:auto;white-space:pre-wrap;background:#020617;border-radius:8px;padding:13px;color:#cbd5e1;font:12px Consolas,monospace}
</style></head><body><div class="card"><h1>)HTML") +
        (asynchronous ? "Asynchronous HTTP Request" : "Synchronous HTTP Fetch") +
        R"HTML(</h1><div class="desc">Uses kui::HttpClient and displays the real status, content type, byte count and response body.</div><div class="row"><input id="url" value="https://example.com/"><div class="btn" id="send">Send GET</div><div class="btn" id="cancel">Cancel</div></div><div id="status">Ready</div><div id="response">No request yet.</div></div></body></html>)HTML");

    auto state = std::make_shared<NetworkDemoState>();
    state->client->setUserAgent("KUI native network example/1.0");
    doc->getElementById("send")->addEventListener("click", [doc, state, asynchronous](Event&) {
        state->client = std::make_shared<HttpClient>();
        state->client->setUserAgent("KUI native network example/1.0");
        const auto url = networkDemoUrl(doc);
        doc->getElementById("status")->setTextContent("GET " + url + " ...");
        doc->getElementById("response")->setTextContent("Waiting for response...");
        if (!asynchronous) {
            showNetworkResponse(doc, state->client->fetchSync(url, 15.0));
            return;
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->pending = true;
            state->completed.reset();
        }
        state->client->fetchAsync(url, [state](HttpClient::Response response) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->completed = std::move(response);
            state->pending = false;
        }, 15.0);
    });
    doc->getElementById("cancel")->addEventListener("click", [doc, state](Event&) {
        state->client->cancelAll();
        doc->getElementById("status")->setTextContent("Cancellation requested");
    });
    Platform::setInterval([doc, state]() {
        std::optional<HttpClient::Response> response;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->completed) {
                response = std::move(state->completed);
                state->completed.reset();
            }
        }
        if (response) showNetworkResponse(doc, *response);
    }, 50);
    return Platform::run();
}

} // namespace kui::examples
