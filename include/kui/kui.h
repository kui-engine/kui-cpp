#pragma once

// Source-compatible core facade over KUI's pure C ABI. This header is compiled
// into the consumer and intentionally has no dependency on the native C++ SDK
// headers or ABI.
#include <kui_c.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#define KUI_CPP_STRINGIFY_IMPL(value) #value
#define KUI_CPP_STRINGIFY(value) KUI_CPP_STRINGIFY_IMPL(value)

// Linkage of the Wrapper's own definitions. The Wrapper defines kui::Document, kui::Element ...
// with the native C++ SDK's names, i.e. the same mangled symbols as the native libkui that
// libkui_c is built on. On ELF (Linux) an executable exports every symbol a linked shared library
// references, so without this the dynamic linker binds libkui_c's calls into libkui to the
// Wrapper's inline copies in the executable -- kui_document_create() recursing into the Wrapper's
// Document::create() until the stack overflows. Hidden visibility keeps each Wrapper definition
// inside its own module; source names and compatibility are unchanged. PE (Windows) has no such
// interposition and Mach-O's two-level namespace binds libkui_c to libkui directly.
#ifndef KUI_CPP_HIDDEN_BEGIN
#if defined(__GNUC__) && !defined(_WIN32)
#define KUI_CPP_HIDDEN_BEGIN _Pragma("GCC visibility push(hidden)")
#define KUI_CPP_HIDDEN_END _Pragma("GCC visibility pop")
#else
#define KUI_CPP_HIDDEN_BEGIN
#define KUI_CPP_HIDDEN_END
#endif
#endif

KUI_CPP_HIDDEN_BEGIN
namespace kui {

using u8 = std::uint8_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using String = std::string;

namespace detail {
template <typename T, typename = void>
struct SharesFromThis : std::false_type {};
template <typename T>
struct SharesFromThis<T, std::void_t<decltype(std::declval<T&>().weak_from_this())>> : std::true_type {};
}  // namespace detail

// The wrapper's reference-counted pointer: a std::shared_ptr, plus the native
// RefPtr's raw-pointer constructor. Native code writes RefPtr<Element>(raw) to
// add a reference to an object something else already holds; here that shares
// the existing ownership (Element and Document are enable_shared_from_this)
// instead of starting a second owner that would free the object twice.
template <typename T>
class RefPtr : public std::shared_ptr<T> {
    using Base = std::shared_ptr<T>;

public:
    constexpr RefPtr() noexcept = default;
    constexpr RefPtr(std::nullptr_t) noexcept {}
    template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    RefPtr(const std::shared_ptr<U>& other) noexcept : Base(other) {}
    template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    RefPtr(std::shared_ptr<U>&& other) noexcept : Base(std::move(other)) {}
    // Like the native RefPtr(T*, addRef): an object that is already owned is
    // shared; a fresh one (from `new`) is adopted. addRef is accepted for source
    // compatibility; ownership is shared either way.
    template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    explicit RefPtr(U* raw, bool addRef = true) : Base(share(raw)) {
        (void)addRef;
    }

private:
    template <typename U>
    static Base share(U* raw) {
        if (!raw) return Base();
        if constexpr (detail::SharesFromThis<U>::value) {
            if (auto owner = raw->weak_from_this().lock()) return Base(owner, static_cast<T*>(raw));
        }
        return Base(raw);
    }
};
template <typename T> using Unique = std::unique_ptr<T>;
template <typename T> using Optional = std::optional<T>;

inline constexpr int kVersionMajor = KUI_C_API_VERSION_MAJOR;
inline constexpr int kVersionMinor = KUI_C_API_VERSION_MINOR;
inline constexpr int kVersionPatch = KUI_C_API_VERSION_PATCH;
inline constexpr const char* kVersionString =
    KUI_CPP_STRINGIFY(KUI_C_API_VERSION_MAJOR) "."
    KUI_CPP_STRINGIFY(KUI_C_API_VERSION_MINOR) "."
    KUI_CPP_STRINGIFY(KUI_C_API_VERSION_PATCH);

class Error : public std::runtime_error {
public:
    Error(KuiStatus status, const char* context)
        : std::runtime_error(std::string(context) + ": " + kui_status_message(status)),
          status_(status) {}

    KuiStatus status() const noexcept { return status_; }

private:
    KuiStatus status_;
};

namespace detail {

inline void check(KuiStatus status, const char* context) {
    if (status != KUI_OK) throw Error(status, context);
}

template <typename Fn>
inline String readString(Fn&& fn, const char* context) {
    std::size_t size = 0;
    const KuiStatus first = fn(nullptr, 0, &size);
    if (first == KUI_ERR_NOT_FOUND) return {};
    check(first, context);
    if (size == 0) return {};
    String result(size + 1, '\0');
    check(fn(result.data(), result.size(), &size), context);
    result.resize(size);
    return result;
}

struct DocumentState;

}  // namespace detail

struct Point {
    f32 x = 0;
    f32 y = 0;
};

struct Size {
    f32 width = 0;
    f32 height = 0;
};

struct Rect {
    f32 x = 0;
    f32 y = 0;
    f32 width = 0;
    f32 height = 0;
};

struct Color {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 255;
    constexpr Color() = default;
    constexpr Color(u8 r_, u8 g_, u8 b_, u8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    constexpr bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

namespace detail {
inline uint32_t packColor(const Color& c) {
    return (uint32_t(c.r) << 24) | (uint32_t(c.g) << 16) | (uint32_t(c.b) << 8) | uint32_t(c.a);
}
inline Color unpackColor(uint32_t v) {
    return Color(static_cast<u8>(v >> 24), static_cast<u8>(v >> 16), static_cast<u8>(v >> 8), static_cast<u8>(v));
}
}  // namespace detail

// Elliptical corner radii (native kui::CornerRadii).
struct CornerRadii {
    Size topLeft;
    Size topRight;
    Size bottomRight;
    Size bottomLeft;
    constexpr CornerRadii() = default;
    constexpr explicit CornerRadii(f32 uniform)
        : topLeft{uniform, uniform}, topRight{uniform, uniform},
          bottomRight{uniform, uniform}, bottomLeft{uniform, uniform} {}
};

// Font for Element::measureText (and the drawText arguments of a custom paint).
struct TextStyle {
    String fontFamily = "sans-serif";
    f32 fontSize = 14.0f;
    u16 fontWeight = 400;
    bool italic = false;
    f32 letterSpacing = 0;
    f32 wordSpacing = 0;
};

struct TextMetrics {
    f32 width = 0;
    f32 ascent = 0;
    f32 descent = 0;
    f32 lineHeight = 0;
};

struct WindowFrameRadii {
    f32 topLeft = 0.0f;
    f32 topRight = 0.0f;
    f32 bottomRight = 0.0f;
    f32 bottomLeft = 0.0f;

    constexpr WindowFrameRadii() = default;
    constexpr explicit WindowFrameRadii(f32 uniform)
        : topLeft(uniform), topRight(uniform), bottomRight(uniform), bottomLeft(uniform) {}
    constexpr WindowFrameRadii(f32 tl, f32 tr, f32 br, f32 bl)
        : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}
    constexpr bool isUniform() const {
        return topLeft == topRight && topRight == bottomRight && bottomRight == bottomLeft;
    }
};

struct WindowConfig {
    String title = "KUI Window";
    u32 width = 1280;
    u32 height = 720;
    u32 minWidth = 0;
    u32 minHeight = 0;
    u32 maxWidth = 0;
    u32 maxHeight = 0;
    bool resizable = true;
    bool decorated = true;
    bool transparent = false;
    bool alwaysOnTop = false;
    bool maximized = false;
    bool centered = true;
    f32 opacity = 1.0f;
    bool frameless = false;
    bool shadow = false;
    bool sizeable = false;
    bool visible = true;
    Optional<f32> frameRadius;
};

class Element;
class Document;
class ScriptEngine;
class ExtensionManager;
class CustomElementRegistry;

enum class EventTypeTag : std::uint8_t {
    Base = KUI_C_EVENT_BASE,
    Mouse = KUI_C_EVENT_MOUSE,
    Wheel = KUI_C_EVENT_WHEEL,
    Pointer = KUI_C_EVENT_POINTER,
    Keyboard = KUI_C_EVENT_KEYBOARD,
    Focus = KUI_C_EVENT_FOCUS,
    Input = KUI_C_EVENT_INPUT,
    Drag = KUI_C_EVENT_DRAG,
};

class DevToolsServer {
public:
    u16 port() const noexcept { return port_; }
    // Section 14 (3.61.8). The access token is part of url().
    bool isRunning() const {
        int running = 0;
        detail::check(kui_window_get_devtools_state(window_, &running, nullptr, nullptr), "DevToolsServer::isRunning");
        return running != 0;
    }
    u32 clientCount() const {
        uint32_t clients = 0;
        detail::check(kui_window_get_devtools_state(window_, nullptr, nullptr, &clients), "DevToolsServer::clientCount");
        return clients;
    }
    void stop() { detail::check(kui_window_stop_devtools(window_), "DevToolsServer::stop"); }
    // Turns the WebSocket token check on or off (off: any local client may connect).
    void setTokenAuth(bool enabled) {
        detail::check(kui_window_set_devtools_token_auth(window_, enabled ? 1 : 0), "DevToolsServer::setTokenAuth");
    }
    String url() const {
        return detail::readString([this](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_window_get_devtools_url(window_, buffer, capacity, size);
        }, "DevToolsServer::url");
    }
private:
    u16 port_ = 0;
    KuiWindow* window_ = nullptr;
    bool started_ = false;
    friend class Window;
};

enum class DropEffect : u8 { None = 0, Copy = 1, Move = 2, Link = 3 };
enum class EffectAllowed : u8 {
    None = 0, Copy = 1, Move = 2, Link = 3, CopyMove = 4, CopyLink = 5, MoveLink = 6,
    All = 7, Uninitialized = 8,
};

// The data of the current drag event (native DataTransfer). Reads and writes go straight to the
// event, so it is only valid inside that event's listener.
class DataTransfer {
public:
    void setData(const String& format, const String& data) {
        detail::check(kui_event_drag_set_data(event_, format.data(), format.size(), data.data(),
                                              data.size()), "DataTransfer::setData");
    }
    // Empty when no data of that format was set (same as native).
    String getData(const String& format) const {
        return detail::readString([this, &format](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_event_drag_get_data(event_, format.data(), format.size(), buffer, capacity, size);
        }, "DataTransfer::getData");
    }
    bool hasData(const String& format) const {
        std::size_t size = 0;
        const KuiStatus status =
            kui_event_drag_get_data(event_, format.data(), format.size(), nullptr, 0, &size);
        if (status == KUI_ERR_NOT_FOUND) return false;
        detail::check(status, "DataTransfer::hasData");
        return true;
    }
    void clearData(const String& format) {
        detail::check(kui_event_drag_clear_data(event_, format.data(), format.size()),
                      "DataTransfer::clearData");
    }
    void clearData() { detail::check(kui_event_drag_clear_data(event_, nullptr, 0), "DataTransfer::clearData"); }
    // The formats set so far, in order.
    const std::vector<String>& types() const {
        std::size_t count = 0;
        detail::check(kui_event_drag_get_type_count(event_, &count), "DataTransfer::types(count)");
        types_.clear();
        types_.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            types_.push_back(detail::readString(
                [this, index](char* buffer, std::size_t capacity, std::size_t* size) {
                    return kui_event_drag_get_type(event_, index, buffer, capacity, size);
                }, "DataTransfer::types"));
        }
        return types_;
    }
    const std::vector<String>& files() const noexcept { return files_; }
    DropEffect dropEffect() const {
        KuiCDropEffect value = KUI_C_DROP_NONE;
        detail::check(kui_event_drag_get_drop_effect(event_, &value), "DataTransfer::dropEffect");
        return static_cast<DropEffect>(value);
    }
    void setDropEffect(DropEffect effect) {
        detail::check(kui_event_drag_set_drop_effect(event_, static_cast<KuiCDropEffect>(effect)),
                      "DataTransfer::setDropEffect");
    }
    EffectAllowed effectAllowed() const {
        KuiCEffectAllowed value = KUI_C_EFFECT_NONE;
        detail::check(kui_event_drag_get_effect_allowed(event_, &value), "DataTransfer::effectAllowed");
        return static_cast<EffectAllowed>(value);
    }
    void setEffectAllowed(EffectAllowed effect) {
        detail::check(kui_event_drag_set_effect_allowed(event_, static_cast<KuiCEffectAllowed>(effect)),
                      "DataTransfer::setEffectAllowed");
    }
private:
    KuiEvent* event_ = nullptr;
    std::vector<String> files_;
    mutable std::vector<String> types_;
    friend class Event;
};

class Event {
public:
    virtual ~Event() = default;
    String type() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_event_get_type(raw_, buffer, capacity, size);
            }, "Event::type");
    }

    Element* target() const;
    Element* currentTarget() const;
    EventTypeTag typeTag() const {
        KuiCEventTypeTag value = KUI_C_EVENT_BASE;
        detail::check(kui_event_get_type_tag(raw_, &value), "Event::typeTag");
        return static_cast<EventTypeTag>(value);
    }
    std::vector<String> dragFiles() const {
        std::size_t count = 0;
        detail::check(kui_event_drag_get_file_count(raw_, &count), "Event::dragFiles(count)");
        std::vector<String> result;
        result.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            result.push_back(detail::readString(
                [this, index](char* buffer, std::size_t capacity, std::size_t* size) {
                    return kui_event_drag_get_file(raw_, index, buffer, capacity, size);
                }, "Event::dragFiles"));
        }
        return result;
    }
    // nullptr for an event that is not a drag event (native code cannot reach a DataTransfer there).
    DataTransfer* dataTransfer() const {
        std::size_t count = 0;
        if (kui_event_drag_get_type_count(raw_, &count) == KUI_ERR_NOT_SUPPORTED) return nullptr;
        dataTransfer_.event_ = raw_;
        dataTransfer_.files_ = dragFiles();
        return &dataTransfer_;
    }

    void preventDefault() { detail::check(kui_event_prevent_default(raw_), "Event::preventDefault"); }
    void stopPropagation() { detail::check(kui_event_stop_propagation(raw_), "Event::stopPropagation"); }

    bool bubbles() const {
        int value = 0;
        detail::check(kui_event_get_flags(raw_, &value, nullptr, nullptr), "Event::bubbles");
        return value != 0;
    }

    bool cancelable() const {
        int value = 0;
        detail::check(kui_event_get_flags(raw_, nullptr, &value, nullptr), "Event::cancelable");
        return value != 0;
    }

    bool defaultPrevented() const {
        int value = 0;
        detail::check(kui_event_get_flags(raw_, nullptr, nullptr, &value),
                      "Event::defaultPrevented");
        return value != 0;
    }

    // The C handle, for the kui_event_* functions the Wrapper does not wrap. Valid only inside the listener.
    KuiEvent* cHandle() const noexcept { return raw_; }

