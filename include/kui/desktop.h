// Desktop integration for the C++ Wrapper (C ABI section 10, 3.61.8): native
// menu bar, tray icon, keyboard shortcuts, toasts and screenshots, with the
// native SDK's type and member names. Included by full_profile.h.
//
// Differences from the native SDK, all because only the C ABI is visible here:
//   - Window::menuBar() (NativeMenuBar with native command ids) is not offered; give an item an id
//     (MenuItemDef::id) and change it with Window::setMenuItemChecked / setMenuItemEnabled, or rebuild
//     the menu with setMenuBar().
//   - ShortcutManager::tryDispatch() is not offered (keys arrive from the window).
//   - Renderer only exposes saveScreenshot().
#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

// --- Menus ---------------------------------------------------------------------
struct MenuItemDef {
    String text;
    String shortcutLabel;
    std::function<void()> onClick;
    bool isSeparator = false;
    bool disabled = false;
    bool checkable = false;
    bool checked = false;
    // Command id for Window::setMenuItemChecked / setMenuItemEnabled: non-zero and unique in the menu
    // bar; 0 picks one automatically (such an item cannot be changed later).
    u32 id = 0;
    std::vector<MenuItemDef> children;

    MenuItemDef() = default;
    MenuItemDef(const String& text_, std::function<void()> onClick_, const String& shortcut_ = "")
        : text(text_), shortcutLabel(shortcut_), onClick(std::move(onClick_)) {}
    static MenuItemDef separator() {
        MenuItemDef def;
        def.isSeparator = true;
        return def;
    }
};

struct MenuBarDef {
    struct TopLevelMenu {
        String text;
        std::vector<MenuItemDef> items;
    };
    std::vector<TopLevelMenu> menus;
    MenuBarDef& addMenu(const String& text, std::vector<MenuItemDef> items) {
        menus.push_back({text, std::move(items)});
        return *this;
    }
};

namespace detail {

// Converts menu definitions to the C structs, numbering leaf items 1..N and
// keeping their onClick callbacks; the C layer copies the structs, so the
// storage only needs to outlive the call.
struct MenuCallbacks {
    std::unordered_map<uint32_t, std::function<void()>> onClick;  // by item id

    static void activate(uint32_t id, void* userData) noexcept {
        auto* self = static_cast<MenuCallbacks*>(userData);
        const auto found = self->onClick.find(id);
        if (id == 0 || found == self->onClick.end()) return;
        try {
            if (found->second) found->second();
        } catch (...) {
        }
    }
};

struct MenuBuilder {
    std::vector<std::unique_ptr<std::vector<KuiCMenuItem>>> storage;
    std::unordered_map<uint32_t, std::function<void()>> onClick;
    uint32_t nextAutoId = 0x80000000u;  // ids of items without MenuItemDef::id

    const KuiCMenuItem* items(const std::vector<MenuItemDef>& defs) {
        auto list = std::make_unique<std::vector<KuiCMenuItem>>(defs.size());
        for (std::size_t index = 0; index < defs.size(); ++index) {
            const MenuItemDef& def = defs[index];
            KuiCMenuItem& item = (*list)[index];
            item = KuiCMenuItem{};
            item.separator = def.isSeparator ? 1 : 0;
            if (def.isSeparator) continue;
            item.text = def.text.c_str();
            item.text_len = def.text.size();
            item.shortcut_label = def.shortcutLabel.c_str();
            item.shortcut_label_len = def.shortcutLabel.size();
            item.disabled = def.disabled ? 1 : 0;
            item.checkable = def.checkable ? 1 : 0;
            item.checked = def.checked ? 1 : 0;
            if (!def.children.empty()) {
                item.children = items(def.children);
                item.child_count = def.children.size();
            } else {
                const uint32_t id = def.id != 0 ? def.id : nextAutoId++;
                onClick[id] = def.onClick;
                item.id = id;
            }
        }
        storage.push_back(std::move(list));
        return storage.back()->data();
    }
};

}  // namespace detail

// --- Tray icon -----------------------------------------------------------------
struct TrayIconConfig {
    String iconPath;
    String tooltip;
    bool visible = true;
};

