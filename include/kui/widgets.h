// Popups, the <input> picker and game audio for the C++ Wrapper (C ABI section
// 12, 3.61.8), with the native SDK's type and member names. Included by
// full_profile.h.
//
// Differences from the native SDK, all because only the C ABI is visible here:
//   - PopupManager has no showCustomPopup() (it only draws an empty rectangle),
//     no hit-testing / painting hooks and no tooltip-target plumbing; those are
//     driven by the document itself.
//   - InputPicker has no popupElement(), ensureUaStylesheet() or
//     resetForNewDocument(); colorHex() is computed from the HSV state.
//   - AudioManager has no setAdapter() / tick(): the document owns both.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

// --- Popups -----------------------------------------------------------------------
enum class PopupType : u8 { Tooltip, ContextMenu, Custom };
enum class PopupMode : u8 { Overlay, NativeWindow };
enum class PopupPosition : u8 { Top, Bottom, Left, Right };
using PopupId = u32;

struct TooltipTheme {
    Color bg = Color(40, 40, 40, 230);
    Color fg = Color(240, 240, 240, 255);
    Color border = Color(60, 60, 60, 200);
    f32 radius = 3.0f;
};

struct MenuItem {
    String text;
    std::function<void()> onClick;
    bool disabled = false;
    bool separator = false;
};

namespace detail {

// One context menu: item index -> callback. Freed through release_fn once the
// menu is gone.
struct PopupMenuBox {
    std::vector<std::function<void()>> callbacks;
};
inline void popupMenuTrampoline(uint32_t id, void* user_data) {
    auto* box = static_cast<PopupMenuBox*>(user_data);
    if (id >= box->callbacks.size() || !box->callbacks[id]) return;
    std::function<void()> callback = box->callbacks[id];  // the callback may close the menu
    try {
        callback();
    } catch (...) {
    }
}

}  // namespace detail

class PopupManager {
public:
    explicit PopupManager(Document* document) : document_(document) {}
    PopupManager(const PopupManager&) = delete;
    PopupManager& operator=(const PopupManager&) = delete;

    PopupId showTooltip(const String& text, const Rect& anchorRect, PopupPosition pos = PopupPosition::Top) {
        KuiRect anchor{anchorRect.x, anchorRect.y, anchorRect.width, anchorRect.height};
        u32 id = 0;
        detail::check(kui_document_show_tooltip(raw(), text.data(), text.size(), &anchor,
                                                static_cast<KuiCPopupPosition>(pos), &id),
                      "PopupManager::showTooltip");
        return id;
    }
    PopupId showContextMenu(f32 x, f32 y, std::vector<MenuItem> items) {
        if (items.empty()) return 0;
        auto* box = new detail::PopupMenuBox;
        std::vector<KuiCMenuItem> raw_items(items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            raw_items[i].text = items[i].text.c_str();
            raw_items[i].text_len = items[i].text.size();
            raw_items[i].id = static_cast<uint32_t>(i);
            raw_items[i].separator = items[i].separator;
            raw_items[i].disabled = items[i].disabled;
            box->callbacks.push_back(std::move(items[i].onClick));
        }
        u32 id = 0;
        if (kui_document_show_context_menu(raw(), x, y, raw_items.data(), raw_items.size(),
                                           detail::popupMenuTrampoline, box,
                                           detail::deleteCallback<detail::PopupMenuBox>, &id) != KUI_OK) {
            delete box;
            return 0;
        }
        return id;
    }
    void dismiss(PopupId id) { detail::check(kui_document_dismiss_popup(raw(), id), "PopupManager::dismiss"); }
    void dismissAll() { detail::check(kui_document_dismiss_all_popups(raw()), "PopupManager::dismissAll"); }
    void dismissByType(PopupType type) {
        detail::check(kui_document_dismiss_popups_of_type(raw(), static_cast<KuiCPopupType>(type)),
                      "PopupManager::dismissByType");
    }
    bool hasActivePopups() const {
        int any = 0;
        detail::check(kui_document_has_popups(raw(), &any), "PopupManager::hasActivePopups");
        return any != 0;
    }
    bool hasPopupOfType(PopupType type) const {
        int has = 0;
        detail::check(kui_document_has_popup_of_type(raw(), static_cast<KuiCPopupType>(type), &has),
                      "PopupManager::hasPopupOfType");
        return has != 0;
    }
    void setTooltipDelay(f64 ms) { detail::check(kui_document_set_tooltip_delay(raw(), ms), "PopupManager::setTooltipDelay"); }
    f64 tooltipDelay() const {
        double ms = 0;
        detail::check(kui_document_get_tooltip_delay(raw(), &ms), "PopupManager::tooltipDelay");
        return ms;
    }
    void setTooltipTheme(const TooltipTheme& theme) {
        KuiCTooltipTheme raw_theme{detail::packColor(theme.bg), detail::packColor(theme.fg),
                                   detail::packColor(theme.border), theme.radius};
        detail::check(kui_document_set_tooltip_theme(raw(), &raw_theme), "PopupManager::setTooltipTheme");
    }
    const TooltipTheme& tooltipTheme() const {
        KuiCTooltipTheme raw_theme{};
        detail::check(kui_document_get_tooltip_theme(raw(), &raw_theme), "PopupManager::tooltipTheme");
        theme_.bg = detail::unpackColor(raw_theme.background);
        theme_.fg = detail::unpackColor(raw_theme.foreground);
        theme_.border = detail::unpackColor(raw_theme.border);
        theme_.radius = raw_theme.radius;
        return theme_;
    }
    void setPopupMode(PopupMode mode) {
        detail::check(kui_document_set_popup_mode(raw(), static_cast<KuiCPopupMode>(mode)), "PopupManager::setPopupMode");
        mode_ = mode;
    }
    PopupMode popupMode() const { return mode_; }

private:
    KuiDocument* raw() const { return document_->cHandle(); }
    Document* document_;
    PopupMode mode_ = PopupMode::Overlay;
    mutable TooltipTheme theme_;
};

