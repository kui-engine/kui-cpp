// =============================================================================
// kui_cpp.h — a C++ face for the KUI C ABI
//
// WHY THIS EXISTS, and why it ships as SOURCE
// -------------------------------------------
// KUI's C++ SDK passes STL types across the DLL boundary, so a consumer must
// match the prebuilt binaries' CRT flavour or crash on the first std::string
// that crosses. This wrapper removes that constraint: it is compiled BY YOU,
// with YOUR compiler and YOUR CRT, and it talks to the DLL over the pure C ABI
// in <kui_c.h>. The only binary boundary in the chain carries no C++ types.
//
// That is why there is no prebuilt kui_cpp.lib and never will be. A prebuilt
// wrapper would reintroduce exactly the boundary it exists to remove -- the same
// reason CEF ships libcef_dll_wrapper as source only.
//
// DO NOT mix this header with the native C++ SDK headers (<kui/dom/document.h>
// and friends) in one program. Both define kui::Document; the names are
// deliberately the same so migrating is mostly a matter of changing includes and
// link targets, which also means they cannot coexist.
//
// ERROR HANDLING
// --------------
// Absence is a return value, not an error: a lookup that finds nothing yields an
// empty handle (test it with `if (el)`), matching the native API's nullptr. Real
// failures -- out of memory, an internal fault, a mis-sized buffer -- throw
// kui::Error, which carries the KuiStatus.
// =============================================================================
#pragma once

#include <kui_c.h>

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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

// -----------------------------------------------------------------------------
// Error
// -----------------------------------------------------------------------------
class Error : public std::runtime_error {
public:
    explicit Error(KuiStatus status, const char* what)
        : std::runtime_error(what), status_(status) {}

    KuiStatus status() const noexcept { return status_; }

private:
    KuiStatus status_;
};