class TrayIcon {
public:
    using ClickCallback = std::function<void()>;

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // window must be the window that owns this tray icon (native: window->trayIcon()).
    bool create(Window* window, const TrayIconConfig& cfg);
    void destroy() { detail::check(kui_window_tray_destroy(raw_), "TrayIcon::destroy"); }
    bool isActive() const { int v = 0; detail::check(kui_window_tray_is_active(raw_, &v), "TrayIcon::isActive"); return v != 0; }
    void setVisible(bool visible) { detail::check(kui_window_tray_set_visible(raw_, visible ? 1 : 0), "TrayIcon::setVisible"); }
    bool isVisible() const { int v = 0; detail::check(kui_window_tray_is_visible(raw_, &v), "TrayIcon::isVisible"); return v != 0; }
    void setTooltip(const String& tooltip) {
        tooltip_ = tooltip;
        detail::check(kui_window_tray_set_tooltip(raw_, tooltip.data(), tooltip.size()), "TrayIcon::setTooltip");
    }
    String tooltip() const { return tooltip_; }
    void setIcon(const String& iconPath) { detail::check(kui_window_tray_set_icon(raw_, iconPath.data(), iconPath.size()), "TrayIcon::setIcon"); }
    // Like the native API, only the items of the first menu are used.
    void setContextMenu(const MenuBarDef& def) {
        detail::MenuBuilder builder;
        static const std::vector<MenuItemDef> empty;
        const auto& items = def.menus.empty() ? empty : def.menus.front().items;
        const KuiCMenuItem* raw = builder.items(items);
        detail::check(kui_window_tray_set_context_menu(raw_, raw, items.size(), &detail::MenuCallbacks::activate, &menu_),
                      "TrayIcon::setContextMenu");
        menu_.onClick = std::move(builder.onClick);
        hasMenu_ = !items.empty();
    }
    bool hasContextMenu() const { return hasMenu_; }
    bool popUpContextMenu() { return kui_window_tray_popup_context_menu(raw_) == KUI_OK; }
    void showBalloon(const String& title, const String& message, int iconType = 1, int durationMs = 5000) {
        detail::check(kui_window_tray_show_balloon(raw_, title.data(), title.size(), message.data(), message.size(),
                                                   iconType, durationMs),
                      "TrayIcon::showBalloon");
    }
    void onClick(ClickCallback cb) { onClick_ = std::move(cb); }
    void onDoubleClick(ClickCallback cb) { onDoubleClick_ = std::move(cb); }
    void onRightClick(ClickCallback cb) { onRightClick_ = std::move(cb); }
    void onBalloonClick(ClickCallback cb) { onBalloonClick_ = std::move(cb); }
    void simulateClick() { kui_window_tray_simulate_event(raw_, KUI_C_TRAY_CLICK); }
    void simulateDoubleClick() { kui_window_tray_simulate_event(raw_, KUI_C_TRAY_DOUBLE_CLICK); }
    void simulateRightClick() { kui_window_tray_simulate_event(raw_, KUI_C_TRAY_RIGHT_CLICK); }
    void simulateBalloonClick() { kui_window_tray_simulate_event(raw_, KUI_C_TRAY_BALLOON_CLICK); }

private:
    friend class Window;
    friend struct detail::WindowDesktop;
    explicit TrayIcon(KuiWindow* raw) : raw_(raw) {}
    static void event(KuiCTrayEvent kind, void* userData) noexcept {
        auto* self = static_cast<TrayIcon*>(userData);
        const ClickCallback* cb = kind == KUI_C_TRAY_CLICK ? &self->onClick_
                                : kind == KUI_C_TRAY_DOUBLE_CLICK ? &self->onDoubleClick_
                                : kind == KUI_C_TRAY_RIGHT_CLICK ? &self->onRightClick_
                                : &self->onBalloonClick_;
        try {
            if (*cb) (*cb)();
        } catch (...) {
        }
    }
    KuiWindow* raw_ = nullptr;
    String tooltip_;
    bool hasMenu_ = false;
    detail::MenuCallbacks menu_;
    ClickCallback onClick_, onDoubleClick_, onRightClick_, onBalloonClick_;
};

