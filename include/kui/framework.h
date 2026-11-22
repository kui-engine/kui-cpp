// Application framework for the C++ Wrapper (C ABI section 11, 3.61.8): router,
// data binding, animation state queries and Mutation / Resize / Intersection
// observers, with the native SDK's type and member names. Included by full_profile.h.
//
// Differences from the native SDK, all because only the C ABI is visible here:
//   - Observable<T> keeps its value as text (native bindings go through
//     toString / fromString as well) and get() parses it back. There is no
//     beginBatch / endBatch / forceNotify.
//   - A route's own beforeEnter / beforeLeave become global guards that only act
//     when the route is the target / the current one.
//   - Observers register themselves when they observe: the registries'
//     registerObserver() does nothing and unregisterObserver() disconnects.
//     MutationObserver::takeRecords() returns nothing (records arrive at once).
//   - AnimationManager / TransitionManager only answer queries; control CSS
//     animations through CSS (class, inline style, animation-play-state).
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

namespace detail {

inline String text(const char* data, std::size_t size) {
    return data && size ? String(data, size) : String();
}

template <typename F>
void deleteCallback(void* user_data) {
    delete static_cast<F*>(user_data);
}

// Wraps an element handle the C layer lends to a callback into the document's
// element cache, adding the reference the cache will own.
inline RefPtr<Element> shareElement(const std::shared_ptr<DocumentState>& state, KuiElement* raw) {
    if (!state || !raw || kui_element_retain(raw) != KUI_OK) return {};
    return state->wrap(raw);
}

}  // namespace detail

// --- Router ---------------------------------------------------------------------
using RouteParams = std::unordered_map<String, String>;

struct RouteContext {
    String path;
    String pattern;
    RouteParams params;
    String query;
    std::unordered_map<String, String> queryParams;
};

using NavigationGuard = std::function<bool(const RouteContext& from, const RouteContext& to)>;

struct RouteDefinition {
    String path;
    using ViewFactory = std::function<RefPtr<Node>(Document*, const RouteParams&)>;
    ViewFactory factory;
    NavigationGuard beforeEnter;
    NavigationGuard beforeLeave;
    std::unordered_map<String, String> meta;
    String name;
};