protected:
    Event(KuiEvent* raw, std::shared_ptr<detail::DocumentState> document)
        : raw_(raw), document_(std::move(document)) {}

    KuiEvent* raw_ = nullptr;
    std::shared_ptr<detail::DocumentState> document_;
    mutable DataTransfer dataTransfer_;

    template <typename> friend struct detail_listener_holder;
};

using DragEvent = Event;

class MouseEvent : public Event {
public:
    f32 clientX() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_client_xy(raw_, &x, &y), "MouseEvent::clientX"); return x; }
    f32 clientY() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_client_xy(raw_, &x, &y), "MouseEvent::clientY"); return y; }
    f32 pageX() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_page_xy(raw_, &x, &y), "MouseEvent::pageX"); return x; }
    f32 pageY() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_page_xy(raw_, &x, &y), "MouseEvent::pageY"); return y; }
    int button() const { int value = 0; detail::check(kui_event_mouse_get_button(raw_, &value), "MouseEvent::button"); return value; }
    // Position relative to the target's padding box.
    f32 offsetX() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_offset_xy(raw_, &x, &y), "MouseEvent::offsetX"); return x; }
    f32 offsetY() const { f32 x = 0, y = 0; detail::check(kui_event_mouse_get_offset_xy(raw_, &x, &y), "MouseEvent::offsetY"); return y; }
    // mouseover / mouseout / mouseenter / mouseleave 的另一端；<kui-context-menu> 的 menu-open 里是被右键的元素。
    Element* relatedTarget() const;

private:
    using Event::Event;
    template <typename> friend struct detail_listener_holder;
};

// A "wheel" event: the scroll amount of the wheel / trackpad gesture.
class WheelEvent : public MouseEvent {
public:
    f32 deltaX() const { f32 x = 0, y = 0; detail::check(kui_event_wheel_get_delta(raw_, &x, &y), "WheelEvent::deltaX"); return x; }
    f32 deltaY() const { f32 x = 0, y = 0; detail::check(kui_event_wheel_get_delta(raw_, &x, &y), "WheelEvent::deltaY"); return y; }

private:
    using MouseEvent::MouseEvent;
    template <typename> friend struct detail_listener_holder;
};

class KeyboardEvent : public Event {
public:
    String key() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_event_key_get_key(raw_, buffer, capacity, size);
            }, "KeyboardEvent::key");
    }
    String code() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_event_key_get_code(raw_, buffer, capacity, size);
            }, "KeyboardEvent::code");
    }
    bool ctrlKey() const { int v = 0; detail::check(kui_event_key_get_modifiers(raw_, &v, nullptr, nullptr, nullptr), "KeyboardEvent::ctrlKey"); return v != 0; }
    bool shiftKey() const { int v = 0; detail::check(kui_event_key_get_modifiers(raw_, nullptr, &v, nullptr, nullptr), "KeyboardEvent::shiftKey"); return v != 0; }
    bool altKey() const { int v = 0; detail::check(kui_event_key_get_modifiers(raw_, nullptr, nullptr, &v, nullptr), "KeyboardEvent::altKey"); return v != 0; }
    bool metaKey() const { int v = 0; detail::check(kui_event_key_get_modifiers(raw_, nullptr, nullptr, nullptr, &v), "KeyboardEvent::metaKey"); return v != 0; }

private:
    using Event::Event;
    template <typename> friend struct detail_listener_holder;
};

struct detail_listener_holder_base {
    explicit detail_listener_holder_base(std::shared_ptr<detail::DocumentState> state)
        : document(std::move(state)) {}
    virtual ~detail_listener_holder_base() = default;
    virtual void invoke(KuiEvent* event) noexcept = 0;
    static void trampoline(KuiEvent* event, void* userData) noexcept {
        static_cast<detail_listener_holder_base*>(userData)->invoke(event);
    }
    std::shared_ptr<detail::DocumentState> document;
};

template <typename Fn>
struct detail_listener_holder final : detail_listener_holder_base {
    detail_listener_holder(std::shared_ptr<detail::DocumentState> state, Fn&& callback)
        : detail_listener_holder_base(std::move(state)), fn(std::forward<Fn>(callback)) {}

    void invoke(KuiEvent* raw) noexcept override {
        try {
            if constexpr (std::is_invocable_v<Fn&, Event&>) {
                KuiCEventTypeTag tag = KUI_C_EVENT_BASE;
                if (kui_event_get_type_tag(raw, &tag) != KUI_OK)
                    tag = KUI_C_EVENT_BASE;
                if (tag == KUI_C_EVENT_KEYBOARD) {
                    KeyboardEvent event(raw, this->document);
                    fn(event);
                } else if (tag == KUI_C_EVENT_WHEEL) {
                    WheelEvent event(raw, this->document);
                    fn(event);
                } else if (tag == KUI_C_EVENT_MOUSE ||
                           tag == KUI_C_EVENT_POINTER || tag == KUI_C_EVENT_DRAG) {
                    MouseEvent event(raw, this->document);
                    fn(event);
                } else {
                    Event event(raw, this->document);
                    fn(event);
                }
            } else if constexpr (std::is_invocable_v<Fn&, WheelEvent&>) {
                WheelEvent event(raw, this->document);
                fn(event);
            } else if constexpr (std::is_invocable_v<Fn&, MouseEvent&>) {
                MouseEvent event(raw, this->document);
                fn(event);
            } else if constexpr (std::is_invocable_v<Fn&, KeyboardEvent&>) {
                KeyboardEvent event(raw, this->document);
                fn(event);
            } else if constexpr (std::is_invocable_v<Fn&>) {
                fn();
            } else {
                static_assert(std::is_invocable_v<Fn&, Event&>,
                              "event callback must accept Event&, MouseEvent&, WheelEvent&, KeyboardEvent&, or no arguments");
            }
        } catch (...) {
            // Exceptions must never unwind through the C callback boundary.
        }
    }

    Fn fn;
};

class CSSStyleDeclaration {
public:
    String getPropertyValue(const String& property) const {
        return detail::readString(
            [this, &property](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_element_get_style_property(raw_, property.data(), property.size(),
                                                      buffer, capacity, size);
            }, "CSSStyleDeclaration::getPropertyValue");
    }