// --- Shortcuts -----------------------------------------------------------------
enum class KeyModifier : u8 { None = 0, Ctrl = 1 << 0, Shift = 1 << 1, Alt = 1 << 2, Meta = 1 << 3 };
inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
    return static_cast<KeyModifier>(static_cast<u8>(a) | static_cast<u8>(b));
}
inline KeyModifier operator&(KeyModifier a, KeyModifier b) {
    return static_cast<KeyModifier>(static_cast<u8>(a) & static_cast<u8>(b));
}
inline bool hasModifier(KeyModifier flags, KeyModifier test) {
    return (static_cast<u8>(flags) & static_cast<u8>(test)) != 0;
}
struct Shortcut {
    String key;
    KeyModifier modifiers = KeyModifier::None;
    bool operator==(const Shortcut& o) const { return key == o.key && modifiers == o.modifiers; }
    bool operator!=(const Shortcut& o) const { return !(*this == o); }
};
using ShortcutCallback = std::function<void()>;
using ShortcutId = u32;

class ShortcutManager {
public:
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;

    // Returns 0 for a combination that cannot be parsed (like the native API).
    ShortcutId bind(const String& shortcutStr, ShortcutCallback callback) {
        if (!callback) return 0;
        auto holder = std::make_unique<ShortcutCallback>(std::move(callback));
        uint32_t id = 0;
        if (kui_window_bind_shortcut(raw_, shortcutStr.data(), shortcutStr.size(), &ShortcutManager::fire,
                                     holder.get(), &id) != KUI_OK)
            return 0;
        callbacks_[id] = std::move(holder);  // rebinding a combination keeps its id
        return id;
    }
    ShortcutId bind(const Shortcut& shortcut, ShortcutCallback callback) {
        return bind(format(shortcut), std::move(callback));
    }
    void unbind(ShortcutId id) {
        detail::check(kui_window_unbind_shortcut(raw_, id), "ShortcutManager::unbind");
        callbacks_.erase(id);
    }
    void unbindAll() {
        detail::check(kui_window_unbind_all_shortcuts(raw_), "ShortcutManager::unbindAll");
        callbacks_.clear();
    }
    void setEnabled(bool enabled) {
        enabled_ = enabled;
        detail::check(kui_window_set_shortcuts_enabled(raw_, enabled ? 1 : 0), "ShortcutManager::setEnabled");
    }
    bool isEnabled() const { return enabled_; }
    static String format(const Shortcut& shortcut) {
        String out;
        if (hasModifier(shortcut.modifiers, KeyModifier::Ctrl)) out += "Ctrl+";
        if (hasModifier(shortcut.modifiers, KeyModifier::Shift)) out += "Shift+";
        if (hasModifier(shortcut.modifiers, KeyModifier::Alt)) out += "Alt+";
        if (hasModifier(shortcut.modifiers, KeyModifier::Meta)) out += "Meta+";
        return out + shortcut.key;
    }

private:
    friend class Window;
    friend struct detail::WindowDesktop;
    explicit ShortcutManager(KuiWindow* raw) : raw_(raw) {}
    static void fire(void* userData) noexcept {
        try {
            (*static_cast<ShortcutCallback*>(userData))();
        } catch (...) {
        }
    }
    KuiWindow* raw_ = nullptr;
    bool enabled_ = true;
    std::unordered_map<ShortcutId, std::unique_ptr<ShortcutCallback>> callbacks_;
};

// --- Renderer (screenshot only) ------------------------------------------------
class Renderer {
public:
    bool saveScreenshot(const String& path) {
        int saved = 0;
        return kui_window_save_screenshot(raw_, path.data(), path.size(), &saved) == KUI_OK && saved != 0;
    }

private:
    friend class Window;
    friend struct detail::WindowDesktop;
    explicit Renderer(KuiWindow* raw) : raw_(raw) {}
    KuiWindow* raw_ = nullptr;
};

namespace detail {
struct WindowDesktop {
    explicit WindowDesktop(KuiWindow* raw) : tray(raw), shortcuts(raw), renderer(raw) {}
    TrayIcon tray;
    ShortcutManager shortcuts;
    Renderer renderer;
    MenuCallbacks menu;
};
}  // namespace detail