// --- Event dispatcher (the part exposed through the C ABI) ---------------------------
// 原生 EventDispatcher 的输入派发由窗口驱动，Wrapper 只提供经 C ABI 暴露的设置：内置默认右键菜单的开关。
class EventDispatcher {
public:
    explicit EventDispatcher(Document* document) : document_(document) {}
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // 内置默认右键菜单默认关闭；开启后右键 / 菜单键在 contextmenu 没被取消时弹出。页面也可写 <html context-menu>。
    void setDefaultContextMenuEnabled(bool enabled) {
        detail::check(kui_document_set_default_context_menu_enabled(raw(), enabled ? 1 : 0),
                      "EventDispatcher::setDefaultContextMenuEnabled");
    }
    bool isDefaultContextMenuEnabled() const {
        int enabled = 0;
        detail::check(kui_document_get_default_context_menu_enabled(raw(), &enabled),
                      "EventDispatcher::isDefaultContextMenuEnabled");
        return enabled != 0;
    }

private:
    KuiDocument* raw() const { return document_->cHandle(); }
    Document* document_;
};

// --- Input picker -------------------------------------------------------------------
enum class PickerMode : u8 { None, Date, Time, DateTimeLocal, Color };

class InputPicker {
public:
    explicit InputPicker(Document* document) : document_(document) {}
    InputPicker(const InputPicker&) = delete;
    InputPicker& operator=(const InputPicker&) = delete;

    // Each returns false when no picker opened (no panel for the input's type, or the picker is disabled).
    bool open(HTMLInputElement* input) { return openAs(input, KUI_C_PICKER_NONE); }
    bool openDate(HTMLInputElement* input) { return openAs(input, KUI_C_PICKER_DATE); }
    bool openTime(HTMLInputElement* input) { return openAs(input, KUI_C_PICKER_TIME); }
    bool openDateTimeLocal(HTMLInputElement* input) { return openAs(input, KUI_C_PICKER_DATETIME_LOCAL); }
    bool openColor(HTMLInputElement* input) { return openAs(input, KUI_C_PICKER_COLOR); }
    void close() { detail::check(kui_document_close_input_picker(raw()), "InputPicker::close"); }
    void commit() { detail::check(kui_document_commit_input_picker(raw()), "InputPicker::commit"); }

    bool isOpen() const { return state().open != 0; }
    PickerMode mode() const { return static_cast<PickerMode>(state().mode); }
    HTMLInputElement* targetInput() const {
        KuiElement* handle = nullptr;
        if (kui_document_get_input_picker_target(raw(), &handle) != KUI_OK) return nullptr;
        return document_->wrapperState()->wrap(handle).get();
    }
    void setEnabled(bool enabled) {
        detail::check(kui_document_set_input_picker_enabled(raw(), enabled ? 1 : 0), "InputPicker::setEnabled");
    }
    bool isEnabled() const { return state().enabled != 0; }

    i32 visibleYear() const { return state().visible_year; }
    i32 visibleMonth() const { return state().visible_month; }
    void setVisibleYearMonth(i32 year, i32 month) {
        detail::check(kui_document_set_input_picker_month(raw(), year, month), "InputPicker::setVisibleYearMonth");
    }