    void setProperty(const String& property, const String& value, bool important = false) {
        detail::check(kui_element_set_style_property(raw_, property.data(), property.size(),
                                                     value.data(), value.size(), important ? 1 : 0),
                      "CSSStyleDeclaration::setProperty");
    }

    void removeProperty(const String& property) {
        detail::check(kui_element_remove_style_property(raw_, property.data(), property.size()),
                      "CSSStyleDeclaration::removeProperty");
    }

    bool isImportant(const String& property) const {
        int value = 0;
        detail::check(kui_element_style_property_is_important(raw_, property.data(),
                                                              property.size(), &value),
                      "CSSStyleDeclaration::isImportant");
        return value != 0;
    }

    String cssText() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_element_get_style_text(raw_, buffer, capacity, size);
            }, "CSSStyleDeclaration::cssText");
    }

    void setCssText(const String& value) {
        detail::check(kui_element_set_style_text(raw_, value.data(), value.size()),
                      "CSSStyleDeclaration::setCssText");
    }

    u32 length() const {
        std::size_t value = 0;
        detail::check(kui_element_get_style_property_count(raw_, &value),
                      "CSSStyleDeclaration::length");
        return static_cast<u32>(value);
    }

    String item(u32 index) const {
        return detail::readString(
            [this, index](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_element_get_style_property_name(raw_, index, buffer, capacity, size);
            }, "CSSStyleDeclaration::item");
    }

private:
    explicit CSSStyleDeclaration(KuiElement* raw) : raw_(raw) {}
    KuiElement* raw_ = nullptr;
    friend class Element;
};

class Element : public std::enable_shared_from_this<Element> {
public:
    ~Element() {
        for (const auto& listener : listeners_)
            kui_element_remove_event_listener(raw_, listener.type.data(), listener.type.size(),
                                               listener.id);
        listeners_.clear();
        if (raw_) kui_element_release(raw_);
    }

    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;

    static RefPtr<Element> create(Document* owner, const String& tagName);
    bool isElement() const noexcept { return true; }
    Element* parentNode() const { return parentElement(); }

    String tagName() const { return read(kui_element_get_tag_name, "Element::tagName"); }
    String id() const { return read(kui_element_get_id, "Element::id"); }
    void setId(const String& value) { setAttribute("id", value); }

    String className() const { return read(kui_element_get_class_name, "Element::className"); }
    void setClassName(const String& value) {
        detail::check(kui_element_set_class_name(raw_, value.data(), value.size()),
                      "Element::setClassName");
    }
    bool hasClass(const String& value) const {
        int result = 0;
        detail::check(kui_element_class_list_contains(raw_, value.data(), value.size(), &result),
                      "Element::hasClass");
        return result != 0;
    }
    void addClass(const String& value) { detail::check(kui_element_class_list_add(raw_, value.data(), value.size()), "Element::addClass"); }
    void removeClass(const String& value) { detail::check(kui_element_class_list_remove(raw_, value.data(), value.size()), "Element::removeClass"); }
    void toggleClass(const String& value) { detail::check(kui_element_class_list_toggle(raw_, value.data(), value.size()), "Element::toggleClass"); }

    bool hasAttribute(const String& name) const {
        int result = 0;
        detail::check(kui_element_has_attribute(raw_, name.data(), name.size(), &result),
                      "Element::hasAttribute");
        return result != 0;
    }
    String getAttribute(const String& name) const {
        return detail::readString(
            [this, &name](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_element_get_attribute(raw_, name.data(), name.size(),
                                                 buffer, capacity, size);
            }, "Element::getAttribute");
    }
    void setAttribute(const String& name, const String& value) {
        detail::check(kui_element_set_attribute(raw_, name.data(), name.size(),
                                                value.data(), value.size()),
                      "Element::setAttribute");
    }
    void removeAttribute(const String& name) {
        detail::check(kui_element_remove_attribute(raw_, name.data(), name.size()),
                      "Element::removeAttribute");
    }

    String textContent() const { return read(kui_element_get_text_content, "Element::textContent"); }
    void setTextContent(const String& value) {
        detail::check(kui_element_set_text_content(raw_, value.data(), value.size()),
                      "Element::setTextContent");
    }
    String innerHTML() const { return read(kui_element_get_inner_html, "Element::innerHTML"); }
    void setInnerHTML(const String& value) {
        detail::check(kui_element_set_inner_html(raw_, value.data(), value.size()),
                      "Element::setInnerHTML");
    }

    CSSStyleDeclaration* style() { return &style_; }
    const CSSStyleDeclaration* style() const { return &style_; }

    Element* querySelector(const String& selector) const;
    std::vector<RefPtr<Element>> querySelectorAll(const String& selector) const;
    Element* parentElement() const { return related(kui_element_parent, "Element::parentElement"); }
    Element* firstElementChild() const { return related(kui_element_first_child, "Element::firstElementChild"); }
    Element* lastElementChild() const { return related(kui_element_last_child, "Element::lastElementChild"); }
    Element* nextElementSibling() const { return related(kui_element_next_sibling, "Element::nextElementSibling"); }
    Element* previousElementSibling() const { return related(kui_element_previous_sibling, "Element::previousElementSibling"); }
    u32 childElementCount() const {
        std::size_t value = 0;
        detail::check(kui_element_child_count(raw_, &value), "Element::childElementCount");
        return static_cast<u32>(value);
    }

    Element* appendChild(const RefPtr<Element>& child) {
        if (!child) return nullptr;
        detail::check(kui_element_append_child(raw_, child->raw_), "Element::appendChild");
        return child.get();
    }
    RefPtr<Element> removeChild(Element* child);
    void remove() { detail::check(kui_element_remove(raw_), "Element::remove"); }

    Rect boundingClientRect() const {
        KuiRect value{};
        detail::check(kui_element_get_bounding_rect(raw_, &value),
                      "Element::boundingClientRect");
        return {value.x, value.y, value.width, value.height};
    }
    f32 clientLeft() const { return clientMetric(0); }
    f32 clientTop() const { return clientMetric(1); }
    f32 clientWidth() const { return clientMetric(2); }
    f32 clientHeight() const { return clientMetric(3); }
    f32 offsetLeft() const { return offsetMetric(0); }
    f32 offsetTop() const { return offsetMetric(1); }
    f32 offsetWidth() const { return offsetMetric(2); }
    f32 offsetHeight() const { return offsetMetric(3); }
    f32 scrollLeft() const { return scrollMetric(0); }
    f32 scrollTop() const { return scrollMetric(1); }
    f32 scrollWidth() const { return scrollMetric(2); }
    f32 scrollHeight() const { return scrollMetric(3); }
    void setScrollLeft(f32 value) { detail::check(kui_element_set_scroll_position(raw_, value, scrollTop()), "Element::setScrollLeft"); }
    void setScrollTop(f32 value) { detail::check(kui_element_set_scroll_position(raw_, scrollLeft(), value), "Element::setScrollTop"); }
    void scrollTo(f32 x, f32 y) { detail::check(kui_element_set_scroll_position(raw_, x, y), "Element::scrollTo"); }
    void scrollBy(f32 x, f32 y) { detail::check(kui_element_scroll_by(raw_, x, y), "Element::scrollBy"); }
    // Scrolls the nearest scrollable ancestors so the element is visible (top- or bottom-aligned).
    void scrollIntoView(bool alignToTop = true) {
        detail::check(kui_element_scroll_into_view(raw_, alignToTop ? 1 : 0), "Element::scrollIntoView");
    }
    // Starts an inertial scroll, velocity in px/ms (positive y scrolls down).
    void startMomentumScroll(f32 velocityX, f32 velocityY) {
        detail::check(kui_element_start_momentum_scroll(raw_, velocityX, velocityY), "Element::startMomentumScroll");
    }
    void focus() { detail::check(kui_element_focus(raw_), "Element::focus"); }
    // Section 17 (3.61.8): repaint on the next frame (a custom-painted element after its state changed).
    void markNeedsPaint() { detail::check(kui_element_request_repaint(raw_), "Element::markNeedsPaint"); }
    // Repaints only this content-box rectangle (wrapper-only; native code uses Renderer::invalidateRect).
    void invalidateRect(const Rect& rect) {
        detail::check(kui_element_invalidate_rect(raw_, rect.x, rect.y, rect.width, rect.height),
                      "Element::invalidateRect");
    }
    // Same shaping as KUI's own text (wrapper-only; native code uses RenderBackend). Needs a window.
    TextMetrics measureText(const String& text, const TextStyle& style = {}) const {
        KuiCTextStyle cStyle{};
        cStyle.font_family = style.fontFamily.c_str();
        cStyle.font_size = style.fontSize;
        cStyle.font_weight = style.fontWeight;
        cStyle.italic = style.italic ? 1 : 0;
        cStyle.letter_spacing = style.letterSpacing;
        cStyle.word_spacing = style.wordSpacing;
        KuiCTextMetrics metrics{};
        detail::check(kui_element_measure_text(raw_, text.data(), text.size(), &cStyle, &metrics),
                      "Element::measureText");
        return {metrics.width, metrics.ascent, metrics.descent, metrics.line_height};
    }
    void blur() { detail::check(kui_element_blur(raw_), "Element::blur"); }
    bool hasFocus() const { int v = 0; detail::check(kui_element_has_focus(raw_, &v), "Element::hasFocus"); return v != 0; }

    // refChild nullptr appends; a child already in a tree is moved.
    Element* insertBefore(const RefPtr<Element>& child, Element* refChild) {
        if (!child) return nullptr;
        detail::check(kui_element_insert_before(raw_, child->raw_, refChild ? refChild->raw_ : nullptr),
                      "Element::insertBefore");
        return child.get();
    }
    Element* replaceChild(const RefPtr<Element>& newChild, Element* oldChild) {
        if (!newChild || !oldChild) return nullptr;
        detail::check(kui_element_replace_child(raw_, newChild->raw_, oldChild->raw_), "Element::replaceChild");
        return newChild.get();
    }
    // Native cloneNode returns RefPtr<Node>; the wrapper only has element handles.
    RefPtr<Element> cloneNode(bool deep = false) const;
    bool matches(const String& selector) const {
        int v = 0;
        detail::check(kui_element_matches(raw_, selector.data(), selector.size(), &v), "Element::matches");
        return v != 0;
    }
    Element* closest(const String& selector) const;
    String outerHTML() const {
        return detail::readString([this](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_element_get_outer_html(raw_, buffer, capacity, size);
        }, "Element::outerHTML");
    }
    // getComputedStyle(el).getPropertyValue(name), same as the native method.
    String computedStylePropertyValue(const String& propertyName) {
        return detail::readString([this, &propertyName](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_element_get_computed_style_value(raw_, propertyName.data(), propertyName.size(),
                                                        buffer, capacity, size);
        }, "Element::computedStylePropertyValue");
    }
    void insertAdjacentHTML(const String& position, const String& html) {
        detail::check(kui_element_insert_adjacent_html(raw_, position.data(), position.size(), html.data(), html.size()),
                      "Element::insertAdjacentHTML");
    }
    // Native code uses document->createTextNode() + appendChild(); the C ABI has no text-node handle.
    void appendText(const String& text) {
        detail::check(kui_element_append_text(raw_, text.data(), text.size()), "Element::appendText");
    }
    void setPointerCapture(i32 pointerId) { detail::check(kui_element_set_pointer_capture(raw_, pointerId), "Element::setPointerCapture"); }
    void releasePointerCapture(i32 pointerId) { detail::check(kui_element_release_pointer_capture(raw_, pointerId), "Element::releasePointerCapture"); }
    bool hasPointerCapture(i32 pointerId) const { int v = 0; detail::check(kui_element_has_pointer_capture(raw_, pointerId, &v), "Element::hasPointerCapture"); return v != 0; }

