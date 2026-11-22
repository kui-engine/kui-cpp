#include <kui/kui.h>

#include <iostream>
#include <type_traits>

namespace {

int passed = 0;
int failed = 0;

#define CHECK(condition, message)                                                \
    do {                                                                         \
        if (condition) ++passed;                                                  \
        else { ++failed; std::cerr << "FAIL: " << (message) << '\n'; }           \
    } while (false)

}  // namespace

int main() {
    static_assert(std::is_same_v<kui::String, std::string>);
    static_assert(KUI_C_API_VERSION_MAJOR == 3);
    // 这条断言确认 Wrapper 编译时看到的是当前 C SDK 头，而不是残留的旧头。
    // MINOR 字面量是版本载体，由 release_sdk_full.py 的 sync_c_api_version()
    // 随根 VERSION 一起改写；不要手工让它漂移。
    static_assert(KUI_C_API_VERSION_MINOR == 61);

    auto document = kui::Document::create();
    document->loadHTML(
        "<html><body><main id='host'><button id='button' class='primary'>Go</button>"
        "</main></body></html>");

    auto* host = document->getElementById("host");
    auto* button = document->querySelector("button.primary");
    CHECK(host != nullptr, "getElementById returns a pointer");
    CHECK(button != nullptr, "querySelector returns a pointer");
    CHECK(button->tagName() == "button", "tagName matches native spelling");
    CHECK(button->className() == "primary", "className reads through C ABI");

    button->addClass("active");
    CHECK(button->hasClass("active"), "addClass/hasClass");
    button->toggleClass("active");
    CHECK(!button->hasClass("active"), "toggleClass");
    button->setClassName("cta");
    CHECK(button->className() == "cta", "setClassName");

    button->style()->setProperty("width", "120px");
    CHECK(button->style()->getPropertyValue("width") == "120px", "inline style");
    button->setTextContent("Run");
    CHECK(button->textContent() == "Run", "textContent");

    auto created = document->createElement("span");
    created->setTextContent("new");
    host->appendChild(created);
    CHECK(host->childElementCount() == 2, "appendChild with RefPtr");
    CHECK(host->lastElementChild() == created.get(), "stable pointer proxy");
    host->removeChild(created.get());
    CHECK(host->childElementCount() == 1, "removeChild");
    CHECK(created->textContent() == "new", "removed RefPtr remains alive");

    host->setInnerHTML("<i class='item'>a</i><i class='item'>b</i>");
    const auto items = host->querySelectorAll(".item");
    CHECK(items.size() == 2, "Element::querySelectorAll");
    CHECK(document->querySelectorAll(".item").size() == 2,
          "Document::querySelectorAll");

    int calls = 0;
    const kui::u32 listener = host->addEventListener("change", [&](kui::Event& event) {
        ++calls;
        CHECK(event.type() == "change", "Event::type");
        CHECK(event.target() == host, "Event::target proxy identity");
    });
    CHECK(listener != 0, "native-like listener id");
    CHECK(host->dispatchEvent("change"), "event was not canceled");
    CHECK(calls == 1, "listener survives ignored return value semantics");
    host->removeEventListener("change", listener);
    host->dispatchEvent("change");
    CHECK(calls == 1, "removeEventListener");

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