    f32 colorH() const { return state().color_h; }
    f32 colorS() const { return state().color_s; }
    f32 colorV() const { return state().color_v; }
    String colorHex() const {
        const KuiCInputPickerState s = state();
        const f32 c = s.color_v * s.color_s;
        const f32 hp = std::fmod(s.color_h / 60.0f, 6.0f);
        const f32 x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
        f32 r = 0, g = 0, b = 0;
        if (hp < 1) { r = c; g = x; }
        else if (hp < 2) { r = x; g = c; }
        else if (hp < 3) { g = c; b = x; }
        else if (hp < 4) { g = x; b = c; }
        else if (hp < 5) { r = x; b = c; }
        else { r = c; b = x; }
        const f32 m = s.color_v - c;
        char buf[8];
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", static_cast<int>(std::lround((r + m) * 255)),
                      static_cast<int>(std::lround((g + m) * 255)), static_cast<int>(std::lround((b + m) * 255)));
        return buf;
    }
    void setColorHsv(f32 h, f32 s, f32 v) {
        detail::check(kui_document_set_input_picker_color_hsv(raw(), h, s, v), "InputPicker::setColorHsv");
    }
    i32 timeHour() const { return state().hour; }
    i32 timeMinute() const { return state().minute; }
    void setTime(i32 hour, i32 minute) {
        detail::check(kui_document_set_input_picker_time(raw(), hour, minute), "InputPicker::setTime");
    }

private:
    KuiDocument* raw() const { return document_->cHandle(); }
    bool openAs(HTMLInputElement* input, KuiCPickerMode mode) {
        // Like the native API: a type without a panel (or a disabled picker) is a no-op.
        return input && kui_element_open_input_picker(input->cHandle(), mode) == KUI_OK;
    }
    KuiCInputPickerState state() const {
        KuiCInputPickerState s{};
        detail::check(kui_document_get_input_picker_state(raw(), &s), "InputPicker::state");
        return s;
    }
    Document* document_;
};

// --- Game audio ------------------------------------------------------------------
enum class AudioGroup : u8 { Master = 0, Music = 1, SFX = 2, UI = 3, _Count = 4 };
using SoundHandle = u32;
static constexpr SoundHandle kInvalidSoundHandle = 0;

struct PlayOptions {
    AudioGroup group = AudioGroup::SFX;
    f32 volume = 1.0f;
    f32 pan = 0.0f;
    f32 pitch = 1.0f;
    bool loop = false;
    bool preloadOnly = false;
};

struct SoundInfo {
    SoundHandle handle = kInvalidSoundHandle;
    String path;
    AudioGroup group = AudioGroup::SFX;
    f32 volume = 1.0f;
    f32 pan = 0.0f;
    f32 pitch = 1.0f;
    bool playing = false;
    bool looping = false;
    f64 currentTime = 0.0;
    f64 duration = 0.0;
};

class AudioManager {
public:
    explicit AudioManager(Document* document) : document_(document) {}
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool isAvailable() const {
        int available = 0;
        detail::check(kui_document_audio_is_available(raw(), &available), "AudioManager::isAvailable");
        return available != 0;
    }
    void setGroupVolume(AudioGroup group, f32 volume) { setGroup(group, volume, isGroupMuted(group)); }
    f32 groupVolume(AudioGroup group) const { return groupState(group).first; }
    void setMasterVolume(f32 volume) { setGroupVolume(AudioGroup::Master, volume); }
    f32 masterVolume() const { return groupVolume(AudioGroup::Master); }
    void setGroupMuted(AudioGroup group, bool muted) { setGroup(group, groupVolume(group), muted); }
    bool isGroupMuted(AudioGroup group) const { return groupState(group).second; }