    struct PositioningOptions {
        bool flip = true;
        bool clampToViewport = true;
        f32 offset = 0;
        f32 viewportMargin = 8;
    };
    void positionRelativeTo(const Element* anchor, const String& placement,
                            const PositioningOptions& options);
    void positionRelativeTo(const Element* anchor, const String& placement) {
        positionRelativeTo(anchor, placement, PositioningOptions{});
    }
    KuiElement* cHandle() const noexcept { return raw_; }
    // The wrapper state of the owning document (element cache); empty once it is gone.
    std::shared_ptr<detail::DocumentState> documentState() const { return document_.lock(); }
    std::shared_ptr<detail::DocumentState> wrapperState() const { return document_.lock(); }

    void mediaPlay();
    void mediaPause();
    void mediaSetSrc(const String& source);
    void mediaSetControls(bool enabled);
    void mediaSetCurrentTime(f64 seconds);
    f64 mediaCurrentTime() const;
    f64 mediaDuration() const;
    void mediaSetVolume(f32 volume);
    f32 mediaVolume() const;
    void mediaSetMuted(bool muted);
    bool mediaMuted() const;
    bool mediaPaused() const;
    i32 mediaReadyState() const;
    i32 mediaNetworkState() const;
    bool mediaAvailable() const;
    void lottieSetSrc(const String& source);
    void lottiePlay();
    void lottiePause();
    bool lottieComplete() const;
    Size lottieNaturalSize() const;
    // Section 13 (3.61.8): media / Lottie playback control. The native-named
    // members below dispatch to whichever the element is, like play().
    void mediaLoad();
    void mediaSetLoop(bool loop);
    void mediaSetAutoplay(bool autoplay);
    bool mediaLoop() const;
    bool mediaAutoplay() const;
    bool mediaEnded() const;
    void lottieStop();
    void lottieSeek(f64 position);
    void lottieSetSpeed(f32 speed);
    void lottieSetLoop(bool loop);
    KuiCLottieState lottieState() const;
    void load() { mediaLoad(); }
    void stop() { lottieStop(); }
    void seek(f64 position) { lottieSeek(position); }
    f32 speed() const { return lottieState().speed; }
    bool isPlaying() const { return lottieState().playing != 0; }
    f64 currentPosition() const { return lottieState().position; }
    bool ended() const { return mediaEnded(); }
    bool loop() const;
    void setLoop(bool loop);
    bool autoplay() const;
    void setAutoplay(bool autoplay);
    // 表单控件的当前值（用户键入 / 选择的）：<input> / <textarea> / <select> / <output>，<progress> / <meter> 为数值文本；
    // 没有「当前值」概念的元素（C ABI 返回 KUI_ERR_NOT_SUPPORTED）退回读内容属性 value，与以前一致。
    // 写入不派发 input / change 事件。
    String value() const {
        std::size_t probe = 0;
        if (kui_element_get_value(raw_, nullptr, 0, &probe) == KUI_ERR_NOT_SUPPORTED)
            return getAttribute("value");
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_element_get_value(raw_, buffer, capacity, size);
            }, "Element::value");
    }
    void setValue(const String& value) {
        const KuiStatus status = kui_element_set_value(raw_, value.data(), value.size());
        if (status == KUI_ERR_NOT_SUPPORTED) {
            setAttribute("value", value);
            return;
        }
        detail::check(status, "Element::setValue");
    }
    // <input> 的选中状态（复选框 / 单选有意义）；非 <input> 抛 KUI_ERR_NOT_SUPPORTED。
    bool checked() const {
        int checked = 0;
        detail::check(kui_element_get_checked(raw_, &checked), "Element::checked");
        return checked != 0;
    }
    void setChecked(bool checked) {
        detail::check(kui_element_set_checked(raw_, checked ? 1 : 0), "Element::setChecked");
    }
    // <select> 当前选中的选项下标（-1 没有）；非 <select> 抛 KUI_ERR_NOT_SUPPORTED。
    i32 selectedIndex() const {
        int32_t index = 0;
        detail::check(kui_element_get_selected_index(raw_, &index), "Element::selectedIndex");
        return index;
    }
    void setSelectedIndex(i32 index) {
        detail::check(kui_element_set_selected_index(raw_, index), "Element::setSelectedIndex");
    }
    void play();
    void pause();
    void setSrc(const String& source);
    void setControls(bool enabled) { mediaSetControls(enabled); }
    void setCurrentTime(f64 seconds) { mediaSetCurrentTime(seconds); }
    f64 currentTime() const { return mediaCurrentTime(); }
    f64 duration() const { return mediaDuration(); }
    void setVolume(f32 volume) { mediaSetVolume(volume); }
    f32 volume() const { return mediaVolume(); }
    void setMuted(bool muted) { mediaSetMuted(muted); }
    bool muted() const { return mediaMuted(); }
    bool paused() const { return mediaPaused(); }
    i32 readyState() const { return mediaReadyState(); }
    i32 networkState() const { return mediaNetworkState(); }
    bool isMediaAvailable() const { return mediaAvailable(); }
    bool complete() const { return lottieComplete(); }
    u32 naturalWidth() const { return static_cast<u32>(lottieNaturalSize().width); }
    u32 naturalHeight() const { return static_cast<u32>(lottieNaturalSize().height); }

    template <typename Fn>
    u32 addEventListener(const String& type, Fn&& fn) {
        using Callback = std::decay_t<Fn>;
        auto holder = std::make_shared<detail_listener_holder<Callback>>(
            document_.lock(), std::forward<Fn>(fn));
        KuiListenerId id = 0;
        detail::check(kui_element_add_event_listener(raw_, type.data(), type.size(),
                                                     &detail_listener_holder_base::trampoline,
                                                     holder.get(), &id),
                      "Element::addEventListener");
        listeners_.push_back({type, id, std::move(holder)});
        return static_cast<u32>(id);
    }
    // capture = true: the listener runs while the event travels down from the root to the target (first
    // at the target itself), before the plain listeners; stopPropagation there ends the descent.
    template <typename Fn>
    u32 addEventListener(const String& type, Fn&& fn, bool capture) {
        using Callback = std::decay_t<Fn>;
        auto holder = std::make_shared<detail_listener_holder<Callback>>(
            document_.lock(), std::forward<Fn>(fn));
        KuiListenerId id = 0;
        detail::check(kui_element_add_event_listener_ex(raw_, type.data(), type.size(),
                                                        &detail_listener_holder_base::trampoline,
                                                        holder.get(), capture ? 1 : 0, &id),
                      "Element::addEventListener");
        listeners_.push_back({type, id, std::move(holder)});
        return static_cast<u32>(id);
    }

    void removeEventListener(const String& type, u32 id) {
        detail::check(kui_element_remove_event_listener(raw_, type.data(), type.size(), id),
                      "Element::removeEventListener");
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
            if (it->type == type && it->id == id) {
                listeners_.erase(it);
                break;
            }
        }
    }

    void removeEventListeners(const String& type) {
        detail::check(kui_element_remove_event_listeners(raw_, type.data(), type.size()),
                      "Element::removeEventListeners");
        for (auto it = listeners_.begin(); it != listeners_.end();) {
            if (it->type == type) it = listeners_.erase(it);
            else ++it;
        }
    }

    bool dispatchEvent(const String& type, bool bubbles = true, bool cancelable = true) {
        int notCanceled = 0;
        detail::check(kui_element_dispatch_event(raw_, type.data(), type.size(),
                                                 bubbles ? 1 : 0, cancelable ? 1 : 0,
                                                 &notCanceled),
                      "Element::dispatchEvent");
        return notCanceled != 0;
    }

private:
    Element(KuiElement* raw, std::shared_ptr<detail::DocumentState> document)
        : raw_(raw), document_(document), style_(raw) {}

    using StringGetter = KuiStatus (*)(KuiElement*, char*, std::size_t, std::size_t*);
    String read(StringGetter getter, const char* context) const {
        return detail::readString(
            [this, getter](char* buffer, std::size_t capacity, std::size_t* size) {
                return getter(raw_, buffer, capacity, size);
            }, context);
    }
    using RelatedGetter = KuiStatus (*)(KuiElement*, KuiElement**);
    Element* related(RelatedGetter getter, const char* context) const;
    f32 clientMetric(int index) const {
        f32 values[4]{};
        detail::check(kui_element_get_client_metrics(raw_, &values[0], &values[1],
                                                     &values[2], &values[3]), "Element::clientMetric");
        return values[index];
    }
    f32 offsetMetric(int index) const {
        f32 values[4]{};
        detail::check(kui_element_get_offset_metrics(raw_, &values[0], &values[1],
                                                     &values[2], &values[3]), "Element::offsetMetric");
        return values[index];
    }
    f32 scrollMetric(int index) const {
        f32 values[4]{};
        detail::check(kui_element_get_scroll_metrics(raw_, &values[0], &values[1],
                                                     &values[2], &values[3]), "Element::scrollMetric");
        return values[index];
    }

    struct ListenerRegistration {
        String type;
        KuiListenerId id;
        std::shared_ptr<detail_listener_holder_base> holder;
    };

    KuiElement* raw_ = nullptr;
    std::weak_ptr<detail::DocumentState> document_;
    CSSStyleDeclaration style_;
    std::vector<ListenerRegistration> listeners_;

    friend struct detail::DocumentState;
};

using Node = Element;

namespace detail {

struct DocumentState : std::enable_shared_from_this<DocumentState> {
    explicit DocumentState(KuiDocument* value) : raw(value) {}
    ~DocumentState() {
        elements.clear();
        if (raw) kui_document_release(raw);
    }