inline detail::WindowDesktop& Window::desktop() {
    if (!desktop_) desktop_ = std::make_shared<detail::WindowDesktop>(raw_);
    return *desktop_;
}
inline void Window::setMenuBar(const MenuBarDef& def) {
    detail::MenuBuilder builder;
    std::vector<KuiCMenu> menus(def.menus.size());
    for (std::size_t index = 0; index < def.menus.size(); ++index) {
        menus[index].text = def.menus[index].text.c_str();
        menus[index].text_len = def.menus[index].text.size();
        menus[index].items = builder.items(def.menus[index].items);
        menus[index].item_count = def.menus[index].items.size();
    }
    detail::MenuCallbacks& state = desktop().menu;
    detail::check(kui_window_set_menu_bar(raw_, menus.data(), menus.size(), &detail::MenuCallbacks::activate, &state),
                  "Window::setMenuBar");
    state.onClick = std::move(builder.onClick);
}
inline void Window::setMenuItemChecked(u32 id, bool checked) {
    detail::check(kui_window_menu_set_checked(raw_, id, checked ? 1 : 0), "Window::setMenuItemChecked");
}
inline void Window::setMenuItemEnabled(u32 id, bool enabled) {
    detail::check(kui_window_menu_set_enabled(raw_, id, enabled ? 1 : 0), "Window::setMenuItemEnabled");
}
inline void Window::clearMenuBar() {
    detail::check(kui_window_clear_menu_bar(raw_), "Window::clearMenuBar");
    desktop().menu.onClick.clear();
}
inline TrayIcon& Window::trayIcon() { return desktop().tray; }
inline ShortcutManager& Window::shortcutManager() { return desktop().shortcuts; }
inline Renderer* Window::renderer() { return &desktop().renderer; }

inline bool TrayIcon::create(Window* window, const TrayIconConfig& cfg) {
    if (!window || window->cHandle() != raw_) return false;
    if (kui_window_tray_create(raw_, cfg.iconPath.data(), cfg.iconPath.size(), cfg.tooltip.data(),
                               cfg.tooltip.size(), cfg.visible ? 1 : 0) != KUI_OK)
        return false;
    tooltip_ = cfg.tooltip;
    detail::check(kui_window_tray_set_event_callback(raw_, &TrayIcon::event, this), "TrayIcon::create");
    return true;
}

// --- Toasts --------------------------------------------------------------------
enum class ToastLevel : u8 { Info, Success, Warning, Error };
struct ToastConfig {
    String message;
    String title;
    ToastLevel level = ToastLevel::Info;
    int durationMs = 3000;
    String position = "bottom-right";
    bool closable = true;
    String icon;
};

class ToastManager {
public:
    ToastManager(const ToastManager&) = delete;
    ToastManager& operator=(const ToastManager&) = delete;

    u32 show(const ToastConfig& cfg) {
        if (cfg.message.empty()) return 0;
        KuiCToastOptions options{};
        options.message = cfg.message.data();
        options.message_len = cfg.message.size();
        options.title = cfg.title.data();
        options.title_len = cfg.title.size();
        options.level = static_cast<KuiCToastLevel>(cfg.level);
        options.duration_ms = cfg.durationMs;
        options.position = cfg.position.data();
        options.position_len = cfg.position.size();
        options.closable = cfg.closable ? 1 : 0;
        options.icon = cfg.icon.empty() ? nullptr : cfg.icon.data();
        options.icon_len = cfg.icon.size();
        uint32_t id = 0;
        detail::check(kui_document_show_toast(raw_, &options, &id), "ToastManager::show");
        return id;
    }
    void dismiss(u32 toastId) { detail::check(kui_document_dismiss_toast(raw_, toastId), "ToastManager::dismiss"); }
    void dismissAll() { detail::check(kui_document_dismiss_all_toasts(raw_), "ToastManager::dismissAll"); }
    int count() const { int value = 0; detail::check(kui_document_get_toast_count(raw_, &value), "ToastManager::count"); return value; }
    u32 info(const String& msg, int durationMs = 3000) { return quick(msg, ToastLevel::Info, durationMs); }
    u32 success(const String& msg, int durationMs = 3000) { return quick(msg, ToastLevel::Success, durationMs); }
    u32 warning(const String& msg, int durationMs = 4000) { return quick(msg, ToastLevel::Warning, durationMs); }
    u32 error(const String& msg, int durationMs = 5000) { return quick(msg, ToastLevel::Error, durationMs); }

private:
    friend class Document;
    explicit ToastManager(KuiDocument* raw) : raw_(raw) {}
    u32 quick(const String& msg, ToastLevel level, int durationMs) {
        ToastConfig cfg;
        cfg.message = msg;
        cfg.level = level;
        cfg.durationMs = durationMs;
        return show(cfg);
    }
    KuiDocument* raw_ = nullptr;
};

inline ToastManager& Document::toastManager() {
    if (!toasts_) toasts_ = std::shared_ptr<ToastManager>(new ToastManager(state_->raw));
    return *toasts_;
}

}  // namespace kui
KUI_CPP_HIDDEN_END