    // Returns kInvalidSoundHandle when audio is unavailable or the file cannot play.
    SoundHandle play(const String& path, const PlayOptions& opts = {}) {
        KuiCAudioPlayOptions raw_opts{static_cast<KuiCAudioGroup>(opts.group), opts.volume, opts.pan,
                                      opts.pitch, opts.loop ? 1 : 0, opts.preloadOnly ? 1 : 0};
        u32 handle = 0;
        (void)kui_document_audio_play(raw(), path.data(), path.size(), &raw_opts, &handle);
        if (handle) paths_[handle] = path;
        return handle;
    }
    void stop(SoundHandle handle) { detail::check(kui_document_audio_stop(raw(), handle), "AudioManager::stop"); }
    void pause(SoundHandle handle) { detail::check(kui_document_audio_set_paused(raw(), handle, 1), "AudioManager::pause"); }
    void resume(SoundHandle handle) { detail::check(kui_document_audio_set_paused(raw(), handle, 0), "AudioManager::resume"); }
    void setVolume(SoundHandle handle, f32 volume) { detail::check(kui_document_audio_set_volume(raw(), handle, volume), "AudioManager::setVolume"); }
    void setPan(SoundHandle handle, f32 pan) { detail::check(kui_document_audio_set_pan(raw(), handle, pan), "AudioManager::setPan"); }
    void setPitch(SoundHandle handle, f32 pitch) { detail::check(kui_document_audio_set_pitch(raw(), handle, pitch), "AudioManager::setPitch"); }
    void seek(SoundHandle handle, f64 seconds) { detail::check(kui_document_audio_seek(raw(), handle, seconds), "AudioManager::seek"); }
    SoundInfo info(SoundHandle handle) const {
        SoundInfo out;
        KuiCSoundInfo raw_info{};
        if (kui_document_audio_get_info(raw(), handle, &raw_info) != KUI_OK) return out;
        out.handle = handle;
        const auto path = paths_.find(handle);
        if (path != paths_.end()) out.path = path->second;
        out.group = static_cast<AudioGroup>(raw_info.group);
        out.volume = raw_info.volume;
        out.pan = raw_info.pan;
        out.pitch = raw_info.pitch;
        out.playing = raw_info.playing != 0;
        out.looping = raw_info.looping != 0;
        out.currentTime = raw_info.current_time;
        out.duration = raw_info.duration;
        return out;
    }
    bool isPlaying(SoundHandle handle) const { return info(handle).playing; }
    void stopAll() { detail::check(kui_document_audio_stop_all(raw(), -1), "AudioManager::stopAll"); }
    void stopAll(AudioGroup group) { detail::check(kui_document_audio_stop_all(raw(), static_cast<int>(group)), "AudioManager::stopAll"); }
    void pauseAll() { detail::check(kui_document_audio_set_all_paused(raw(), -1, 1), "AudioManager::pauseAll"); }
    void pauseAll(AudioGroup group) { detail::check(kui_document_audio_set_all_paused(raw(), static_cast<int>(group), 1), "AudioManager::pauseAll"); }
    void resumeAll() { detail::check(kui_document_audio_set_all_paused(raw(), -1, 0), "AudioManager::resumeAll"); }
    void resumeAll(AudioGroup group) { detail::check(kui_document_audio_set_all_paused(raw(), static_cast<int>(group), 0), "AudioManager::resumeAll"); }
    u32 preload(const String& path, u32 count = 1) {
        u32 loaded = 0;
        (void)kui_document_audio_preload(raw(), path.data(), path.size(), count, &loaded);
        return loaded;
    }
    void unload(const String& path) { detail::check(kui_document_audio_unload(raw(), path.data(), path.size()), "AudioManager::unload"); }
    void unloadAll() { detail::check(kui_document_audio_unload(raw(), nullptr, 0), "AudioManager::unloadAll"); }
    u32 activeCount() const { return counts(-1).first; }
    u32 activeCount(AudioGroup group) const { return counts(static_cast<int>(group)).first; }
    u32 pooledCount() const { return counts(-1).second; }

private:
    KuiDocument* raw() const { return document_->cHandle(); }
    void setGroup(AudioGroup group, f32 volume, bool muted) {
        detail::check(kui_document_audio_set_group(raw(), static_cast<KuiCAudioGroup>(group), volume, muted ? 1 : 0),
                      "AudioManager::setGroup");
    }
    std::pair<f32, bool> groupState(AudioGroup group) const {
        float volume = 0;
        int muted = 0;
        detail::check(kui_document_audio_get_group(raw(), static_cast<KuiCAudioGroup>(group), &volume, &muted),
                      "AudioManager::group");
        return {volume, muted != 0};
    }
    std::pair<u32, u32> counts(int group) const {
        uint32_t active = 0, pooled = 0;
        detail::check(kui_document_audio_get_counts(raw(), group, &active, &pooled), "AudioManager::counts");
        return {active, pooled};
    }
    Document* document_;
    std::unordered_map<SoundHandle, String> paths_;
};

// --- Document accessors -------------------------------------------------------------
inline PopupManager& Document::popupManager() {
    if (!popups_) popups_ = std::make_shared<PopupManager>(this);
    return *popups_;
}
inline InputPicker& Document::inputPicker() {
    if (!picker_) picker_ = std::make_shared<InputPicker>(this);
    return *picker_;
}
inline AudioManager& Document::audioManager() {
    if (!audio_) audio_ = std::make_shared<AudioManager>(this);
    return *audio_;
}
inline EventDispatcher* Document::eventDispatcher() {
    if (!events_) events_ = std::make_shared<EventDispatcher>(this);
    return events_.get();
}

}  // namespace kui
KUI_CPP_HIDDEN_END