    RefPtr<Element> wrap(KuiElement* value) {
        if (!value) return {};
        const auto found = elements.find(value);
        if (found != elements.end()) {
            kui_element_release(value);
            return found->second;
        }
        auto result = RefPtr<Element>(new Element(value, shared_from_this()));
        elements.emplace(value, result);
        return result;
    }

    Element* borrow(KuiElement* value) {
        if (!value) return nullptr;
        const auto found = elements.find(value);
        if (found != elements.end()) return found->second.get();
        detail::check(kui_element_retain(value), "DocumentState::borrow");
        return wrap(value).get();
    }

    KuiDocument* raw = nullptr;
    Document* owner = nullptr;
    std::unordered_map<KuiElement*, RefPtr<Element>> elements;
};

}  // namespace detail

inline Element* Event::target() const {
    KuiElement* raw = nullptr;
    const KuiStatus status = kui_event_get_target(raw_, &raw);
    if (status == KUI_ERR_NOT_FOUND || !document_) return nullptr;
    detail::check(status, "Event::target");
    return document_->wrap(raw).get();
}

inline Element* Event::currentTarget() const {
    KuiElement* raw = nullptr;
    const KuiStatus status = kui_event_get_current_target(raw_, &raw);
    if (status == KUI_ERR_NOT_FOUND || !document_) return nullptr;
    detail::check(status, "Event::currentTarget");
    return document_->wrap(raw).get();
}

inline Element* MouseEvent::relatedTarget() const {
    KuiElement* raw = nullptr;
    const KuiStatus status = kui_event_get_related_target(raw_, &raw);
    if (status == KUI_ERR_NOT_FOUND || !document_) return nullptr;
    detail::check(status, "MouseEvent::relatedTarget");
    return document_->wrap(raw).get();
}

inline Element* Element::related(RelatedGetter getter, const char* context) const {
    KuiElement* raw = nullptr;
    const KuiStatus status = getter(raw_, &raw);
    if (status == KUI_ERR_NOT_FOUND) return nullptr;
    detail::check(status, context);
    const auto document = document_.lock();
    return document ? document->wrap(raw).get() : nullptr;
}

inline Element* Element::querySelector(const String& selector) const {
    KuiElement* raw = nullptr;
    const KuiStatus status = kui_element_query_selector(raw_, selector.data(), selector.size(), &raw);
    if (status == KUI_ERR_NOT_FOUND) return nullptr;
    detail::check(status, "Element::querySelector");
    const auto document = document_.lock();
    return document ? document->wrap(raw).get() : nullptr;
}

inline std::vector<RefPtr<Element>> Element::querySelectorAll(const String& selector) const {
    std::size_t count = 0;
    detail::check(kui_element_query_selector_all(raw_, selector.data(), selector.size(),
                                                 nullptr, 0, &count),
                  "Element::querySelectorAll(count)");
    std::vector<KuiElement*> handles(count);
    if (count) detail::check(kui_element_query_selector_all(raw_, selector.data(), selector.size(),
                                                            handles.data(), handles.size(), &count),
                             "Element::querySelectorAll");
    std::vector<RefPtr<Element>> result;
    result.reserve(count);
    const auto document = document_.lock();
    for (auto* handle : handles) result.push_back(document->wrap(handle));
    return result;
}

inline RefPtr<Element> Element::cloneNode(bool deep) const {
    KuiElement* raw = nullptr;
    detail::check(kui_element_clone(raw_, deep ? 1 : 0, &raw), "Element::cloneNode");
    const auto document = document_.lock();
    if (!document) {
        kui_element_release(raw);
        return {};
    }
    return document->wrap(raw);
}

inline Element* Element::closest(const String& selector) const {
    KuiElement* raw = nullptr;
    const KuiStatus status = kui_element_closest(raw_, selector.data(), selector.size(), &raw);
    if (status == KUI_ERR_NOT_FOUND) return nullptr;
    detail::check(status, "Element::closest");
    const auto document = document_.lock();
    return document ? document->wrap(raw).get() : nullptr;
}

inline RefPtr<Element> Element::removeChild(Element* child) {
    if (!child) return {};
    detail::check(kui_element_remove_child(raw_, child->raw_), "Element::removeChild");
    const auto document = document_.lock();
    if (!document) return {};
    const auto found = document->elements.find(child->raw_);
    return found == document->elements.end() ? RefPtr<Element>() : found->second;
}

class ToastManager;
class Router;
class BindingManager;
class AnimationManager;
class TransitionManager;
class MutationObserverRegistry;
class ResizeObserverRegistry;
class IntersectionObserverRegistry;
class PopupManager;
class InputPicker;
class AudioManager;
class EventDispatcher;
class TrayIcon;
class ShortcutManager;
class Renderer;
struct MenuBarDef;
namespace detail { struct WindowDesktop; }

class Document : public std::enable_shared_from_this<Document> {
public:
    explicit Document(KuiDocument* raw)
        : state_(std::make_shared<detail::DocumentState>(raw)) {
        state_->owner = this;
    }
    ~Document();

    static RefPtr<Document> create() {
        KuiDocument* raw = nullptr;
        detail::check(kui_document_create(&raw), "Document::create");
        return std::make_shared<Document>(raw);
    }

    // Section 19 (3.61.8): NativeGui keeps KUI's GUI defaults; BrowserCompat uses the browser's
    // document defaults (serif default font). Author styles override either.
    enum class StyleProfile : u8 { NativeGui, BrowserCompat };
    static RefPtr<Document> create(StyleProfile profile) {
        auto document = create();
        document->setStyleProfile(profile);
        return document;
    }
    // The directory relative resource paths (images, stylesheets, scripts) resolve against.
    void setBasePath(const String& path) {
        detail::check(kui_document_set_base_path(state_->raw, path.data(), path.size()), "Document::setBasePath");
    }
    String basePath() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_document_get_base_path(state_->raw, buffer, capacity, size);
            }, "Document::basePath");
    }
    // The render backend's device pixel ratio (native: document->renderBackend()->setDevicePixelRatio).
    // Throws KUI_ERR_NOT_SUPPORTED before the document has a renderer (no window yet).
    void setRenderDevicePixelRatio(f32 ratio) {
        detail::check(kui_document_set_render_device_pixel_ratio(state_->raw, ratio), "Document::setRenderDevicePixelRatio");
    }

    // -- In-process DevTools: inject input and run Chrome DevTools Protocol commands against this
    //    document without a server. Native: EventDispatcher::handle* and DevToolsServer::dispatchCommandForTest.
    // modifiers: bit 0 Ctrl, 1 Shift, 2 Alt, 3 Meta (see kui_c.h). Throws KUI_ERR_NOT_FOUND without a dispatcher.
    void dispatchMouse(KuiCMouseEventType type, f32 x, f32 y, KuiCMouseButton button = KUI_C_MOUSE_BUTTON_NONE,
                       int modifiers = 0, int clickCount = 0, f32 deltaX = 0, f32 deltaY = 0) {
        detail::check(kui_devtools_dispatch_mouse_event(state_->raw, type, x, y, button, modifiers, clickCount,
                                                        deltaX, deltaY), "Document::dispatchMouse");
    }
    // code defaults to key when empty; text is the typed character for KUI_C_KEY_CHAR.
    void dispatchKey(KuiCKeyEventType type, const String& key, const String& code = {}, int modifiers = 0,
                     const String& text = {}, bool autoRepeat = false) {
        detail::check(kui_devtools_dispatch_key_event(state_->raw, type, key.data(), key.size(),
                                                      code.empty() ? nullptr : code.data(), code.size(), modifiers,
                                                      text.data(), text.size(), autoRepeat ? 1 : 0),
                      "Document::dispatchKey");
    }
    void dispatchIme(KuiCImeEventType type, const String& text = {}) {
        detail::check(kui_devtools_dispatch_ime_event(state_->raw, type, text.data(), text.size()),
                      "Document::dispatchIme");
    }
    // Runs one CDP command (a JSON string) and returns the JSON response. A response longer than 64 KiB
    // makes the command run a second time to read it in full, so do not send side-effecting commands
    // whose answers can be that large.
    String dispatchDevToolsCommand(const String& jsonCommand) {
        String buffer(65536, '\0');
        std::size_t size = 0;
        KuiStatus status = kui_devtools_dispatch_command(state_->raw, jsonCommand.c_str(), buffer.data(),
                                                         buffer.size(), &size);
        if (status == KUI_ERR_BUFFER_TOO_SMALL) {
            buffer.assign(size + 1, '\0');
            status = kui_devtools_dispatch_command(state_->raw, jsonCommand.c_str(), buffer.data(), buffer.size(),
                                                   &size);
        }
        detail::check(status, "Document::dispatchDevToolsCommand");
        buffer.resize(size);
        return buffer;
    }
    // The four corners of each CSS box of the first element matching selector, and its content size.
    struct BoxModel {
        std::array<Point, 4> content, padding, border, margin;
        Size size;
    };
    Optional<BoxModel> boxModel(const String& selector) const {
        float quads[32] = {};
        float width = 0, height = 0;
        const KuiStatus status = kui_devtools_get_box_model(state_->raw, selector.c_str(), quads, &width, &height);
        if (status == KUI_ERR_NOT_FOUND) return std::nullopt;
        detail::check(status, "Document::boxModel");
        BoxModel model;
        std::array<Point, 4>* boxes[4] = {&model.content, &model.padding, &model.border, &model.margin};
        for (int box = 0; box < 4; ++box)
            for (int corner = 0; corner < 4; ++corner)
                (*boxes[box])[corner] = {quads[box * 8 + corner * 2], quads[box * 8 + corner * 2 + 1]};
        model.size = {width, height};
        return model;
    }
    StyleProfile styleProfile() const {
        KuiCStyleProfile value = KUI_C_STYLE_NATIVE_GUI;
        detail::check(kui_document_get_style_profile(state_->raw, &value), "Document::styleProfile");
        return value == KUI_C_STYLE_BROWSER_COMPAT ? StyleProfile::BrowserCompat : StyleProfile::NativeGui;
    }
    void setStyleProfile(StyleProfile profile) {
        detail::check(kui_document_set_style_profile(state_->raw, profile == StyleProfile::BrowserCompat
                                                                       ? KUI_C_STYLE_BROWSER_COMPAT
                                                                       : KUI_C_STYLE_NATIVE_GUI),
                      "Document::setStyleProfile");
    }

    void loadHTML(const String& html) { detail::check(kui_document_load_html(state_->raw, html.data(), html.size()), "Document::loadHTML"); }
    void loadFile(const String& path) { detail::check(kui_document_load_html_file(state_->raw, path.data(), path.size()), "Document::loadFile"); }
    void addStylesheet(const String& css) { detail::check(kui_document_add_stylesheet(state_->raw, css.data(), css.size()), "Document::addStylesheet"); }
    void clearStylesheets() { detail::check(kui_document_clear_stylesheets(state_->raw), "Document::clearStylesheets"); }

    Element* documentElement() const { return get(kui_document_root, "Document::documentElement"); }
    Element* head() const { return get(kui_document_head, "Document::head"); }
    Element* body() const { return get(kui_document_body, "Document::body"); }
    Element* getElementById(const String& id) const {
        KuiElement* raw = nullptr;
        const KuiStatus status = kui_document_get_element_by_id(state_->raw, id.data(), id.size(), &raw);
        if (status == KUI_ERR_NOT_FOUND) return nullptr;
        detail::check(status, "Document::getElementById");
        return state_->wrap(raw).get();
    }
    Element* querySelector(const String& selector) const {
        KuiElement* raw = nullptr;
        const KuiStatus status = kui_document_query_selector(state_->raw, selector.data(), selector.size(), &raw);
        if (status == KUI_ERR_NOT_FOUND) return nullptr;
        detail::check(status, "Document::querySelector");
        return state_->wrap(raw).get();
    }
    std::vector<RefPtr<Element>> querySelectorAll(const String& selector) const {
        std::size_t count = 0;
        detail::check(kui_document_query_selector_all(state_->raw, selector.data(), selector.size(),
                                                      nullptr, 0, &count),
                      "Document::querySelectorAll(count)");
        std::vector<KuiElement*> handles(count);
        if (count) detail::check(kui_document_query_selector_all(state_->raw, selector.data(), selector.size(),
                                                                 handles.data(), handles.size(), &count),
                                 "Document::querySelectorAll");
        std::vector<RefPtr<Element>> result;
        result.reserve(count);
        for (auto* handle : handles) result.push_back(state_->wrap(handle));
        return result;
    }
    RefPtr<Element> createElement(const String& tagName) {
        KuiElement* raw = nullptr;
        detail::check(kui_document_create_element(state_->raw, tagName.data(), tagName.size(), &raw),
                      "Document::createElement");
        return state_->wrap(raw);
    }

    // Native returns RefPtr<NodeList>; the wrapper returns the elements like querySelectorAll.
    std::vector<RefPtr<Element>> getElementsByClassName(const String& className) const {
        return collect(kui_document_get_elements_by_class_name, className, "Document::getElementsByClassName");
    }
    std::vector<RefPtr<Element>> getElementsByTagName(const String& tagName) const {
        return collect(kui_document_get_elements_by_tag_name, tagName, "Document::getElementsByTagName");
    }

    Element* elementFromPoint(f32 x, f32 y) {
        KuiElement* raw = nullptr;
        const KuiStatus status = kui_document_element_from_point(state_->raw, x, y, &raw);
        if (status == KUI_ERR_NOT_FOUND) return nullptr;
        detail::check(status, "Document::elementFromPoint");
        return state_->wrap(raw).get();
    }

    void initExtensions(const String& directory = "extensions");
    ExtensionManager* extensionManager() const;
    CustomElementRegistry* customElements() const;
    bool printToPDF(const String& path, const struct PDFOptions& options);
    KuiDocument* cHandle() const noexcept { return state_->raw; }
    std::shared_ptr<detail::DocumentState> wrapperState() const { return state_; }