namespace detail {

inline RouteContext toRouteContext(const KuiCRouteInfo* raw) {
    RouteContext route;
    if (!raw) return route;
    route.path = text(raw->path, raw->path_len);
    route.pattern = text(raw->pattern, raw->pattern_len);
    route.query = text(raw->query, raw->query_len);
    for (std::size_t i = 0; i < raw->param_count; ++i) {
        route.params[text(raw->params[i].key, raw->params[i].key_len)] =
            text(raw->params[i].value, raw->params[i].value_len);
    }
    for (std::size_t i = 0; i < raw->query_param_count; ++i) {
        route.queryParams[text(raw->query_params[i].key, raw->query_params[i].key_len)] =
            text(raw->query_params[i].value, raw->query_params[i].value_len);
    }
    return route;
}

struct RouteViewBox {
    RouteDefinition::ViewFactory factory;
    Document* document = nullptr;
};

inline KuiElement* routeViewTrampoline(KuiDocument*, const KuiCRouteInfo* route, void* user_data) {
    auto* box = static_cast<RouteViewBox*>(user_data);
    try {
        RefPtr<Node> view = box->factory(box->document, toRouteContext(route).params);
        if (!view || kui_element_retain(view->cHandle()) != KUI_OK) return nullptr;
        return view->cHandle();  // the extra reference is handed to KUI
    } catch (...) {
        return nullptr;
    }
}

inline int routeGuardTrampoline(const KuiCRouteInfo* from, const KuiCRouteInfo* to, void* user_data) {
    try {
        return (*static_cast<NavigationGuard*>(user_data))(toRouteContext(from), toRouteContext(to)) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

using RouteInfoCallback = std::function<void(const RouteContext&)>;

inline void routeInfoTrampoline(const KuiCRouteInfo* route, void* user_data) {
    try {
        (*static_cast<RouteInfoCallback*>(user_data))(toRouteContext(route));
    } catch (...) {
    }
}

}  // namespace detail

class Router {
public:
    explicit Router(Document* document) : document_(document) {}
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    bool addRoute(const String& path, RouteDefinition::ViewFactory factory, const String& name = "") {
        RouteDefinition def;
        def.path = path;
        def.factory = std::move(factory);
        def.name = name;
        return addRoute(std::move(def));
    }
    bool addRoute(RouteDefinition def) {
        if (def.path.empty() || !def.factory) return false;
        auto* box = new detail::RouteViewBox{def.factory, document_};
        const KuiStatus status = kui_document_router_add_route(
            raw(), def.path.data(), def.path.size(), def.name.data(), def.name.size(),
            detail::routeViewTrampoline, box, detail::deleteCallback<detail::RouteViewBox>);
        if (status != KUI_OK) {
            delete box;
            return false;
        }
        Entry entry;
        const String pattern = def.path;
        if (def.beforeEnter) {
            NavigationGuard guard = def.beforeEnter;
            entry.enterGuard = addBeforeEnter([pattern, guard](const RouteContext& from, const RouteContext& to) {
                return to.pattern != pattern || guard(from, to);
            });
        }
        if (def.beforeLeave) {
            NavigationGuard guard = def.beforeLeave;
            entry.leaveGuard = addBeforeLeave([pattern, guard](const RouteContext& from, const RouteContext& to) {
                return from.pattern != pattern || guard(from, to);
            });
        }
        entry.def = std::move(def);
        routes_.push_back(std::move(entry));
        return true;
    }
    bool removeRoute(const String& path) {
        if (kui_document_router_remove_route(raw(), path.data(), path.size()) != KUI_OK) return false;
        for (auto it = routes_.begin(); it != routes_.end(); ++it) {
            if (it->def.path != path) continue;
            dropGuards(*it);
            routes_.erase(it);
            break;
        }
        return true;
    }
    void clearRoutes() {
        detail::check(kui_document_router_clear_routes(raw()), "Router::clearRoutes");
        for (auto& entry : routes_) dropGuards(entry);
        routes_.clear();
    }

    bool push(const String& path) { return navigate(kui_document_router_push, path, "Router::push"); }
    bool replace(const String& path) { return navigate(kui_document_router_replace, path, "Router::replace"); }
    bool back() { return go(-1); }
    bool forward() { return go(1); }
    bool go(int delta) {
        int navigated = 0;
        detail::check(kui_document_router_go(raw(), delta, &navigated), "Router::go");
        return navigated != 0;
    }

    const RouteContext& currentRoute() const {
        detail::RouteInfoCallback store = [this](const RouteContext& route) { current_ = route; };
        detail::check(kui_document_router_get_current(raw(), detail::routeInfoTrampoline, &store),
                      "Router::currentRoute");
        return current_;
    }
    String currentPath() const {
        return detail::readString([this](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_document_router_get_current_path(raw(), buffer, capacity, size);
        }, "Router::currentPath");
    }
    // A ":name" parameter of the current route; nullopt when the route has no such parameter.
    Optional<String> param(const String& name) const {
        std::size_t size = 0;
        KuiStatus status = kui_document_router_get_param(raw(), name.data(), name.size(), nullptr, 0, &size);
        if (status == KUI_ERR_NOT_FOUND) return std::nullopt;
        detail::check(status, "Router::param");
        String value(size + 1, '\0');
        detail::check(kui_document_router_get_param(raw(), name.data(), name.size(), value.data(), value.size(), &size),
                      "Router::param");
        value.resize(size);
        return value;
    }
    const RouteParams& currentParams() const { return currentRoute().params; }
    int historyIndex() const { return history().first; }
    int historySize() const { return history().second; }

    u32 addBeforeEnter(NavigationGuard guard) { return addGuard(KUI_C_ROUTE_GUARD_BEFORE_ENTER, std::move(guard)); }
    u32 addBeforeLeave(NavigationGuard guard) { return addGuard(KUI_C_ROUTE_GUARD_BEFORE_LEAVE, std::move(guard)); }
    void removeGuard(u32 id) { detail::check(kui_document_router_remove_guard(raw(), id), "Router::removeGuard"); }

    // <kui-router-view> registers itself; register any other container by hand.
    void registerView(Element* view) {
        if (view) detail::check(kui_document_router_register_view(raw(), view->cHandle()), "Router::registerView");
    }
    // false when the view was not registered.
    bool unregisterView(Element* view) {
        return view && kui_document_router_unregister_view(raw(), view->cHandle()) == KUI_OK;
    }

    using AfterNavigateCallback = std::function<void(const RouteContext&)>;
    u32 onAfterNavigate(AfterNavigateCallback callback) {
        if (!callback) return 0;
        auto* box = new detail::RouteInfoCallback(std::move(callback));
        u32 id = 0;
        if (kui_document_router_add_navigated_callback(raw(), detail::routeInfoTrampoline, box,
                                                       detail::deleteCallback<detail::RouteInfoCallback>,
                                                       &id) != KUI_OK) {
            delete box;
            return 0;
        }
        return id;
    }
    void removeAfterNavigate(u32 id) {
        detail::check(kui_document_router_remove_navigated_callback(raw(), id), "Router::removeAfterNavigate");
    }

    u32 routeCount() const {
        uint32_t count = 0;
        detail::check(kui_document_router_get_route_count(raw(), &count), "Router::routeCount");
        return count;
    }
    bool hasRoute(const String& path) const {
        int has = 0;
        detail::check(kui_document_router_has_route(raw(), path.data(), path.size(), &has), "Router::hasRoute");
        return has != 0;
    }
    const RouteDefinition* findRoute(const String& path) const {
        for (const auto& entry : routes_) {
            if (entry.def.path == path) return &entry.def;
        }
        return nullptr;
    }

private:
    struct Entry {
        RouteDefinition def;
        u32 enterGuard = 0;
        u32 leaveGuard = 0;
    };

    KuiDocument* raw() const { return document_->cHandle(); }
    using Navigate = KuiStatus (*)(KuiDocument*, const char*, std::size_t, int*);
    bool navigate(Navigate call, const String& path, const char* context) {
        int navigated = 0;
        detail::check(call(raw(), path.data(), path.size(), &navigated), context);
        return navigated != 0;
    }
    std::pair<int, int> history() const {
        int index = -1, size = 0;
        detail::check(kui_document_router_get_history(raw(), &index, &size), "Router::history");
        return {index, size};
    }
    u32 addGuard(KuiCRouteGuardKind kind, NavigationGuard guard) {
        if (!guard) return 0;
        auto* box = new NavigationGuard(std::move(guard));
        u32 id = 0;
        if (kui_document_router_add_guard(raw(), kind, detail::routeGuardTrampoline, box,
                                          detail::deleteCallback<NavigationGuard>, &id) != KUI_OK) {
            delete box;
            return 0;
        }
        return id;
    }
    void dropGuards(const Entry& entry) {
        if (entry.enterGuard) removeGuard(entry.enterGuard);
        if (entry.leaveGuard) removeGuard(entry.leaveGuard);
    }

    Document* document_;
    std::vector<Entry> routes_;
    mutable RouteContext current_;
};

// --- Data binding ---------------------------------------------------------------
using SubscriptionId = u32;
using BindingId = u32;

namespace detail {

inline String obsToString(const String& v) { return v; }
inline String obsToString(const char* v) { return v ? String(v) : String(); }
inline String obsToString(bool v) { return v ? "true" : "false"; }
inline String obsToString(int v) { return std::to_string(v); }
inline String obsToString(unsigned int v) { return std::to_string(v); }
inline String obsToString(long v) { return std::to_string(v); }
inline String obsToString(unsigned long v) { return std::to_string(v); }
inline String obsToString(long long v) { return std::to_string(v); }
inline String obsToString(unsigned long long v) { return std::to_string(v); }
inline String obsToString(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}
inline String obsToString(float v) { return obsToString(static_cast<double>(v)); }

inline void obsFromString(const String& s, String& out) { out = s; }
inline void obsFromString(const String& s, bool& out) { out = s == "true" || s == "1"; }
template <typename T>
void obsFromNumber(const String& s, T& out) {
    if (s.empty()) return;
    char* end = nullptr;
    if (std::is_floating_point<T>::value) {
        const double value = std::strtod(s.c_str(), &end);
        if (end && *end == '\0') out = static_cast<T>(value);
    } else if (std::is_signed<T>::value) {
        const long long value = std::strtoll(s.c_str(), &end, 10);
        if (end && *end == '\0') out = static_cast<T>(value);
    } else {
        const unsigned long long value = std::strtoull(s.c_str(), &end, 10);
        if (end && *end == '\0') out = static_cast<T>(value);
    }
}
inline void obsFromString(const String& s, int& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, unsigned int& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, long& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, unsigned long& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, long long& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, unsigned long long& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, float& out) { obsFromNumber(s, out); }
inline void obsFromString(const String& s, double& out) { obsFromNumber(s, out); }

using ChangeCallback = std::function<void()>;
inline void changeTrampoline(void* user_data) {
    try {
        (*static_cast<ChangeCallback*>(user_data))();
    } catch (...) {
    }
}

}  // namespace detail

class ObservableBase {
public:
    ObservableBase() { detail::check(kui_observable_create(nullptr, 0, &raw_), "Observable::create"); }
    virtual ~ObservableBase() {
        if (raw_) kui_observable_release(raw_);
    }
    ObservableBase(const ObservableBase&) = delete;
    ObservableBase& operator=(const ObservableBase&) = delete;

    SubscriptionId subscribe(std::function<void()> callback) {
        if (!callback) return 0;
        auto* box = new detail::ChangeCallback(std::move(callback));
        u32 id = 0;
        if (kui_observable_subscribe(raw_, detail::changeTrampoline, box,
                                     detail::deleteCallback<detail::ChangeCallback>, &id) != KUI_OK) {
            delete box;
            return 0;
        }
        subscriptions_.push_back(id);
        return id;
    }
    // false when id is not a live subscription of this observable.
    bool unsubscribe(SubscriptionId id) {
        const bool removed = kui_observable_unsubscribe(raw_, id) == KUI_OK;
        subscriptions_.erase(std::remove(subscriptions_.begin(), subscriptions_.end(), id),
                             subscriptions_.end());
        return removed;
    }
    // Removes every binding of this observable, including those made through a BindingManager (which
    // keeps listing them until it unbinds them itself).
    void unbindAll() { detail::check(kui_observable_unbind_all(raw_), "Observable::unbindAll"); }
    void clearSubscriptions() {
        for (SubscriptionId id : subscriptions_) (void)kui_observable_unsubscribe(raw_, id);
        subscriptions_.clear();
    }
    String toString() const { return text(); }
    KuiObservable* cHandle() const noexcept { return raw_; }

protected:
    void setText(const String& value) {
        detail::check(kui_observable_set(raw_, value.data(), value.size()), "Observable::set");
    }
    String text() const {
        return detail::readString(
            [this](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_observable_get(raw_, buffer, capacity, size);
            }, "Observable::get");
    }

private:
    KuiObservable* raw_ = nullptr;
    std::vector<SubscriptionId> subscriptions_;
};

// Supported types: String, bool, int, unsigned, long, long long, float, double.
template <typename T>
class Observable : public ObservableBase {
public:
    Observable() { setText(detail::obsToString(value_)); }
    explicit Observable(const T& initial) : value_(initial) { setText(detail::obsToString(initial)); }

    const T& get() const {
        detail::obsFromString(text(), value_);
        return value_;
    }
    void set(const T& value) { setText(detail::obsToString(value)); }
    Observable& operator=(const T& value) {
        set(value);
        return *this;
    }
    template <typename Fn>
    void modify(Fn&& fn) {
        T value = get();
        fn(value);
        set(value);
    }
    operator const T&() const { return get(); }

private:
    mutable T value_{};
};

class BindingManager {
public:
    explicit BindingManager(Document* document) : document_(document) {}
    ~BindingManager() { unbindAll(); }
    BindingManager(const BindingManager&) = delete;
    BindingManager& operator=(const BindingManager&) = delete;

    BindingId bind(Element* element, const String& property, ObservableBase* observable) {
        return add(element, property, observable, false);
    }
    BindingId bindTwoWay(Element* element, const String& property, ObservableBase* observable) {
        return add(element, property, observable, true);
    }
    void unbind(BindingId id) {
        auto found = records_.find(id);
        if (found == records_.end()) return;
        Record record = found->second;
        records_.erase(found);
        (void)kui_observable_unbind(record.observable, record.binding);
        kui_observable_release(record.observable);
    }
    void unbindElement(Element* element) {
        std::vector<BindingId> ids;
        for (const auto& entry : records_) {
            if (entry.second.element == element) ids.push_back(entry.first);
        }
        for (BindingId id : ids) unbind(id);
    }
    void unbindAll() {
        while (!records_.empty()) unbind(records_.begin()->first);
    }
    void flush() { detail::check(kui_document_flush_bindings(document_->cHandle()), "BindingManager::flush"); }
    u32 bindingCount() const { return static_cast<u32>(records_.size()); }

private:
    struct Record {
        KuiObservable* observable = nullptr;
        u32 binding = 0;
        const Element* element = nullptr;
    };
    BindingId add(Element* element, const String& property, ObservableBase* observable, bool twoWay) {
        if (!element || !observable) return 0;
        u32 binding = 0;
        if (kui_observable_bind(observable->cHandle(), element->cHandle(), property.data(), property.size(),
                                twoWay ? 1 : 0, &binding) != KUI_OK) {
            return 0;
        }
        kui_observable_retain(observable->cHandle());  // the binding outlives a destroyed Observable safely
        const BindingId id = next_++;
        records_[id] = Record{observable->cHandle(), binding, element};
        return id;
    }

    Document* document_;
    std::unordered_map<BindingId, Record> records_;
    BindingId next_ = 1;
};

// --- Animation state (queries only) --------------------------------------------
class AnimationManager {
public:
    explicit AnimationManager(Document* document) : document_(document) {}
    bool hasActiveAnimations(const Element* element) const {
        int animating = 0;
        if (element) detail::check(kui_element_get_animation_state(element->cHandle(), &animating, nullptr),
                                   "AnimationManager::hasActiveAnimations");
        return animating != 0;
    }
    u32 activeAnimationCount() const {
        uint32_t count = 0;
        detail::check(kui_document_get_animation_counts(document_->cHandle(), &count, nullptr),
                      "AnimationManager::activeAnimationCount");
        return count;
    }

private:
    Document* document_;
};

class TransitionManager {
public:
    explicit TransitionManager(Document* document) : document_(document) {}
    bool hasActiveTransitions(const Element* element) const {
        int transitioning = 0;
        if (element) detail::check(kui_element_get_animation_state(element->cHandle(), nullptr, &transitioning),
                                   "TransitionManager::hasActiveTransitions");
        return transitioning != 0;
    }
    u32 activeTransitionCount() const {
        uint32_t count = 0;
        detail::check(kui_document_get_animation_counts(document_->cHandle(), nullptr, &count),
                      "TransitionManager::activeTransitionCount");
        return count;
    }

private:
    Document* document_;
};

// --- Observers ------------------------------------------------------------------
struct EdgeInsets {
    f32 top = 0;
    f32 right = 0;
    f32 bottom = 0;
    f32 left = 0;
    constexpr EdgeInsets() = default;
    constexpr EdgeInsets(f32 all) : top(all), right(all), bottom(all), left(all) {}
    constexpr EdgeInsets(f32 t, f32 r, f32 b, f32 l) : top(t), right(r), bottom(b), left(l) {}
};

struct MutationRecord {
    enum class Type : u8 { ChildList, Attributes, CharacterData };
    Type type = Type::ChildList;
    RefPtr<Node> target;           // a text node's parent element for CharacterData
    RefPtr<Node> addedNode;        // elements only; text / comment nodes are nullptr
    RefPtr<Node> removedNode;
    RefPtr<Node> previousSibling;
    RefPtr<Node> nextSibling;
    String attributeName;
    String oldValue;
    // DOM nodeType of the underlying nodes (0 none, 1 element, 3 text, 8 comment): a text node shows
    // as nullptr in addedNode / removedNode with the type 3 here. A CharacterData target is 3.
    int targetNodeType = 0;
    int addedNodeType = 0;
    int removedNodeType = 0;
};

struct MutationObserverInit {
    bool childList = false;
    bool attributes = false;
    bool characterData = false;
    bool subtree = false;
    bool attributeOldValue = false;
    bool characterDataOldValue = false;
    std::vector<String> attributeFilter;
};

struct ResizeObserverSize {
    f32 inlineSize = 0;
    f32 blockSize = 0;
};

struct ResizeObserverEntry {
    Element* target = nullptr;
    ResizeObserverSize contentBoxSize;
    ResizeObserverSize borderBoxSize;
    f32 contentRectX = 0;
    f32 contentRectY = 0;
    f32 contentRectWidth = 0;
    f32 contentRectHeight = 0;
};

struct IntersectionObserverEntry {
    Element* target = nullptr;
    Rect boundingClientRect;
    Rect intersectionRect;
    Rect rootBounds;
    f32 intersectionRatio = 0.0f;
    bool isIntersecting = false;
};

struct IntersectionObserverInit {
    Element* root = nullptr;
    EdgeInsets rootMargin;
    std::vector<f32> thresholds = {0};
};

namespace detail {

// The C observer's user_data is the wrapper object itself: the wrapper releases
// the C observer in its destructor, after which the C layer never calls back
// (a release from inside the callback detaches it at once and frees it later).
class ObserverBase {
public:
    ObserverBase(const ObserverBase&) = delete;
    ObserverBase& operator=(const ObserverBase&) = delete;
    virtual ~ObserverBase() {
        if (raw_) kui_observer_release(raw_);
    }
    void disconnect() { detail::check(kui_observer_disconnect(raw_), "Observer::disconnect"); }
    KuiObserver* cHandle() const noexcept { return raw_; }

protected:
    ObserverBase() = default;
    void remember(Element* target) {
        if (target && !document_.lock()) document_ = target->documentState();
    }
    KuiObserver* raw_ = nullptr;
    std::weak_ptr<DocumentState> document_;
};

inline Rect toRect(const KuiRect& raw) { return Rect{raw.x, raw.y, raw.width, raw.height}; }

}  // namespace detail

class MutationObserver : public detail::ObserverBase {
public:
    using Callback = std::function<void(const std::vector<MutationRecord>&, MutationObserver*)>;

    static RefPtr<MutationObserver> create(Callback callback) {
        RefPtr<MutationObserver> observer(new MutationObserver(std::move(callback)));
        detail::check(kui_mutation_observer_create(&MutationObserver::trampoline, observer.get(), nullptr,
                                                   &observer->raw_),
                      "MutationObserver::create");
        return observer;
    }
    void observe(Node* target, const MutationObserverInit& options) {
        if (!target) return;
        std::vector<const char*> names;
        for (const auto& name : options.attributeFilter) names.push_back(name.c_str());
        KuiCMutationOptions raw{};
        raw.child_list = options.childList;
        raw.attributes = options.attributes;
        raw.character_data = options.characterData;
        raw.subtree = options.subtree;
        raw.attribute_old_value = options.attributeOldValue;
        raw.character_data_old_value = options.characterDataOldValue;
        raw.attribute_filter = names.empty() ? nullptr : names.data();
        raw.attribute_filter_count = names.size();
        detail::check(kui_mutation_observer_observe(raw_, target->cHandle(), &raw), "MutationObserver::observe");
        remember(target);
    }
    std::vector<MutationRecord> takeRecords() { return {}; }

private:
    explicit MutationObserver(Callback callback) : callback_(std::move(callback)) {}

    static void trampoline(const KuiCMutationRecord* records, std::size_t count, void* user_data) {
        auto* self = static_cast<MutationObserver*>(user_data);
        const auto state = self->document_.lock();
        std::vector<MutationRecord> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const KuiCMutationRecord& r = records[i];
            MutationRecord record;
            record.type = static_cast<MutationRecord::Type>(r.type);
            record.target = detail::shareElement(state, r.target);
            record.addedNode = detail::shareElement(state, r.added);
            record.removedNode = detail::shareElement(state, r.removed);
            record.previousSibling = detail::shareElement(state, r.previous_sibling);
            record.nextSibling = detail::shareElement(state, r.next_sibling);
            record.attributeName = detail::text(r.attribute_name, r.attribute_name_len);
            record.oldValue = detail::text(r.old_value, r.old_value_len);
            record.targetNodeType = r.target_node_type;
            record.addedNodeType = r.added_node_type;
            record.removedNodeType = r.removed_node_type;
            out.push_back(std::move(record));
        }
        Callback callback = self->callback_;  // the callback may destroy the observer
        try {
            if (callback) callback(out, self);
        } catch (...) {
        }
    }

    Callback callback_;
};

class ResizeObserver : public detail::ObserverBase {
public:
    using Callback = std::function<void(const std::vector<ResizeObserverEntry>&, ResizeObserver*)>;