namespace detail {

// Throws on a genuine failure. KUI_ERR_NOT_FOUND is NOT a failure here -- the
// callers that can encounter it handle it by returning an empty handle.
inline void check(KuiStatus s, const char* context) {
    if (s == KUI_OK) return;
    std::string msg = context;
    msg += ": ";
    msg += kui_status_message(s);
    throw Error(s, msg.c_str());
}

// Wraps the two-call output-string protocol. Nothing is allocated by the DLL, so
// the std::string below belongs entirely to the caller's CRT.
template <typename Fn>
inline std::string readString(Fn&& call, const char* context) {
    std::size_t need = 0;
    KuiStatus s = call(nullptr, std::size_t{0}, &need);
    if (s == KUI_ERR_NOT_FOUND) return std::string();
    check(s, context);
    if (need == 0) return std::string();

    // Sized need + 1, not need: the C side always writes a NUL terminator, and
    // writing to s[s.size()] is undefined behaviour even though the character is
    // readable. Allocating the extra byte inside size() keeps the write in bounds;
    // the resize afterwards trims it back off.
    std::string out(need + 1, '\0');
    s = call(&out[0], need + 1, &need);
    check(s, context);
    out.resize(need);
    return out;
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Event — borrowed for the duration of a callback, never owned
// -----------------------------------------------------------------------------
class Element;

class Event {
public:
    explicit Event(KuiEvent* raw) noexcept : raw_(raw) {}

    std::string type() const {
        return detail::readString(
            [this](char* b, std::size_t c, std::size_t* n) {
                return kui_event_get_type(raw_, b, c, n);
            },
            "event type");
    }

    Element target() const;
    Element currentTarget() const;

    void preventDefault()  { detail::check(kui_event_prevent_default(raw_), "preventDefault"); }
    void stopPropagation() { detail::check(kui_event_stop_propagation(raw_), "stopPropagation"); }

    bool defaultPrevented() const {
        int prevented = 0;
        detail::check(kui_event_get_flags(raw_, nullptr, nullptr, &prevented), "event flags");
        return prevented != 0;
    }

    // Returns false when this is not a mouse event, rather than throwing: a
    // listener bound to several event types probes rather than knows.
    bool mousePosition(float& x, float& y) const {
        return kui_event_mouse_get_client_xy(raw_, &x, &y) == KUI_OK;
    }

    bool keyName(std::string& out) const {
        std::size_t need = 0;
        if (kui_event_key_get_key(raw_, nullptr, 0, &need) != KUI_OK) return false;
        out.assign(need + 1, '\0');   // + 1 for the terminator, see readString
        if (need && kui_event_key_get_key(raw_, &out[0], need + 1, &need) != KUI_OK) return false;
        out.resize(need);
        return true;
    }

    KuiEvent* raw() const noexcept { return raw_; }

private:
    KuiEvent* raw_;
};

// -----------------------------------------------------------------------------
// ListenerHandle — RAII registration
//
// Holding the std::function is the whole problem this solves. A C callback is a
// function pointer plus void*, so the closure has to live somewhere with a
// definite owner; this object is that owner, and unregisters on destruction.
// -----------------------------------------------------------------------------
class ListenerHandle {
public:
    ListenerHandle() = default;

    ListenerHandle(ListenerHandle&& other) noexcept { *this = std::move(other); }

    ListenerHandle& operator=(ListenerHandle&& other) noexcept {
        if (this != &other) {
            reset();
            element_ = other.element_;
            type_    = std::move(other.type_);
            id_      = other.id_;
            holder_  = std::move(other.holder_);
            other.element_ = nullptr;
            other.id_      = 0;
        }
        return *this;
    }

    ListenerHandle(const ListenerHandle&)            = delete;
    ListenerHandle& operator=(const ListenerHandle&) = delete;

    ~ListenerHandle() { reset(); }

    void reset() noexcept {
        if (element_) {
            // Unregister before the holder dies, or a dispatch in flight would
            // call through a freed closure.
            kui_element_remove_event_listener(element_, type_.c_str(), type_.size(), id_);
            element_ = nullptr;
        }
        holder_.reset();
    }

    explicit operator bool() const noexcept { return element_ != nullptr; }

private:
    friend class Element;

    struct Holder {
        std::function<void(Event&)> fn;
    };

    KuiElement*             element_ = nullptr;
    std::string             type_;
    KuiListenerId           id_ = 0;
    std::unique_ptr<Holder> holder_;
};

// -----------------------------------------------------------------------------
// Element — reference counted, so copyable
// -----------------------------------------------------------------------------
class Element {
public:
    Element() = default;

    // Adopts an existing reference; does not take another.
    static Element adopt(KuiElement* raw) noexcept {
        Element e;
        e.raw_ = raw;
        return e;
    }

    Element(const Element& other) : raw_(other.raw_) {
        if (raw_) kui_element_retain(raw_);
    }

    Element(Element&& other) noexcept : raw_(other.raw_) { other.raw_ = nullptr; }

    Element& operator=(const Element& other) {
        if (this != &other) {
            Element copy(other);
            swap(copy);
        }
        return *this;
    }

    Element& operator=(Element&& other) noexcept {
        if (this != &other) {
            reset();
            raw_ = other.raw_;
            other.raw_ = nullptr;
        }
        return *this;
    }

    ~Element() { reset(); }

    void reset() noexcept {
        if (raw_) {
            kui_element_release(raw_);
            raw_ = nullptr;
        }
    }

    void swap(Element& other) noexcept { std::swap(raw_, other.raw_); }

    explicit operator bool() const noexcept { return raw_ != nullptr; }
    KuiElement* raw() const noexcept { return raw_; }

    // --- attributes ---
    std::string tagName() const {
        return detail::readString(
            [this](char* b, std::size_t c, std::size_t* n) {
                return kui_element_get_tag_name(raw_, b, c, n);
            },
            "tagName");
    }

    std::string id() const {
        return detail::readString(
            [this](char* b, std::size_t c, std::size_t* n) {
                return kui_element_get_id(raw_, b, c, n);
            },
            "id");
    }

    bool hasAttribute(const std::string& name) const {
        int has = 0;
        detail::check(kui_element_has_attribute(raw_, name.c_str(), name.size(), &has),
                      "hasAttribute");
        return has != 0;
    }

    // Empty for an absent attribute, matching the native API. Use hasAttribute
    // to tell absent from present-but-empty.
    std::string getAttribute(const std::string& name) const {
        return detail::readString(
            [this, &name](char* b, std::size_t c, std::size_t* n) {
                return kui_element_get_attribute(raw_, name.c_str(), name.size(), b, c, n);
            },
            "getAttribute");
    }

    void setAttribute(const std::string& name, const std::string& value) {
        detail::check(kui_element_set_attribute(raw_, name.c_str(), name.size(),
                                                value.c_str(), value.size()),
                      "setAttribute");
    }

    void removeAttribute(const std::string& name) {
        detail::check(kui_element_remove_attribute(raw_, name.c_str(), name.size()),
                      "removeAttribute");
    }

    // --- text ---
    std::string textContent() const {
        return detail::readString(
            [this](char* b, std::size_t c, std::size_t* n) {
                return kui_element_get_text_content(raw_, b, c, n);
            },
            "textContent");
    }

    void setTextContent(const std::string& text) {
        detail::check(kui_element_set_text_content(raw_, text.c_str(), text.size()),
                      "setTextContent");
    }

    // --- inline style ---
    void setStyleProperty(const std::string& prop, const std::string& value,
                          bool important = false) {
        detail::check(kui_element_set_style_property(raw_, prop.c_str(), prop.size(),
                                                     value.c_str(), value.size(),
                                                     important ? 1 : 0),
                      "setStyleProperty");
    }

    // Empty when the property is not set.
    std::string styleProperty(const std::string& prop) const {
        return detail::readString(
            [this, &prop](char* b, std::size_t c, std::size_t* n) {
                return kui_element_get_style_property(raw_, prop.c_str(), prop.size(), b, c, n);
            },
            "styleProperty");
    }

    // --- geometry ---
    KuiRect boundingRect() const {
        KuiRect r{};
        detail::check(kui_element_get_bounding_rect(raw_, &r), "boundingRect");
        return r;
    }

    // --- events ---
    // The returned handle owns the closure and unregisters on destruction. Drop
    // it on the floor and the listener goes away immediately -- which is the
    // behaviour that makes the ownership obvious rather than surprising.
    [[nodiscard]] ListenerHandle addEventListener(const std::string& type,
                                                  std::function<void(Event&)> fn) {
        auto holder = std::make_unique<ListenerHandle::Holder>();
        holder->fn = std::move(fn);

        KuiListenerId id = 0;
        detail::check(kui_element_add_event_listener(
                          raw_, type.c_str(), type.size(), &trampoline, holder.get(), &id),
                      "addEventListener");

        ListenerHandle h;
        h.element_ = raw_;
        h.type_    = type;
        h.id_      = id;
        h.holder_  = std::move(holder);
        return h;
    }

    void dispatchEvent(const std::string& type, bool bubbles = true, bool cancelable = true) {
        detail::check(kui_element_dispatch_event(raw_, type.c_str(), type.size(),
                                                 bubbles ? 1 : 0, cancelable ? 1 : 0, nullptr),
                      "dispatchEvent");
    }

private:
    // The C side calls this; it recovers the closure from user_data. Exceptions
    // must not escape into the C ABI, so they stop here.
    static void trampoline(KuiEvent* raw, void* user_data) noexcept {
        auto* holder = static_cast<ListenerHandle::Holder*>(user_data);
        if (!holder || !holder->fn) return;
        try {
            Event ev(raw);
            holder->fn(ev);
        } catch (...) {
        }
    }

    KuiElement* raw_ = nullptr;
};

inline Element Event::target() const {
    KuiElement* out = nullptr;
    const KuiStatus s = kui_event_get_target(raw_, &out);
    if (s == KUI_ERR_NOT_FOUND) return Element();
    detail::check(s, "event target");
    return Element::adopt(out);
}

inline Element Event::currentTarget() const {
    KuiElement* out = nullptr;
    const KuiStatus s = kui_event_get_current_target(raw_, &out);
    if (s == KUI_ERR_NOT_FOUND) return Element();
    detail::check(s, "event currentTarget");
    return Element::adopt(out);
}

// -----------------------------------------------------------------------------
// Document — reference counted, so copyable
// -----------------------------------------------------------------------------
class Document {
public:
    Document() = default;

    static Document create() {
        KuiDocument* raw = nullptr;
        detail::check(kui_document_create(&raw), "Document::create");
        return adopt(raw);
    }

    static Document adopt(KuiDocument* raw) noexcept {
        Document d;
        d.raw_ = raw;
        return d;
    }

    Document(const Document& other) : raw_(other.raw_) {
        if (raw_) kui_document_retain(raw_);
    }

    Document(Document&& other) noexcept : raw_(other.raw_) { other.raw_ = nullptr; }

    Document& operator=(const Document& other) {
        if (this != &other) {
            Document copy(other);
            std::swap(raw_, copy.raw_);
        }
        return *this;
    }

    Document& operator=(Document&& other) noexcept {
        if (this != &other) {
            reset();
            raw_ = other.raw_;
            other.raw_ = nullptr;
        }
        return *this;
    }

    ~Document() { reset(); }

    void reset() noexcept {
        if (raw_) {
            kui_document_release(raw_);
            raw_ = nullptr;
        }
    }

    explicit operator bool() const noexcept { return raw_ != nullptr; }
    KuiDocument* raw() const noexcept { return raw_; }

    void loadHTML(const std::string& html) {
        detail::check(kui_document_load_html(raw_, html.c_str(), html.size()), "loadHTML");
    }

    // Empty handle when nothing matches, mirroring the native API's nullptr.
    Element getElementById(const std::string& id) const {
        return lookup([&](KuiElement** out) {
            return kui_document_get_element_by_id(raw_, id.c_str(), id.size(), out);
        }, "getElementById");
    }

    Element querySelector(const std::string& selector) const {
        return lookup([&](KuiElement** out) {
            return kui_document_query_selector(raw_, selector.c_str(), selector.size(), out);
        }, "querySelector");
    }

    Element body() const {
        return lookup([&](KuiElement** out) { return kui_document_body(raw_, out); }, "body");
    }

    Element createElement(const std::string& tag) {
        return lookup([&](KuiElement** out) {
            return kui_document_create_element(raw_, tag.c_str(), tag.size(), out);
        }, "createElement");
    }

private:
    template <typename Fn>
    static Element lookup(Fn&& call, const char* context) {
        KuiElement* out = nullptr;
        const KuiStatus s = call(&out);
        if (s == KUI_ERR_NOT_FOUND) return Element();
        detail::check(s, context);
        return Element::adopt(out);
    }

    KuiDocument* raw_ = nullptr;
};

// -----------------------------------------------------------------------------
// Window — uniquely owned, so move-only
// -----------------------------------------------------------------------------
struct WindowConfig {
    std::string title;
    int  width  = 0;          // 0 = library default
    int  height = 0;
    bool centered      = false;
    bool transparent   = false;
    bool alwaysOnTop   = false;
    bool maximized     = false;
    bool frameless     = false;
    bool noResize      = false;   // negated for the same reason as the C struct
    bool noDecorations = false;
    float opacity      = 0.0f;    // 0 = fully opaque
};

class Window {
public:
    Window() = default;

    static Window create(const WindowConfig& cfg = {}) {
        KuiWindowProps props;
        std::memset(&props, 0, sizeof props);
        if (!cfg.title.empty()) {
            props.title     = cfg.title.c_str();
            props.title_len = cfg.title.size();
        }
        props.width            = cfg.width;
        props.height           = cfg.height;
        props.centered         = cfg.centered ? 1 : 0;
        props.transparent      = cfg.transparent ? 1 : 0;
        props.always_on_top    = cfg.alwaysOnTop ? 1 : 0;
        props.maximized        = cfg.maximized ? 1 : 0;
        props.frameless        = cfg.frameless ? 1 : 0;
        props.no_resize        = cfg.noResize ? 1 : 0;
        props.no_decorations   = cfg.noDecorations ? 1 : 0;
        props.opacity          = cfg.opacity;

        KuiWindow* raw = nullptr;
        detail::check(kui_window_create(&props, &raw), "Window::create");
        Window w;
        w.raw_ = raw;
        return w;
    }

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept : raw_(other.raw_) { other.raw_ = nullptr; }

    Window& operator=(Window&& other) noexcept {
        if (this != &other) {
            reset();
            raw_ = other.raw_;
            other.raw_ = nullptr;
        }
        return *this;
    }

    ~Window() { reset(); }

    // Destroys the window -- unique ownership, not a reference drop.
    void reset() noexcept {
        if (raw_) {
            kui_window_release(raw_);
            raw_ = nullptr;
        }
    }

    explicit operator bool() const noexcept { return raw_ != nullptr; }
    KuiWindow* raw() const noexcept { return raw_; }

    // The window owns this document; the returned handle holds a reference, so it
    // stays usable even after the window is destroyed.
    Document document() const {
        KuiDocument* out = nullptr;
        detail::check(kui_window_get_document(raw_, &out), "Window::document");
        return Document::adopt(out);
    }

    void show() { detail::check(kui_window_show(raw_), "show"); }
    void hide() { detail::check(kui_window_hide(raw_), "hide"); }

    bool isVisible() const {
        int visible = 0;
        detail::check(kui_window_is_visible(raw_, &visible), "isVisible");
        return visible != 0;
    }

    std::string title() const {
        return detail::readString(
            [this](char* b, std::size_t c, std::size_t* n) {
                return kui_window_get_title(raw_, b, c, n);
            },
            "title");
    }

    void setTitle(const std::string& t) {
        detail::check(kui_window_set_title(raw_, t.c_str(), t.size()), "setTitle");
    }

    void resize(int width, int height) {
        detail::check(kui_window_resize(raw_, width, height), "resize");
    }

    void* nativeHandle() const {
        void* h = nullptr;
        detail::check(kui_window_get_native_handle(raw_, &h), "nativeHandle");
        return h;
    }

private:
    KuiWindow* raw_ = nullptr;
};

// -----------------------------------------------------------------------------
// Platform — static, mirroring the C API's handle-free shape
// -----------------------------------------------------------------------------
class TimerHandle;

class Platform {
public:
    static void init()     { detail::check(kui_platform_init(), "Platform::init"); }
    static void shutdown() { detail::check(kui_platform_shutdown(), "Platform::shutdown"); }

    static int run() {
        int code = 0;
        detail::check(kui_platform_run(&code), "Platform::run");
        return code;
    }

    static void quit(int exitCode = 0) {
        detail::check(kui_platform_quit(exitCode), "Platform::quit");
    }

    static void pollEvents() { detail::check(kui_platform_poll_events(), "pollEvents"); }

    // Timers take a std::function, so the closure needs an owner for as long as
    // the timer can fire. TimerHandle below is that owner.
    static TimerHandle setTimeout(std::function<void()> fn, unsigned delayMs);
    static TimerHandle setInterval(std::function<void()> fn, unsigned intervalMs);

private:
    friend class TimerHandle;

    struct TaskHolder {
        std::function<void()> fn;
    };

    static void taskTrampoline(void* user_data) noexcept {
        auto* holder = static_cast<TaskHolder*>(user_data);
        if (!holder || !holder->fn) return;
        try {
            holder->fn();
        } catch (...) {
        }
    }
};

// RAII timer registration, same shape as ListenerHandle and for the same reason.
class TimerHandle {
public:
    TimerHandle() = default;

    TimerHandle(TimerHandle&& other) noexcept { *this = std::move(other); }

    TimerHandle& operator=(TimerHandle&& other) noexcept {
        if (this != &other) {
            reset();
            id_     = other.id_;
            armed_  = other.armed_;
            holder_ = std::move(other.holder_);
            other.id_    = 0;
            other.armed_ = false;
        }
        return *this;
    }

    TimerHandle(const TimerHandle&)            = delete;
    TimerHandle& operator=(const TimerHandle&) = delete;

    ~TimerHandle() { reset(); }

    void reset() noexcept {
        if (armed_) {
            kui_platform_clear_timer(id_);
            armed_ = false;
        }
        holder_.reset();
    }

    // Let a one-shot timer outlive this handle. The closure is leaked
    // deliberately -- a fire-and-forget timeout has no other owner, and the
    // alternative is a use-after-free when it fires.
    void release() noexcept {
        armed_ = false;
        (void)holder_.release();
    }

    explicit operator bool() const noexcept { return armed_; }

private:
    friend class Platform;

    KuiTimerId                          id_ = 0;
    bool                                armed_ = false;
    std::unique_ptr<Platform::TaskHolder> holder_;
};

inline TimerHandle Platform::setTimeout(std::function<void()> fn, unsigned delayMs) {
    auto holder = std::make_unique<TaskHolder>();
    holder->fn = std::move(fn);
    KuiTimerId id = 0;
    detail::check(kui_platform_set_timeout(&taskTrampoline, holder.get(), delayMs, &id),
                  "setTimeout");
    TimerHandle h;
    h.id_     = id;
    h.armed_  = true;
    h.holder_ = std::move(holder);
    return h;
}

inline TimerHandle Platform::setInterval(std::function<void()> fn, unsigned intervalMs) {
    auto holder = std::make_unique<TaskHolder>();
    holder->fn = std::move(fn);
    KuiTimerId id = 0;
    detail::check(kui_platform_set_interval(&taskTrampoline, holder.get(), intervalMs, &id),
                  "setInterval");
    TimerHandle h;
    h.id_     = id;
    h.armed_  = true;
    h.holder_ = std::move(holder);
    return h;
}

}  // namespace kui
KUI_CPP_HIDDEN_END