private:
    using Collector = KuiStatus (*)(KuiDocument*, const char*, std::size_t, KuiElement**, std::size_t, std::size_t*);
    std::vector<RefPtr<Element>> collect(Collector call, const String& key, const char* context) const {
        std::size_t count = 0;
        detail::check(call(state_->raw, key.data(), key.size(), nullptr, 0, &count), context);
        std::vector<KuiElement*> handles(count);
        if (count) detail::check(call(state_->raw, key.data(), key.size(), handles.data(), handles.size(), &count), context);
        std::vector<RefPtr<Element>> result;
        result.reserve(count);
        for (std::size_t index = 0; index < count; ++index) result.push_back(state_->wrap(handles[index]));
        return result;
    }
    using Getter = KuiStatus (*)(KuiDocument*, KuiElement**);
    Element* get(Getter getter, const char* context) const {
        KuiElement* raw = nullptr;
        const KuiStatus status = getter(state_->raw, &raw);
        if (status == KUI_ERR_NOT_FOUND) return nullptr;
        detail::check(status, context);
        return state_->wrap(raw).get();
    }

    std::shared_ptr<detail::DocumentState> state_;
    mutable Unique<ExtensionManager> extensionManager_;
    mutable Unique<CustomElementRegistry> customElementRegistry_;
    std::shared_ptr<ToastManager> toasts_;  // shared_ptr: deleter fixed where ToastManager is complete
    std::shared_ptr<Router> router_;          // these four: see framework.h
    std::shared_ptr<BindingManager> bindings_;
    std::shared_ptr<AnimationManager> animations_;
    std::shared_ptr<TransitionManager> transitions_;
    std::shared_ptr<PopupManager> popups_;    // these three: see widgets.h
    std::shared_ptr<InputPicker> picker_;
    std::shared_ptr<AudioManager> audio_;
    std::shared_ptr<EventDispatcher> events_;  // widgets.h

public:
    // Defined in desktop.h.
    ToastManager& toastManager();
    // Defined in framework.h.
    Router& router();
    BindingManager& bindingManager();
    AnimationManager* animationManager();
    TransitionManager* transitionManager();
    MutationObserverRegistry& mutationObserverRegistry();
    ResizeObserverRegistry& resizeObserverRegistry();
    IntersectionObserverRegistry& intersectionObserverRegistry();
    // Defined in widgets.h.
    PopupManager& popupManager();
    InputPicker& inputPicker();
    AudioManager& audioManager();
    EventDispatcher* eventDispatcher();
};

class Window {
public:
    static Unique<Window> create(const WindowConfig& config = {}) {
        KuiWindowProps props{};
        props.title = config.title.c_str();
        props.title_len = config.title.size();
        props.width = static_cast<int>(config.width);
        props.height = static_cast<int>(config.height);
        props.min_width = static_cast<int>(config.minWidth);
        props.min_height = static_cast<int>(config.minHeight);
        props.max_width = static_cast<int>(config.maxWidth);
        props.max_height = static_cast<int>(config.maxHeight);
        props.no_resize = config.resizable ? 0 : 1;
        props.no_decorations = config.decorated ? 0 : 1;
        props.transparent = config.transparent ? 1 : 0;
        props.always_on_top = config.alwaysOnTop ? 1 : 0;
        props.maximized = config.maximized ? 1 : 0;
        props.centered = config.centered ? 1 : 0;
        props.frameless = config.frameless ? 1 : 0;
        props.frameless_shadow = config.shadow ? 1 : 0;
        props.opacity = config.opacity;
        KuiWindowPropsEx extended{};
        extended.struct_size = sizeof(extended);
        extended.base = props;
        extended.sizeable = config.sizeable ? 1 : 0;
        extended.hidden = config.visible ? 0 : 1;
        if (config.frameRadius) {
            extended.has_frame_radius = 1;
            extended.frame_radius = *config.frameRadius;
        }
        KuiWindow* raw = nullptr;
        detail::check(kui_window_create_ex(&extended, &raw), "Window::create");
        return Unique<Window>(new Window(raw));
    }