    static RefPtr<ResizeObserver> create(Callback callback) {
        RefPtr<ResizeObserver> observer(new ResizeObserver(std::move(callback)));
        detail::check(kui_resize_observer_create(&ResizeObserver::trampoline, observer.get(), nullptr,
                                                 &observer->raw_),
                      "ResizeObserver::create");
        return observer;
    }
    void observe(Element* target) {
        if (!target) return;
        detail::check(kui_observer_observe(raw_, target->cHandle()), "ResizeObserver::observe");
        remember(target);
    }
    void unobserve(Element* target) {
        if (target) detail::check(kui_observer_unobserve(raw_, target->cHandle()), "ResizeObserver::unobserve");
    }

private:
    explicit ResizeObserver(Callback callback) : callback_(std::move(callback)) {}

    static void trampoline(const KuiCResizeEntry* entries, std::size_t count, void* user_data) {
        auto* self = static_cast<ResizeObserver*>(user_data);
        const auto state = self->document_.lock();
        std::vector<ResizeObserverEntry> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const KuiCResizeEntry& e = entries[i];
            ResizeObserverEntry entry;
            entry.target = detail::shareElement(state, e.target).get();
            entry.contentBoxSize = {e.content_width, e.content_height};
            entry.borderBoxSize = {e.border_width, e.border_height};
            entry.contentRectX = e.content_x;
            entry.contentRectY = e.content_y;
            entry.contentRectWidth = e.content_width;
            entry.contentRectHeight = e.content_height;
            out.push_back(entry);
        }
        Callback callback = self->callback_;
        try {
            if (callback) callback(out, self);
        } catch (...) {
        }
    }