    Window(Window&& other) noexcept
        : raw_(std::exchange(other.raw_, nullptr)), document_(std::move(other.document_)),
          callbacks_(std::move(other.callbacks_)), devToolsServer_(other.devToolsServer_),
          windowListeners_(std::move(other.windowListeners_)) {}
    Window& operator=(Window&& other) noexcept {
        if (this != &other) {
            reset();
            raw_ = std::exchange(other.raw_, nullptr);
            document_ = std::move(other.document_);
            callbacks_ = std::move(other.callbacks_);
            devToolsServer_ = other.devToolsServer_;
            windowListeners_ = std::move(other.windowListeners_);
        }
        return *this;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    Window* operator->() noexcept { return this; }
    const Window* operator->() const noexcept { return this; }
    Window* get() noexcept { return this; }
    const Window* get() const noexcept { return this; }
    explicit operator bool() const noexcept { return raw_ != nullptr; }

    Document* document() const noexcept { return document_.get(); }
    // 与原生相同：窗口的事件派发器就是文档那一个（定义在 widgets.h）。
    EventDispatcher* eventDispatcher() const { return document_ ? document_->eventDispatcher() : nullptr; }
    String title() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_window_get_title(raw_, buffer, capacity, size);
            }, "Window::title");
    }
    void setTitle(const String& value) { detail::check(kui_window_set_title(raw_, value.data(), value.size()), "Window::setTitle"); }
    u32 width() const { int w = 0, h = 0; detail::check(kui_window_get_size(raw_, &w, &h), "Window::width"); return static_cast<u32>(w); }
    u32 height() const { int w = 0, h = 0; detail::check(kui_window_get_size(raw_, &w, &h), "Window::height"); return static_cast<u32>(h); }
    void resize(u32 width, u32 height) { detail::check(kui_window_resize(raw_, static_cast<int>(width), static_cast<int>(height)), "Window::resize"); }
    Point position() const { int x = 0, y = 0; detail::check(kui_window_get_position(raw_, &x, &y), "Window::position"); return {static_cast<f32>(x), static_cast<f32>(y)}; }
    void setPosition(i32 x, i32 y) { detail::check(kui_window_set_position(raw_, x, y), "Window::setPosition"); }
    void setPosition(Point value) { setPosition(static_cast<i32>(value.x), static_cast<i32>(value.y)); }
    void show() { detail::check(kui_window_show(raw_), "Window::show"); }
    void hide() { detail::check(kui_window_hide(raw_), "Window::hide"); }
    bool isVisible() const { int v = 0; detail::check(kui_window_is_visible(raw_, &v), "Window::isVisible"); return v != 0; }
    void minimize() { detail::check(kui_window_minimize(raw_), "Window::minimize"); }
    void maximize() { detail::check(kui_window_maximize(raw_), "Window::maximize"); }
    void restore() { detail::check(kui_window_restore(raw_), "Window::restore"); }
    bool isMinimized() const { int v = 0; detail::check(kui_window_is_minimized(raw_, &v), "Window::isMinimized"); return v != 0; }
    bool isMaximized() const { int v = 0; detail::check(kui_window_is_maximized(raw_, &v), "Window::isMaximized"); return v != 0; }
    void close() { detail::check(kui_window_close(raw_), "Window::close"); }
    bool shouldClose() const { int v = 0; detail::check(kui_window_should_close(raw_, &v), "Window::shouldClose"); return v != 0; }
    void setAlwaysOnTop(bool value) { detail::check(kui_window_set_always_on_top(raw_, value ? 1 : 0), "Window::setAlwaysOnTop"); }
    void setOpacity(f32 value) { detail::check(kui_window_set_opacity(raw_, value), "Window::setOpacity"); }
    f32 devicePixelRatio() const { f32 value = 0; detail::check(kui_window_get_device_pixel_ratio(raw_, &value), "Window::devicePixelRatio"); return value; }
    void setFramelessSizeable(bool enabled) { detail::check(kui_window_set_frameless_sizeable(raw_, enabled ? 1 : 0), "Window::setFramelessSizeable"); }
    void setFrameRadius(f32 radius) { detail::check(kui_window_set_frame_radius(raw_, radius), "Window::setFrameRadius"); }
    void clearFrameRadius() { detail::check(kui_window_clear_frame_radius(raw_), "Window::clearFrameRadius"); }
    void* nativeHandle() const { void* value = nullptr; detail::check(kui_window_get_native_handle(raw_, &value), "Window::nativeHandle"); return value; }
    // Enables / disables mouse and keyboard input to the window (a modal parent); Windows only
    // (other platforms throw KUI_ERR_NOT_SUPPORTED).
    void setEnabled(bool enabled) { detail::check(kui_window_set_enabled(raw_, enabled ? 1 : 0), "Window::setEnabled"); }
    bool isEnabled() const { int v = 0; detail::check(kui_window_is_enabled(raw_, &v), "Window::isEnabled"); return v != 0; }
    // Handles pending events, updates the document and paints one frame now.
    void processFrame() { detail::check(kui_window_process_frame(raw_), "Window::processFrame"); }
    void focus() { detail::check(kui_window_focus(raw_), "Window::focus"); }
    bool hasFocus() const { int v = 0; detail::check(kui_window_has_focus(raw_, &v), "Window::hasFocus"); return v != 0; }
    // cursorType takes the native CursorType values (KuiCCursor).
    void setCursor(u8 cursorType) { detail::check(kui_window_set_cursor(raw_, static_cast<KuiCCursor>(cursorType)), "Window::setCursor"); }
    void setCursorVisible(bool visible) { detail::check(kui_window_set_cursor_visible(raw_, visible ? 1 : 0), "Window::setCursorVisible"); }
    void setFullscreen(bool fullscreen) { detail::check(kui_window_set_fullscreen(raw_, fullscreen ? 1 : 0), "Window::setFullscreen"); }
    bool isFullscreen() const { int v = 0; detail::check(kui_window_is_fullscreen(raw_, &v), "Window::isFullscreen"); return v != 0; }
    void setFrameless(bool enabled) { detail::check(kui_window_set_frameless(raw_, enabled ? 1 : 0), "Window::setFrameless"); }
    void setFramelessShadow(bool enabled) { detail::check(kui_window_set_frameless_shadow(raw_, enabled ? 1 : 0), "Window::setFramelessShadow"); }
    bool isFrameless() const { int v = 0; detail::check(kui_window_get_frame_state(raw_, &v, nullptr, nullptr), "Window::isFrameless"); return v != 0; }
    bool hasFramelessShadow() const { int v = 0; detail::check(kui_window_get_frame_state(raw_, nullptr, &v, nullptr), "Window::hasFramelessShadow"); return v != 0; }
    bool isFramelessSizeable() const { int v = 0; detail::check(kui_window_get_frame_state(raw_, nullptr, nullptr, &v), "Window::isFramelessSizeable"); return v != 0; }
    void setFrameRadii(const WindowFrameRadii& radii) {
        detail::check(kui_window_set_frame_radii(raw_, radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft), "Window::setFrameRadii");
    }
    Optional<WindowFrameRadii> frameRadii() const {
        WindowFrameRadii radii;
        const KuiStatus status = kui_window_get_frame_radii(raw_, &radii.topLeft, &radii.topRight, &radii.bottomRight, &radii.bottomLeft);
        if (status == KUI_ERR_NOT_FOUND) return std::nullopt;
        detail::check(status, "Window::frameRadii");
        return radii;
    }
    Optional<f32> frameRadius() const {
        const auto radii = frameRadii();
        if (!radii || !radii->isUniform()) return std::nullopt;
        return radii->topLeft;
    }
    void setImeCandidatePos(f32 x, f32 y) { detail::check(kui_window_set_ime_candidate_pos(raw_, x, y), "Window::setImeCandidatePos"); }
    DevToolsServer* startDevTools(u16 port = 0) {
        int started = 0;
        detail::check(kui_window_start_devtools(raw_, port, &started), "Window::startDevTools");
        if (!started) return nullptr;
        detail::check(kui_window_get_devtools_port(raw_, &devToolsServer_.port_),
                      "Window::startDevTools(port)");
        devToolsServer_.window_ = raw_;
        devToolsServer_.started_ = true;
        return &devToolsServer_;
    }
    // The server started by startDevTools() (it may have been stopped since), or nullptr.
    DevToolsServer* devToolsServer() { return devToolsServer_.started_ ? &devToolsServer_ : nullptr; }
    String devToolsUrl() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_window_get_devtools_url(raw_, buffer, capacity, size);
            }, "Window::devToolsUrl");
    }
    void setScriptEngine(ScriptEngine* engine);

    template <typename Fn>
    u32 addEventListener(const String& type, Fn&& fn, bool once = false) {
        using Callback = std::decay_t<Fn>;
        auto holder = std::make_shared<detail_listener_holder<Callback>>(
            std::shared_ptr<detail::DocumentState>(), std::forward<Fn>(fn));
        KuiListenerId id = 0;
        detail::check(kui_window_add_event_listener(
                          raw_, type.data(), type.size(),
                          &detail_listener_holder_base::trampoline,
                          holder.get(), once ? 1 : 0, &id),
                      "Window::addEventListener");
        windowListeners_.push_back({type, id, std::move(holder)});
        return static_cast<u32>(id);
    }
    void removeEventListener(const String& type, u32 id) {
        detail::check(kui_window_remove_event_listener(raw_, type.data(), type.size(), id),
                      "Window::removeEventListener");
        for (auto it = windowListeners_.begin(); it != windowListeners_.end(); ++it) {
            if (it->type == type && it->id == id) {
                windowListeners_.erase(it);
                break;
            }
        }
    }
    void removeEventListeners(const String& type) {
        detail::check(kui_window_remove_event_listeners(raw_, type.data(), type.size()),
                      "Window::removeEventListeners");
        for (auto it = windowListeners_.begin(); it != windowListeners_.end();) {
            if (it->type == type) it = windowListeners_.erase(it);
            else ++it;
        }
    }
    KuiWindow* cHandle() const noexcept { return raw_; }

    // Desktop integration (defined in desktop.h).
    void setMenuBar(const MenuBarDef& def);
    void clearMenuBar();
    // Change an item of the menu set with setMenuBar() without rebuilding it; id is MenuItemDef::id.
    void setMenuItemChecked(u32 id, bool checked);
    void setMenuItemEnabled(u32 id, bool enabled);
    TrayIcon& trayIcon();
    ShortcutManager& shortcutManager();
    Renderer* renderer();

    template <typename Fn>
    void onResize(Fn&& fn) {
        callbacks_->resize = std::forward<Fn>(fn);
        detail::check(kui_window_on_resize(raw_, &Window::resizeTrampoline, callbacks_.get()),
                      "Window::onResize");
    }

private:
    struct Callbacks { std::function<void(u32, u32)> resize; };
    struct WindowListenerRegistration {
        String type;
        KuiListenerId id;
        std::shared_ptr<detail_listener_holder_base> holder;
    };

    explicit Window(KuiWindow* raw) : raw_(raw), callbacks_(std::make_shared<Callbacks>()) {
        KuiDocument* document = nullptr;
        detail::check(kui_window_get_document(raw_, &document), "Window::document");
        document_ = std::make_shared<Document>(document);
    }
    static void resizeTrampoline(uint32_t width, uint32_t height, void* userData) noexcept {
        try {
            auto* callbacks = static_cast<Callbacks*>(userData);
            if (callbacks->resize) callbacks->resize(width, height);
        } catch (...) {}
    }
    void reset() noexcept {
        if (raw_) {
            for (const auto& listener : windowListeners_) {
                kui_window_remove_event_listener(raw_, listener.type.data(),
                                                 listener.type.size(), listener.id);
            }
            windowListeners_.clear();
            kui_window_on_resize(raw_, nullptr, nullptr);
            kui_window_release(raw_);
            raw_ = nullptr;
        }
        document_.reset();
        callbacks_.reset();
        desktop_.reset();  // the window (and its C-side menu / tray state) is gone first
    }

    KuiWindow* raw_ = nullptr;
    // Shared so that native-style RefPtr<Document>(window->document()) shares it.
    RefPtr<Document> document_;
    std::shared_ptr<Callbacks> callbacks_;
    DevToolsServer devToolsServer_;
    std::vector<WindowListenerRegistration> windowListeners_;
    // Menu / tray / shortcut callbacks; released after the window (see reset()).
    std::shared_ptr<detail::WindowDesktop> desktop_;
    detail::WindowDesktop& desktop();
};

class Platform {
public:
    struct MonitorInfo {
        String name;
        Rect bounds;
        Rect workArea;
        f32 scale = 1;
        bool isPrimary = false;
    };

    enum class MsgBoxType : u8 { Info, Warning, Error, Question };
    enum class MsgBoxButtons : u8 { OK, OKCancel, YesNo, YesNoCancel };
    enum class MsgBoxResult : u8 { OK, Cancel, Yes, No };