    Callback callback_;
};

class IntersectionObserver : public detail::ObserverBase {
public:
    using Callback = std::function<void(const std::vector<IntersectionObserverEntry>&, IntersectionObserver*)>;

    static RefPtr<IntersectionObserver> create(Callback callback, const IntersectionObserverInit& init = {}) {
        RefPtr<IntersectionObserver> observer(new IntersectionObserver(std::move(callback), init));
        KuiCIntersectionOptions raw{};
        raw.root = init.root ? init.root->cHandle() : nullptr;
        raw.margin_top = init.rootMargin.top;
        raw.margin_right = init.rootMargin.right;
        raw.margin_bottom = init.rootMargin.bottom;
        raw.margin_left = init.rootMargin.left;
        raw.thresholds = init.thresholds.empty() ? nullptr : init.thresholds.data();
        raw.threshold_count = init.thresholds.size();
        detail::check(kui_intersection_observer_create(&raw, &IntersectionObserver::trampoline, observer.get(),
                                                       nullptr, &observer->raw_),
                      "IntersectionObserver::create");
        observer->remember(init.root);
        return observer;
    }
    void observe(Element* target) {
        if (!target) return;
        detail::check(kui_observer_observe(raw_, target->cHandle()), "IntersectionObserver::observe");
        remember(target);
    }
    void unobserve(Element* target) {
        if (target) detail::check(kui_observer_unobserve(raw_, target->cHandle()), "IntersectionObserver::unobserve");
    }
    const IntersectionObserverInit& options() const { return init_; }

private:
    IntersectionObserver(Callback callback, IntersectionObserverInit init)
        : callback_(std::move(callback)), init_(std::move(init)) {}