    static bool init() { return kui_platform_init() == KUI_OK; }
    static MsgBoxResult messageBox(const String& title, const String& message,
                                   MsgBoxType type = MsgBoxType::Info,
                                   MsgBoxButtons buttons = MsgBoxButtons::OK) {
        KuiCMessageBoxResult result = KUI_C_MESSAGE_BOX_RESULT_OK;
        detail::check(kui_platform_message_box(title.data(), title.size(), message.data(), message.size(),
                                               static_cast<KuiCMessageBoxType>(type),
                                               static_cast<KuiCMessageBoxButtons>(buttons), &result),
                      "Platform::messageBox");
        return static_cast<MsgBoxResult>(result);
    }
    static void openUrl(const String& url) { detail::check(kui_platform_open_url(url.data(), url.size()), "Platform::openUrl"); }
    static f64 now() { double value = 0; detail::check(kui_platform_now(&value), "Platform::now"); return value; }
    static MonitorInfo monitorFromPoint(f32 x, f32 y) {
        KuiCMonitorInfo value{};
        auto name = detail::readString([x, y, &value](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_platform_get_monitor_from_point(x, y, &value, buffer, capacity, size);
        }, "Platform::monitorFromPoint");
        return {std::move(name),
                {value.bounds_x, value.bounds_y, value.bounds_width, value.bounds_height},
                {value.work_x, value.work_y, value.work_width, value.work_height},
                value.scale, value.is_primary != 0};
    }
    static MonitorInfo monitorFromWindow(void* nativeHandle) {
        KuiCMonitorInfo value{};
        auto name = detail::readString([nativeHandle, &value](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_platform_get_monitor_from_window(nativeHandle, &value, buffer, capacity, size);
        }, "Platform::monitorFromWindow");
        return {std::move(name),
                {value.bounds_x, value.bounds_y, value.bounds_width, value.bounds_height},
                {value.work_x, value.work_y, value.work_width, value.work_height},
                value.scale, value.is_primary != 0};
    }
    // Turns the system input method off for the calling thread (Windows only; others throw
    // KUI_ERR_NOT_SUPPORTED).
    static void disableIme() { detail::check(kui_platform_disable_ime(), "Platform::disableIme"); }
    static void shutdown() {
        timers().clear();
        idleTasks().clear();
        detail::check(kui_platform_shutdown(), "Platform::shutdown");
    }
    static int run() { int code = 0; detail::check(kui_platform_run(&code), "Platform::run"); return code; }
    static void quit(int code = 0) { detail::check(kui_platform_quit(code), "Platform::quit"); }
    static void pollEvents() { detail::check(kui_platform_poll_events(), "Platform::pollEvents"); }
    static void waitEvents(f64 timeout = -1) { detail::check(kui_platform_wait_events(timeout), "Platform::waitEvents"); }
    static void setF12DevToolsEnabled(bool enabled) { detail::check(kui_platform_set_f12_devtools_enabled(enabled ? 1 : 0), "Platform::setF12DevToolsEnabled"); }
    static MonitorInfo primaryMonitor() {
        KuiCMonitorInfo value{};
        detail::check(kui_platform_get_primary_monitor(&value), "Platform::primaryMonitor");
        return {{},
                {value.bounds_x, value.bounds_y, value.bounds_width, value.bounds_height},
                {value.work_x, value.work_y, value.work_width, value.work_height},
                value.scale, value.is_primary != 0};
    }
    static std::vector<MonitorInfo> monitors() {
        std::size_t count = 0;
        detail::check(kui_platform_get_monitor_count(&count), "Platform::monitors(count)");
        std::vector<MonitorInfo> result;
        result.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            KuiCMonitorInfo value{};
            auto name = detail::readString(
                [index, &value](char* buffer, std::size_t capacity, std::size_t* size) {
                    return kui_platform_get_monitor(index, &value, buffer, capacity, size);
                }, "Platform::monitors");
            result.push_back({std::move(name),
                              {value.bounds_x, value.bounds_y, value.bounds_width, value.bounds_height},
                              {value.work_x, value.work_y, value.work_width, value.work_height},
                              value.scale, value.is_primary != 0});
        }
        return result;
    }
    static Size primaryMonitorPhysicalSize() {
        Size result{};
        detail::check(kui_platform_get_primary_monitor_physical_size(&result.width, &result.height),
                      "Platform::primaryMonitorPhysicalSize");
        return result;
    }
    static String clipboardText() {
        return detail::readString(
            [](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_platform_get_clipboard_text(buffer, capacity, size);
            }, "Platform::clipboardText");
    }
    static void setClipboardText(const String& text) {
        detail::check(kui_platform_set_clipboard_text(text.data(), text.size()),
                      "Platform::setClipboardText");
    }
    static bool hasClipboardText() {
        int value = 0;
        detail::check(kui_platform_has_clipboard_text(&value), "Platform::hasClipboardText");
        return value != 0;
    }
    static String osName() {
        return detail::readString(
            [](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_platform_get_os_name(buffer, capacity, size);
            }, "Platform::osName");
    }
    static String osVersion() {
        return detail::readString(
            [](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_platform_get_os_version(buffer, capacity, size);
            }, "Platform::osVersion");
    }
    struct FileDialogOptions {
        String title;
        String defaultPath;
        std::vector<String> filters;
        bool allowMultiple = false;
        FileDialogOptions(String titleIn = {}, String defaultPathIn = {},
                          std::vector<String> filtersIn = {}, bool allowMultipleIn = false)
            : title(std::move(titleIn)), defaultPath(std::move(defaultPathIn)),
              filters(std::move(filtersIn)), allowMultiple(allowMultipleIn) {}
    };
    static Optional<String> openFileDialog(const FileDialogOptions& options = {}) {
        return fileDialog(options, kui_platform_open_file_dialog);
    }
    static Optional<String> saveFileDialog(const FileDialogOptions& options = {}) {
        return fileDialog(options, kui_platform_save_file_dialog);
    }
    static Optional<String> selectFolderDialog(const String& title = {}) {
        std::vector<char> buffer(32768);
        std::size_t size = 0;
        const auto status = kui_platform_select_folder_dialog(
            title.data(), title.size(), buffer.data(), buffer.size(), &size);
        if (status == KUI_ERR_NOT_FOUND) return std::nullopt;
        detail::check(status, "Platform::selectFolderDialog");
        return String(buffer.data(), size);
    }

    template <typename Fn>
    static u32 setTimeout(Fn&& fn, u32 delayMs) {
        return addTimer(std::forward<Fn>(fn), delayMs, true);
    }
    template <typename Fn>
    static u32 setInterval(Fn&& fn, u32 intervalMs) {
        return addTimer(std::forward<Fn>(fn), intervalMs, false);
    }
    static void clearTimer(u32 id) {
        detail::check(kui_platform_clear_timer(id), "Platform::clearTimer");
        timers().erase(id);
    }

    // Thread-safe: hands fn to the main thread, where it runs asynchronously inside
    // the event loop (posting order is kept per posting thread). Like the native SDK
    // it never throws; posts made before init() or once shutdown() has started are
    // dropped (the native SDK queues pre-init posts until init()), and posts still
    // queued at shutdown() are destroyed without running.
    template <typename Fn>
    static void postToMainThread(Fn&& fn) {
        auto task = std::make_unique<std::function<void()>>(std::forward<Fn>(fn));
        if (kui_platform_post_to_main_thread(&Platform::postedTrampoline,
                                             &Platform::postedDiscard, task.get()) == KUI_OK)
            task.release();
    }

    // Runs fn once when the event loop next goes idle (main thread only). Idle callbacks that never ran
    // are dropped at shutdown().
    template <typename Fn>
    static void requestIdle(Fn&& fn) {
        const std::uintptr_t id = ++nextIdleId();
        idleTasks().emplace(id, std::function<void()>(std::forward<Fn>(fn)));
        const KuiStatus status = kui_platform_request_idle(&Platform::idleTrampoline, reinterpret_cast<void*>(id));
        if (status != KUI_OK) {
            idleTasks().erase(id);
            detail::check(status, "Platform::requestIdle");
        }
    }

    // The version of the loaded library at run time (may differ from the headers you compiled against).
    static KuiVersion runtimeVersion() noexcept { return kui_get_version(); }

private:
    using FileDialogFn = KuiStatus (*)(const KuiCFileDialogOptions*, char*, size_t, size_t*);
    static Optional<String> fileDialog(const FileDialogOptions& options, FileDialogFn fn) {
        std::vector<const char*> filters;
        filters.reserve(options.filters.size());
        for (const auto& filter : options.filters) filters.push_back(filter.c_str());
        KuiCFileDialogOptions raw{options.title.c_str(), options.title.size(),
                                  options.defaultPath.c_str(), options.defaultPath.size(),
                                  filters.data(), filters.size(), options.allowMultiple ? 1 : 0};
        std::vector<char> buffer(32768);
        std::size_t size = 0;
        const auto status = fn(&raw, buffer.data(), buffer.size(), &size);
        if (status == KUI_ERR_NOT_FOUND) return std::nullopt;
        detail::check(status, "Platform::fileDialog");
        return String(buffer.data(), size);
    }
    struct Timer {
        std::function<void()> callback;
        KuiTimerId id = 0;
        bool once = false;
    };
    static std::unordered_map<KuiTimerId, std::shared_ptr<Timer>>& timers() {
        static std::unordered_map<KuiTimerId, std::shared_ptr<Timer>> value;
        return value;
    }
    static void postedTrampoline(void* userData) noexcept {
        std::unique_ptr<std::function<void()>> task(static_cast<std::function<void()>*>(userData));
        try { (*task)(); } catch (...) {}
    }
    static void postedDiscard(void* userData) noexcept {
        delete static_cast<std::function<void()>*>(userData);
    }
    static std::unordered_map<std::uintptr_t, std::function<void()>>& idleTasks() {
        static std::unordered_map<std::uintptr_t, std::function<void()>> value;
        return value;
    }
    static std::uintptr_t& nextIdleId() {
        static std::uintptr_t value = 0;
        return value;
    }
    static void idleTrampoline(void* userData) noexcept {
        const auto found = idleTasks().find(reinterpret_cast<std::uintptr_t>(userData));
        if (found == idleTasks().end()) return;
        std::function<void()> task = std::move(found->second);
        idleTasks().erase(found);
        try { task(); } catch (...) {}
    }
    static void timerTrampoline(void* userData) noexcept {
        auto* raw = static_cast<Timer*>(userData);
        const auto found = timers().find(raw->id);
        if (found == timers().end()) return;
        const auto keepAlive = found->second;
        try { keepAlive->callback(); } catch (...) {}
        if (keepAlive->once) timers().erase(keepAlive->id);
    }
    template <typename Fn>
    static u32 addTimer(Fn&& fn, u32 delay, bool once) {
        auto timer = std::make_shared<Timer>();
        timer->callback = std::forward<Fn>(fn);
        timer->once = once;
        KuiTimerId id = 0;
        const KuiStatus status = once
            ? kui_platform_set_timeout(&Platform::timerTrampoline, timer.get(), delay, &id)
            : kui_platform_set_interval(&Platform::timerTrampoline, timer.get(), delay, &id);
        detail::check(status, once ? "Platform::setTimeout" : "Platform::setInterval");
        timer->id = id;
        timers().emplace(id, std::move(timer));
        return static_cast<u32>(id);
    }
};

}  // namespace kui
KUI_CPP_HIDDEN_END

#include <kui/full_profile.h>

#undef KUI_CPP_STRINGIFY
#undef KUI_CPP_STRINGIFY_IMPL