    static void trampoline(const KuiCIntersectionEntry* entries, std::size_t count, void* user_data) {
        auto* self = static_cast<IntersectionObserver*>(user_data);
        const auto state = self->document_.lock();
        std::vector<IntersectionObserverEntry> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const KuiCIntersectionEntry& e = entries[i];
            IntersectionObserverEntry entry;
            entry.target = detail::shareElement(state, e.target).get();
            entry.boundingClientRect = detail::toRect(e.bounding_client_rect);
            entry.intersectionRect = detail::toRect(e.intersection_rect);
            entry.rootBounds = detail::toRect(e.root_bounds);
            entry.intersectionRatio = e.intersection_ratio;
            entry.isIntersecting = e.is_intersecting != 0;
            out.push_back(entry);
        }
        Callback callback = self->callback_;
        try {
            if (callback) callback(out, self);
        } catch (...) {
        }
    }

    Callback callback_;
    IntersectionObserverInit init_;
};

// Source compatibility with the native registries: observe() already registers.
template <typename Observer>
class ObserverRegistry {
public:
    void registerObserver(Observer*) {}
    void unregisterObserver(Observer* observer) {
        if (observer) observer->disconnect();
    }
};
class MutationObserverRegistry : public ObserverRegistry<MutationObserver> {};
class ResizeObserverRegistry : public ObserverRegistry<ResizeObserver> {};
class IntersectionObserverRegistry : public ObserverRegistry<IntersectionObserver> {};

// --- Document accessors -----------------------------------------------------------
inline Router& Document::router() {
    if (!router_) router_ = std::make_shared<Router>(this);
    return *router_;
}
inline BindingManager& Document::bindingManager() {
    if (!bindings_) bindings_ = std::make_shared<BindingManager>(this);
    return *bindings_;
}
inline AnimationManager* Document::animationManager() {
    if (!animations_) animations_ = std::make_shared<AnimationManager>(this);
    return animations_.get();
}
inline TransitionManager* Document::transitionManager() {
    if (!transitions_) transitions_ = std::make_shared<TransitionManager>(this);
    return transitions_.get();
}
inline MutationObserverRegistry& Document::mutationObserverRegistry() {
    static MutationObserverRegistry registry;
    return registry;
}
inline ResizeObserverRegistry& Document::resizeObserverRegistry() {
    static ResizeObserverRegistry registry;
    return registry;
}
inline IntersectionObserverRegistry& Document::intersectionObserverRegistry() {
    static IntersectionObserverRegistry registry;
    return registry;
}

}  // namespace kui
KUI_CPP_HIDDEN_END
