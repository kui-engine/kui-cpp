#pragma once

// Included by <kui/kui.h> after the base facade definitions.
// Do not include this file directly.

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <variant>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

using HTMLInputElement = Element;
using HTMLMediaElement = Element;
using HTMLLottieElement = Element;

struct PDFOptions {
    String title;
    String author;
    String subject;
    String keywords;
    String creator;
    String creationDate;
    f32 pageWidth_pt = 612.0f;
    f32 pageHeight_pt = 792.0f;
    bool rasterizePDFFonts = false;
    bool generatePDFOutline = true;
};

enum class ExtensionValueType : std::uint8_t { Nil, Integer, Number, String, Boolean };

struct ExtensionValue {
    ExtensionValueType type = ExtensionValueType::Nil;
    std::int64_t intValue = 0;
    double floatValue = 0;
    String stringValue;
    bool boolValue = false;

    static ExtensionValue nullValue() { return {}; }
    static ExtensionValue nil() { return {}; }
    static ExtensionValue integer(std::int64_t value) {
        ExtensionValue result; result.type = ExtensionValueType::Integer;
        result.intValue = value; return result;
    }
    static ExtensionValue number(double value) {
        ExtensionValue result; result.type = ExtensionValueType::Number;
        result.floatValue = value; return result;
    }
    static ExtensionValue string(String value) {
        ExtensionValue result; result.type = ExtensionValueType::String;
        result.stringValue = std::move(value); return result;
    }
    static ExtensionValue boolean(bool value) {
        ExtensionValue result; result.type = ExtensionValueType::Boolean;
        result.boolValue = value; return result;
    }
};

enum class ExtensionInvokeStatus {
    Success = 0, NullElement, WrongDocument, MethodNotFound,
    InvalidArgumentCount, InvalidResultCount, CallbackException
};

struct ExtensionInvokeResult {
    ExtensionInvokeStatus status = ExtensionInvokeStatus::Success;
    std::vector<ExtensionValue> values;
    String error;
    explicit operator bool() const noexcept { return status == ExtensionInvokeStatus::Success; }
};

class ExtensionManager {
public:
    struct PerfStats {
        double lastTickMs = 0, lastPaintMs = 0, maxTickMs = 0, maxPaintMs = 0;
        u64 tickCount = 0, paintCount = 0;
        double totalTickMs = 0, totalPaintMs = 0;
    };

    explicit ExtensionManager(std::shared_ptr<detail::DocumentState> state)
        : state_(std::move(state)) {}

    // Section 14 (3.61.8): single-extension lifecycle and hot reload.
    bool loadExtension(const String& dllPath) {
        int loaded = 0;
        detail::check(kui_document_load_extension(state_->raw, dllPath.data(), dllPath.size(), &loaded),
                      "ExtensionManager::loadExtension");
        return loaded != 0;
    }
    bool unloadExtension(const String& tagName) {
        const KuiStatus status = kui_document_unload_extension(state_->raw, tagName.data(), tagName.size());
        if (status == KUI_ERR_NOT_FOUND) return false;
        detail::check(status, "ExtensionManager::unloadExtension");
        return true;
    }
    bool reloadExtension(const String& dllPath) {
        int reloaded = 0;
        detail::check(kui_document_reload_extension(state_->raw, dllPath.data(), dllPath.size(), &reloaded),
                      "ExtensionManager::reloadExtension");
        return reloaded != 0;
    }
    void unloadAll() { detail::check(kui_document_unload_all_extensions(state_->raw), "ExtensionManager::unloadAll"); }
    std::size_t count() const {
        std::size_t value = 0;
        detail::check(kui_document_get_extension_state(state_->raw, &value, nullptr), "ExtensionManager::count");
        return value;
    }
    void startWatching(const String& dirPath) {
        detail::check(kui_document_watch_extensions(state_->raw, dirPath.data(), dirPath.size()),
                      "ExtensionManager::startWatching");
    }
    void stopWatching() { detail::check(kui_document_watch_extensions(state_->raw, nullptr, 0), "ExtensionManager::stopWatching"); }
    bool isWatching() const {
        int watching = 0;
        detail::check(kui_document_get_extension_state(state_->raw, nullptr, &watching), "ExtensionManager::isWatching");
        return watching != 0;
    }

    bool isExtensionLoaded(const String& tagName) const {
        int loaded = 0;
        detail::check(kui_document_extension_is_loaded(
                          state_->raw, tagName.data(), tagName.size(), &loaded),
                      "ExtensionManager::isExtensionLoaded");
        return loaded != 0;
    }

    ExtensionInvokeResult invoke(Element* target, const String& method,
                                 const std::vector<ExtensionValue>& args = {},
                                 std::size_t maxResults = 8) const {
        ExtensionInvokeResult result;
        if (!target) {
            result.status = ExtensionInvokeStatus::NullElement;
            result.error = "null element";
            return result;
        }
        std::vector<KuiCExtensionValue> rawArgs;
        rawArgs.reserve(args.size());
        for (const auto& value : args) {
            KuiCExtensionValue raw{};
            switch (value.type) {
            case ExtensionValueType::Integer: raw.type = KUI_C_EXT_INT; break;
            case ExtensionValueType::Number: raw.type = KUI_C_EXT_FLOAT; break;
            case ExtensionValueType::String: raw.type = KUI_C_EXT_STRING; break;
            case ExtensionValueType::Boolean: raw.type = KUI_C_EXT_BOOL; break;
            default: raw.type = KUI_C_EXT_NIL; break;
            }
            raw.int_value = value.intValue;
            raw.float_value = value.floatValue;
            raw.string_value = value.stringValue.data();
            raw.string_len = value.stringValue.size();
            raw.bool_value = value.boolValue ? 1 : 0;
            rawArgs.push_back(raw);
        }
        KuiExtensionResult* rawResult = nullptr;
        detail::check(kui_document_extension_invoke(
                          state_->raw, target->cHandle(), method.data(), method.size(),
                          rawArgs.data(), rawArgs.size(), maxResults, &rawResult),
                      "ExtensionManager::invoke");
        struct Releaser {
            void operator()(KuiExtensionResult* value) const {
                if (value) kui_extension_result_release(value);
            }
        };
        std::unique_ptr<KuiExtensionResult, Releaser> owner(rawResult);
        int status = 0;
        detail::check(kui_extension_result_get_status(rawResult, &status),
                      "ExtensionManager::invoke(status)");
        result.status = static_cast<ExtensionInvokeStatus>(status);
        result.error = detail::readString(
            [rawResult](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_extension_result_get_error(rawResult, buffer, capacity, size);
            }, "ExtensionManager::invoke(error)");
        std::size_t count = 0;
        detail::check(kui_extension_result_get_count(rawResult, &count),
                      "ExtensionManager::invoke(count)");
        result.values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            KuiCExtensionValue raw{};
            detail::check(kui_extension_result_get_value(rawResult, index, &raw),
                          "ExtensionManager::invoke(value)");
            ExtensionValue value;
            value.type = static_cast<ExtensionValueType>(raw.type);
            value.intValue = raw.int_value;
            value.floatValue = raw.float_value;
            value.boolValue = raw.bool_value != 0;
            if (raw.type == KUI_C_EXT_STRING) {
                value.stringValue = detail::readString(
                    [rawResult, index](char* buffer, std::size_t capacity,
                                       std::size_t* size) {
                        return kui_extension_result_get_string(rawResult, index,
                                                               buffer, capacity, size);
                    }, "ExtensionManager::invoke(string)");
            }
            result.values.push_back(std::move(value));
        }
        return result;
    }

    PerfStats getPerfStats(const String& tagName) const {
        KuiCExtensionPerfStats raw{};
        detail::check(kui_document_extension_get_perf_stats(
                          state_->raw, tagName.data(), tagName.size(), &raw),
                      "ExtensionManager::getPerfStats");
        return {raw.last_tick_ms, raw.last_paint_ms, raw.max_tick_ms,
                raw.max_paint_ms, raw.tick_count, raw.paint_count,
                raw.total_tick_ms, raw.total_paint_ms};
    }

    void resetPerfStats() {
        detail::check(kui_document_extension_reset_perf_stats(state_->raw),
                      "ExtensionManager::resetPerfStats");
    }

private:
    std::shared_ptr<detail::DocumentState> state_;
};

// The drawing commands a custom element's paint callback can use: the native DisplayList methods
// below, with the same signatures. Valid only during the callback. Arguments the C ABI does not
// carry (text layout options, nearest-neighbour sampling) throw Error(KUI_ERR_NOT_SUPPORTED).
class DisplayList {
public:
    explicit DisplayList(KuiPaintContext* paint) noexcept : paint_(paint) {}

    void save() { detail::check(kui_paint_save(paint_), "DisplayList::save"); }
    void restore() { detail::check(kui_paint_restore(paint_), "DisplayList::restore"); }
    void clipRect(const Rect& rect, bool antiAlias = true) {
        (void)antiAlias;
        detail::check(kui_paint_clip_rect(paint_, rect.x, rect.y, rect.width, rect.height),
                      "DisplayList::clipRect");
    }
    void clipRoundedRect(const Rect& rect, const CornerRadii& radii, bool antiAlias = true) {
        (void)antiAlias;
        const auto c = toC(radii);
        detail::check(kui_paint_clip_rounded_rect(paint_, rect.x, rect.y, rect.width, rect.height, &c),
                      "DisplayList::clipRoundedRect");
    }
    void translate(f32 dx, f32 dy) { detail::check(kui_paint_translate(paint_, dx, dy), "DisplayList::translate"); }
    void scale(f32 sx, f32 sy) { detail::check(kui_paint_scale(paint_, sx, sy), "DisplayList::scale"); }
    void rotate(f32 degrees) { detail::check(kui_paint_rotate(paint_, degrees), "DisplayList::rotate"); }
    void setOpacity(f32 opacity) {
        detail::check(kui_paint_set_opacity(paint_, opacity), "DisplayList::setOpacity");
    }

    void fillRect(const Rect& rect, const Color& color) {
        detail::check(kui_paint_fill_rect(paint_, rect.x, rect.y, rect.width, rect.height,
                                          detail::packColor(color)),
                      "DisplayList::fillRect");
    }
    // The last two arguments tune how KUI's own painter clamps radii; accepted and ignored.
    void fillRoundedRect(const Rect& rect, const CornerRadii& radii, const Color& color,
                         bool preserveCornerRadii = false, bool canonicalizeFullSingleCorner = false) {
        (void)preserveCornerRadii;
        (void)canonicalizeFullSingleCorner;
        const auto c = toC(radii);
        detail::check(kui_paint_fill_rounded_rect(paint_, rect.x, rect.y, rect.width, rect.height, &c,
                                                  detail::packColor(color)),
                      "DisplayList::fillRoundedRect");
    }
    void strokeRect(const Rect& rect, const Color& color, f32 width) {
        detail::check(kui_paint_stroke_rect(paint_, rect.x, rect.y, rect.width, rect.height, width,
                                            detail::packColor(color)),
                      "DisplayList::strokeRect");
    }
    void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, const Color& color, f32 width) {
        const auto c = toC(radii);
        detail::check(kui_paint_stroke_rounded_rect(paint_, rect.x, rect.y, rect.width, rect.height, &c,
                                                    width, detail::packColor(color)),
                      "DisplayList::strokeRoundedRect");
    }
    // pos is the top-left of the text's line box (not the baseline): the glyphs hang below pos.y and
    // the baseline lies at pos.y + ascent.
    void drawText(const String& text, const Point& pos, const String& fontFamily, f32 fontSize,
                  u16 fontWeight, u8 fontStyle, const Color& color, f32 maxWidth = 0,
                  bool ellipsis = false, u8 textAlign = 0, f32 lineHeightMult = 0,
                  f32 letterSpacing = 0, f32 wordSpacing = 0, f32 textIndent = 0,
                  bool breakLongWords = false) {
        if (maxWidth != 0 || ellipsis || textAlign != 0 || lineHeightMult != 0 || textIndent != 0 ||
            breakLongWords)
            throw Error(KUI_ERR_NOT_SUPPORTED, "DisplayList::drawText (layout options)");
        KuiCTextStyle style{};
        style.font_family = fontFamily.c_str();
        style.font_size = fontSize;
        style.font_weight = fontWeight;
        style.italic = fontStyle ? 1 : 0;
        style.letter_spacing = letterSpacing;
        style.word_spacing = wordSpacing;
        detail::check(kui_paint_draw_text(paint_, text.data(), text.size(), pos.x, pos.y, &style,
                                          detail::packColor(color)),
                      "DisplayList::drawText");
    }
    void drawImage(const String& path, const Rect& src, const Rect& dest) {
        const KuiRect from{src.x, src.y, src.width, src.height};
        const KuiRect to{dest.x, dest.y, dest.width, dest.height};
        detail::check(kui_paint_draw_image(paint_, path.data(), path.size(), &from, &to),
                      "DisplayList::drawImage");
    }
    void drawImage(const String& path, const Rect& src, const Rect& dest, bool nearestNeighbor) {
        if (nearestNeighbor) throw Error(KUI_ERR_NOT_SUPPORTED, "DisplayList::drawImage (nearestNeighbor)");
        drawImage(path, src, dest);
    }
    // pixels: premultiplied 0xAARRGGBB words.
    void drawPixelBuffer(const u32* pixels, u32 width, u32 height, const Rect& dest) {
        detail::check(kui_paint_draw_pixels(paint_, pixels, width, height, dest.x, dest.y, dest.width,
                                            dest.height),
                      "DisplayList::drawPixelBuffer");
    }
    // style: 0 solid, 1 double, 2 dotted, 3 dashed, 4 wavy.
    void drawDecorationLine(f32 x, f32 y, f32 width, f32 thickness, u8 style, const Color& color) {
        detail::check(kui_paint_draw_decoration_line(paint_, x, y, width, thickness,
                                                     static_cast<KuiCDecorationStyle>(style),
                                                     detail::packColor(color)),
                      "DisplayList::drawDecorationLine");
    }
    KuiPaintContext* cHandle() const noexcept { return paint_; }

private:
    static KuiCCornerRadii toC(const CornerRadii& r) noexcept {
        return {r.topLeft.width, r.topLeft.height, r.topRight.width, r.topRight.height,
                r.bottomRight.width, r.bottomRight.height, r.bottomLeft.width, r.bottomLeft.height};
    }
    KuiPaintContext* paint_ = nullptr;
};

struct CustomElementDefinition {
    String tagName;
    using Factory = std::function<RefPtr<Element>(Document*)>;
    using LifecycleCallback = std::function<void(Element*)>;
    using AttributeCallback = std::function<void(Element*, const String&,
                                                  const String&, const String&)>;
    Factory factory;
    LifecycleCallback connectedCallback;
    LifecycleCallback disconnectedCallback;
    AttributeCallback attributeChangedCallback;
    std::vector<String> observedAttributes;

    // Section 17 (3.61.8): custom paint, tick and input, as in native.
    struct PaintContext {
        DisplayList* displayList = nullptr;  // the wrapper's DisplayList (see above)
        f32 contentWidth = 0;
        f32 contentHeight = 0;
        f32 devicePixelRatio = 1;
        Element* element = nullptr;
    };
    using PaintCallback = std::function<void(PaintContext&)>;
    PaintCallback paintCallback;

    // deltaTime in seconds. Ticks come about every 250 ms; a tick that calls markNeedsPaint()
    // gets the next frame's tick too.
    using TickCallback = std::function<void(Element*, f64 deltaTime)>;
    TickCallback tickCallback;

    struct SizeInfo { f32 width; f32 height; };
    using IntrinsicSizeCallback = std::function<SizeInfo(Element*, f32 availableWidth)>;
    // Kept for source compatibility; KUI layout does not consult it yet (native neither).
    IntrinsicSizeCallback intrinsicSizeCallback;

    using NeedsTickCallback = std::function<bool(Element*)>;
    NeedsTickCallback needsTickCallback;

    // Input callbacks return true when they consumed the event. Coordinates are in the content
    // box; modifiers: 1 Ctrl, 2 Shift, 4 Alt, 8 Meta.
    struct KeyEventData {
        int eventType;  // 0 down, 1 up
        String key;
        String code;
        int modifiers;
        int isRepeat;
    };
    struct MouseEventData {
        int eventType;  // 0 down, 1 up, 2 move, 3 enter, 4 leave
        f32 x, y;
        int button;     // 0 left, 1 middle, 2 right, -1 none
        int buttons;
        int modifiers;
        int clickCount;
    };
    struct WheelEventData {
        f32 x, y;
        f32 deltaX, deltaY;
        int modifiers;
        int momentum;   // always 0 (as in native)
    };
    struct ImeEventData {
        int eventType;  // 0 start, 1 update, 2 commit, 3 cancel
        String text;
        int caretPos;
        String committedText;
    };
    using KeyEventCallback = std::function<bool(Element*, const KeyEventData&)>;
    using MouseEventCallback = std::function<bool(Element*, const MouseEventData&)>;
    using WheelEventCallback = std::function<bool(Element*, const WheelEventData&)>;
    using ImeEventCallback = std::function<bool(Element*, const ImeEventData&)>;
    KeyEventCallback keyEventCallback;
    MouseEventCallback mouseEventCallback;
    WheelEventCallback wheelEventCallback;
    ImeEventCallback imeEventCallback;

    // Kept for source compatibility; not passed on (repaints take the default path).
    bool repaintContract = false;
};

class CustomElementRegistry {
public:
    explicit CustomElementRegistry(std::shared_ptr<detail::DocumentState> state)
        : state_(std::move(state)) {}

    bool define(const String& tagName, CustomElementDefinition definition) {
        auto registration = std::make_shared<Registration>();
        registration->state = state_;
        registration->definition = std::move(definition);
        KuiCCustomElementCallbacks callbacks{};
        callbacks.connected = &Registration::connected;
        callbacks.disconnected = &Registration::disconnected;
        callbacks.attribute_changed = &Registration::attributeChanged;
        registration->observedAttributes.reserve(
            registration->definition.observedAttributes.size());
        for (const auto& name : registration->definition.observedAttributes)
            registration->observedAttributes.push_back(name.c_str());
        callbacks.observed_attributes = registration->observedAttributes.data();
        callbacks.observed_attribute_count = registration->observedAttributes.size();
        callbacks.user_data = registration.get();
        KuiCCustomElementDefinition full{};
        full.callbacks = callbacks;
        const auto& def = registration->definition;
        if (def.paintCallback) full.paint = &Registration::paint;
        if (def.tickCallback) full.tick = &Registration::tick;
        if (def.needsTickCallback) full.needs_tick = &Registration::needsTick;
        if (def.keyEventCallback) full.key = &Registration::key;
        if (def.mouseEventCallback) full.mouse = &Registration::mouse;
        if (def.wheelEventCallback) full.wheel = &Registration::wheel;
        if (def.imeEventCallback) full.ime = &Registration::ime;
        // The registry keeps the registration alive, so KUI need not release it.
        const auto status = kui_document_define_custom_element_ex(
            state_->raw, tagName.data(), tagName.size(), &full);
        if (status != KUI_OK) return false;
        registrations_.push_back(std::move(registration));
        return true;
    }

    bool isDefined(const String& tagName) const {
        int value = 0;
        detail::check(kui_document_custom_element_is_defined(
                          state_->raw, tagName.data(), tagName.size(), &value),
                      "CustomElementRegistry::isDefined");
        return value != 0;
    }

private:
    struct Registration {
        std::shared_ptr<detail::DocumentState> state;
        CustomElementDefinition definition;
        std::vector<const char*> observedAttributes;
        static void paint(KuiElement* raw, KuiPaintContext* context, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                DisplayList list(context);
                CustomElementDefinition::PaintContext ctx;
                ctx.displayList = &list;
                kui_paint_get_info(context, &ctx.contentWidth, &ctx.contentHeight,
                                   &ctx.devicePixelRatio);
                ctx.element = self->state->borrow(raw);
                self->definition.paintCallback(ctx);
            } catch (...) {}
        }
        static void tick(KuiElement* raw, double delta, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                self->definition.tickCallback(self->state->borrow(raw), delta);
            } catch (...) {}
        }
        static int needsTick(KuiElement* raw, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                return self->definition.needsTickCallback(self->state->borrow(raw)) ? 1 : 0;
            } catch (...) {
                return 0;
            }
        }
        static int key(KuiElement* raw, const KuiCCustomKeyEvent* event, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                const CustomElementDefinition::KeyEventData data{
                    static_cast<int>(event->type), String(event->key, event->key_len),
                    String(event->code, event->code_len), event->modifiers, event->is_repeat};
                return self->definition.keyEventCallback(self->state->borrow(raw), data) ? 1 : 0;
            } catch (...) {
                return 0;
            }
        }
        static int mouse(KuiElement* raw, const KuiCCustomMouseEvent* event, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                const CustomElementDefinition::MouseEventData data{
                    static_cast<int>(event->type), event->x, event->y, event->button,
                    event->buttons, event->modifiers, event->click_count};
                return self->definition.mouseEventCallback(self->state->borrow(raw), data) ? 1 : 0;
            } catch (...) {
                return 0;
            }
        }
        static int wheel(KuiElement* raw, const KuiCCustomWheelEvent* event, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                const CustomElementDefinition::WheelEventData data{
                    event->x, event->y, event->delta_x, event->delta_y, event->modifiers, 0};
                return self->definition.wheelEventCallback(self->state->borrow(raw), data) ? 1 : 0;
            } catch (...) {
                return 0;
            }
        }
        static int ime(KuiElement* raw, const KuiCCustomImeEvent* event, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                const CustomElementDefinition::ImeEventData data{
                    static_cast<int>(event->type), String(event->text, event->text_len),
                    event->caret_pos, String(event->committed_text, event->committed_len)};
                return self->definition.imeEventCallback(self->state->borrow(raw), data) ? 1 : 0;
            } catch (...) {
                return 0;
            }
        }
        static void connected(KuiElement* raw, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                if (self->definition.connectedCallback)
                    self->definition.connectedCallback(self->state->borrow(raw));
            } catch (...) {}
        }
        static void disconnected(KuiElement* raw, void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                if (self->definition.disconnectedCallback)
                    self->definition.disconnectedCallback(self->state->borrow(raw));
            } catch (...) {}
        }
        static void attributeChanged(KuiElement* raw,
                                     const char* name, std::size_t nameLen,
                                     const char* oldValue, std::size_t oldLen,
                                     const char* newValue, std::size_t newLen,
                                     void* userData) noexcept {
            auto* self = static_cast<Registration*>(userData);
            try {
                if (self->definition.attributeChangedCallback) {
                    self->definition.attributeChangedCallback(
                        self->state->borrow(raw), String(name, nameLen),
                        String(oldValue, oldLen), String(newValue, newLen));
                }
            } catch (...) {}
        }
    };

    std::shared_ptr<detail::DocumentState> state_;
    std::vector<std::shared_ptr<Registration>> registrations_;
};

// Same shape as the native kui::ScriptValue (nil, bool, integer, float, string, function,
// light userdata). A function value calls into Lua (or into a C++ function handed to Lua).
struct ScriptValue {
    enum Type { Nil, Bool, Int, Float, String, Function, UserData };

    using FunctionType = std::function<ScriptValue(const std::vector<ScriptValue>&)>;

    Type type = Nil;
    std::variant<std::monostate, bool, i64, f64, kui::String, FunctionType, void*> data;

    static ScriptValue nil() { return {}; }
    static ScriptValue from(bool v) { ScriptValue sv; sv.type = Bool; sv.data = v; return sv; }
    static ScriptValue from(i64 v) { ScriptValue sv; sv.type = Int; sv.data = v; return sv; }
    static ScriptValue from(i32 v) { return from(static_cast<i64>(v)); }
    static ScriptValue from(f64 v) { ScriptValue sv; sv.type = Float; sv.data = v; return sv; }
    static ScriptValue from(f32 v) { return from(static_cast<f64>(v)); }
    static ScriptValue from(const kui::String& v) {
        ScriptValue sv; sv.type = String; sv.data = v; return sv;
    }
    static ScriptValue from(const char* v) { return from(kui::String(v)); }
    static ScriptValue fromFunction(FunctionType fn) {
        ScriptValue sv; sv.type = Function; sv.data = std::move(fn); return sv;
    }
    static ScriptValue fromUserData(void* ptr) {
        ScriptValue sv; sv.type = UserData; sv.data = ptr; return sv;
    }

    bool isNil() const { return type == Nil; }
    bool asBool() const { return type == Bool ? std::get<bool>(data) : false; }
    i64 asInt() const {
        if (type == Int) return std::get<i64>(data);
        if (type == Float) return static_cast<i64>(std::get<f64>(data));
        return 0;
    }
    f64 asFloat() const {
        if (type == Float) return std::get<f64>(data);
        if (type == Int) return static_cast<f64>(std::get<i64>(data));
        return 0.0;
    }
    const kui::String& asString() const {
        static const kui::String empty;
        return type == String ? std::get<kui::String>(data) : empty;
    }
    const FunctionType& asFunction() const { return std::get<FunctionType>(data); }
    void* asUserData() const { return type == UserData ? std::get<void*>(data) : nullptr; }
};

struct ScriptResult {
    bool success = false;
    String error;
    ScriptValue value;
};

namespace detail {

using ScriptFunctionHandle = std::shared_ptr<KuiScriptFunction>;

inline ScriptFunctionHandle ownScriptFunction(KuiScriptFunction* raw) {
    return ScriptFunctionHandle(raw, [](KuiScriptFunction* value) { kui_script_function_release(value); });
}

// The engine a value conversion works in; `alive` expires with the wrapper ScriptEngine.
struct ScriptContext {
    KuiScriptEngine* engine = nullptr;
    std::weak_ptr<void> alive;
};

inline String scriptLastError(KuiScriptEngine* engine) {
    return readString([engine](char* buffer, std::size_t capacity, std::size_t* size) {
        return kui_script_engine_get_last_error(engine, buffer, capacity, size);
    }, "ScriptEngine::lastError");
}

ScriptValue scriptFromC(const ScriptContext& context, const KuiCScriptValue& value);
KuiCScriptValue scriptToC(const ScriptContext& context, const ScriptValue& value,
                          std::vector<ScriptFunctionHandle>& keep);

// A function behind a KuiScriptFunction handle (a Lua function, or a C++ one already in Lua).
struct ScriptFunctionRef {
    ScriptContext context;
    ScriptFunctionHandle handle;

    ScriptValue operator()(const std::vector<ScriptValue>& args) const {
        if (context.alive.expired()) throw Error(KUI_ERR_INVALID_ARG, "ScriptValue::Function (engine gone)");
        std::vector<ScriptFunctionHandle> keep;
        std::vector<KuiCScriptValue> values;
        values.reserve(args.size());
        for (const auto& arg : args) values.push_back(scriptToC(context, arg, keep));
        int success = 0;
        KuiCScriptValue result{};
        check(kui_script_function_call(handle.get(), values.empty() ? nullptr : values.data(),
                                       values.size(), &success, &result),
              "ScriptValue::Function");
        ScriptValue converted = scriptFromC(context, result);
        // A C++ function that threw: rethrow, as a native function value would.
        if (!success) throw std::runtime_error(scriptLastError(context.engine));
        return converted;
    }
};

// A C++ function handed to Lua; owned by KUI through user_data, freed by release.
struct ScriptCallback {
    ScriptValue::FunctionType fn;
    ScriptContext context;
    // The last result, which KUI copies right after the trampoline returns.
    String resultText;
    std::vector<ScriptFunctionHandle> resultFunctions;

    static KuiStatus trampoline(const KuiCScriptValue* args, std::size_t count,
                                KuiCScriptValue* out, void* userData) noexcept {
        auto* self = static_cast<ScriptCallback*>(userData);
        try {
            std::vector<ScriptValue> values;
            values.reserve(count);
            for (std::size_t index = 0; index < count; ++index)
                values.push_back(scriptFromC(self->context, args[index]));
            const ScriptValue result = self->fn(values);
            std::vector<ScriptFunctionHandle> keep;
            *out = scriptToC(self->context, result, keep);
            self->resultFunctions = std::move(keep);
            if (result.type == ScriptValue::String) {
                self->resultText = result.asString();
                out->string_value = self->resultText.c_str();
                out->string_len = self->resultText.size();
            }
            return KUI_OK;
        } catch (const std::exception& error) {
            return fail(self, out, error.what());
        } catch (...) {
            return fail(self, out, "C++ exception");
        }
    }
    static void release(void* userData) noexcept { delete static_cast<ScriptCallback*>(userData); }

private:
    static KuiStatus fail(ScriptCallback* self, KuiCScriptValue* out, const char* message) noexcept {
        try {
            self->resultText = message;
            out->type = KUI_C_SCRIPT_STRING;
            out->string_value = self->resultText.c_str();
            out->string_len = self->resultText.size();
        } catch (...) {}
        return KUI_ERR_INTERNAL;
    }
};

inline ScriptValue scriptFromC(const ScriptContext& context, const KuiCScriptValue& value) {
    switch (value.type) {
        case KUI_C_SCRIPT_BOOL: return ScriptValue::from(value.bool_value != 0);
        case KUI_C_SCRIPT_INT: return ScriptValue::from(static_cast<i64>(value.int_value));
        case KUI_C_SCRIPT_FLOAT: return ScriptValue::from(static_cast<f64>(value.float_value));
        case KUI_C_SCRIPT_STRING:
            return ScriptValue::from(value.string_value ? String(value.string_value, value.string_len)
                                                        : String());
        case KUI_C_SCRIPT_FUNCTION: {
            KuiScriptFunction* owned = nullptr;
            check(kui_script_function_retain(value.function_value, &owned), "ScriptValue(function)");
            return ScriptValue::fromFunction(ScriptFunctionRef{context, ownScriptFunction(owned)});
        }
        case KUI_C_SCRIPT_USERDATA: return ScriptValue::fromUserData(value.userdata);
        default: return ScriptValue::nil();
    }
}

// Strings point into `value`; function handles are held by `keep`.
inline KuiCScriptValue scriptToC(const ScriptContext& context, const ScriptValue& value,
                                 std::vector<ScriptFunctionHandle>& keep) {
    KuiCScriptValue out{};
    switch (value.type) {
        case ScriptValue::Nil: out.type = KUI_C_SCRIPT_NIL; break;
        case ScriptValue::Bool: out.type = KUI_C_SCRIPT_BOOL; out.bool_value = value.asBool() ? 1 : 0; break;
        case ScriptValue::Int: out.type = KUI_C_SCRIPT_INT; out.int_value = value.asInt(); break;
        case ScriptValue::Float: out.type = KUI_C_SCRIPT_FLOAT; out.float_value = value.asFloat(); break;
        case ScriptValue::String:
            out.type = KUI_C_SCRIPT_STRING;
            out.string_value = value.asString().c_str();
            out.string_len = value.asString().size();
            break;
        case ScriptValue::Function: {
            out.type = KUI_C_SCRIPT_FUNCTION;
            if (const auto* ref = value.asFunction().target<ScriptFunctionRef>()) {
                keep.push_back(ref->handle);
            } else {
                if (context.alive.expired()) throw Error(KUI_ERR_INVALID_ARG, "ScriptValue (engine gone)");
                auto* callback = new ScriptCallback{value.asFunction(), context, {}, {}};
                KuiScriptFunction* raw = nullptr;
                const KuiStatus status = kui_script_function_create(
                    context.engine, &ScriptCallback::trampoline, callback, &ScriptCallback::release, &raw);
                if (status != KUI_OK) delete callback;
                check(status, "ScriptValue(function)");
                keep.push_back(ownScriptFunction(raw));
            }
            out.function_value = keep.back().get();
            break;
        }
        case ScriptValue::UserData: out.type = KUI_C_SCRIPT_USERDATA; out.userdata = value.asUserData(); break;
    }
    return out;
}

}  // namespace detail

class ScriptEngine {
public:
    using ErrorHandler = std::function<void(const String&, const String&, int)>;
    using PrintHandler = std::function<void(const String& message)>;

    static Unique<ScriptEngine> create() {
        KuiScriptEngine* raw = nullptr;
        const auto status = kui_script_engine_create(&raw);
        if (status == KUI_ERR_NOT_SUPPORTED) return {};
        detail::check(status, "ScriptEngine::create");
        return Unique<ScriptEngine>(new ScriptEngine(raw));
    }
    ~ScriptEngine() {
        alive_.reset();
        if (raw_) kui_script_engine_release(raw_);
    }
    // Runs the due timers (setTimeout / setInterval) for the time nowMs (the Platform::now clock).
    // The engine does not tick itself: call it once per frame when scripts use timers.
    void tickTimers(f64 nowMs) { detail::check(kui_script_engine_tick_timers(raw_, nowMs), "ScriptEngine::tickTimers"); }
    // The error text of the last failed run (eval, evalFile, callFunction, a script function); empty after
    // a successful one.
    String lastError() const { return detail::scriptLastError(raw_); }
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    void bindDocument(Document* document) {
        detail::check(kui_script_engine_bind_document(raw_, document->cHandle()),
                      "ScriptEngine::bindDocument");
    }
    void unbindDocument() {
        detail::check(kui_script_engine_unbind_document(raw_),
                      "ScriptEngine::unbindDocument");
    }
    ScriptResult eval(const String& code, const String& source = "<eval>") {
        int success = 0;
        KuiCScriptValue value{};
        detail::check(kui_script_engine_eval_value(raw_, code.data(), code.size(), source.data(),
                                                   source.size(), &success, &value),
                      "ScriptEngine::eval");
        return finish(success, value);
    }
    ScriptResult evalFile(const String& path) {
        int success = 0;
        KuiCScriptValue value{};
        detail::check(kui_script_engine_eval_file(raw_, path.data(), path.size(), &success, &value),
                      "ScriptEngine::evalFile");
        return finish(success, value);
    }
    ScriptResult callFunction(const String& name, const std::vector<ScriptValue>& args = {}) {
        std::vector<detail::ScriptFunctionHandle> keep;
        std::vector<KuiCScriptValue> values;
        values.reserve(args.size());
        for (const auto& arg : args) values.push_back(detail::scriptToC(context(), arg, keep));
        int success = 0;
        KuiCScriptValue value{};
        detail::check(kui_script_engine_call_function(raw_, name.data(), name.size(),
                                                      values.empty() ? nullptr : values.data(),
                                                      values.size(), &success, &value),
                      "ScriptEngine::callFunction");
        return finish(success, value);
    }
    void registerFunction(const String& name, ScriptValue::FunctionType fn) {
        auto* callback = new detail::ScriptCallback{std::move(fn), context(), {}, {}};
        const KuiStatus status = kui_script_engine_register_function(
            raw_, name.data(), name.size(), &detail::ScriptCallback::trampoline, callback,
            &detail::ScriptCallback::release);
        if (status != KUI_OK) delete callback;
        detail::check(status, "ScriptEngine::registerFunction");
    }
    void setGlobal(const String& name, ScriptValue value) {
        std::vector<detail::ScriptFunctionHandle> keep;
        const KuiCScriptValue converted = detail::scriptToC(context(), value, keep);
        detail::check(kui_script_engine_set_global(raw_, name.data(), name.size(), &converted),
                      "ScriptEngine::setGlobal");
    }
    ScriptValue getGlobal(const String& name) {
        KuiCScriptValue value{};
        detail::check(kui_script_engine_get_global(raw_, name.data(), name.size(), &value),
                      "ScriptEngine::getGlobal");
        return detail::scriptFromC(context(), value);
    }
    void executeScripts() {
        detail::check(kui_script_engine_execute_scripts(raw_),
                      "ScriptEngine::executeScripts");
    }
    void setErrorHandler(ErrorHandler handler) {
        errorHandler_ = std::move(handler);
        detail::check(kui_script_engine_set_error_handler(
                          raw_, errorHandler_ ? &ScriptEngine::errorTrampoline : nullptr,
                          this), "ScriptEngine::setErrorHandler");
    }
    void setPrintHandler(PrintHandler handler) {
        if (!handler) {
            detail::check(kui_script_engine_set_print_handler(raw_, nullptr, nullptr, nullptr),
                          "ScriptEngine::setPrintHandler");
            return;
        }
        auto* holder = new PrintHandler(std::move(handler));
        const KuiStatus status = kui_script_engine_set_print_handler(
            raw_, &ScriptEngine::printTrampoline, holder,
            [](void* userData) { delete static_cast<PrintHandler*>(userData); });
        if (status != KUI_OK) delete holder;
        detail::check(status, "ScriptEngine::setPrintHandler");
    }

    std::size_t memoryUsageBytes() const {
        std::size_t value = 0;
        detail::check(kui_script_engine_get_resource_usage(raw_, &value, nullptr, nullptr),
                      "ScriptEngine::memoryUsageBytes");
        return value;
    }
    void setMemoryLimit(std::size_t bytes) {
        detail::check(kui_script_engine_set_memory_limit(raw_, bytes), "ScriptEngine::setMemoryLimit");
    }
    void setInstructionLimit(std::uint64_t maxOps) {
        detail::check(kui_script_engine_set_instruction_limit(raw_, maxOps),
                      "ScriptEngine::setInstructionLimit");
    }
    // No limit reads as SIZE_MAX, as in native.
    std::size_t memoryLimitBytes() const {
        std::size_t value = 0;
        detail::check(kui_script_engine_get_resource_usage(raw_, nullptr, &value, nullptr),
                      "ScriptEngine::memoryLimitBytes");
        return value == 0 ? SIZE_MAX : value;
    }
    std::uint64_t instructionLimit() const {
        std::uint64_t value = 0;
        detail::check(kui_script_engine_get_resource_usage(raw_, nullptr, nullptr, &value),
                      "ScriptEngine::instructionLimit");
        return value;
    }
    KuiScriptEngine* cHandle() const noexcept { return raw_; }

private:
    explicit ScriptEngine(KuiScriptEngine* raw) : raw_(raw) {}
    detail::ScriptContext context() const { return {raw_, alive_}; }
    ScriptResult finish(int success, const KuiCScriptValue& value) {
        ScriptResult result;
        result.success = success != 0;
        result.value = detail::scriptFromC(context(), value);
        if (!result.success) result.error = detail::scriptLastError(raw_);
        return result;
    }
    static void errorTrampoline(const char* error, std::size_t errorLen,
                                const char* source, std::size_t sourceLen,
                                int line, void* userData) noexcept {
        auto* self = static_cast<ScriptEngine*>(userData);
        try {
            if (self->errorHandler_)
                self->errorHandler_(String(error, errorLen), String(source, sourceLen), line);
        } catch (...) {}
    }
    static void printTrampoline(const char* message, std::size_t length, void* userData) noexcept {
        try {
            (*static_cast<PrintHandler*>(userData))(String(message, length));
        } catch (...) {}
    }
    KuiScriptEngine* raw_ = nullptr;
    std::shared_ptr<void> alive_ = std::make_shared<int>(0);
    ErrorHandler errorHandler_;
};

class HttpClient {
public:
    struct Response {
        i32 statusCode = 0;
        std::vector<std::uint8_t> body;
        String contentType;
        String error;
    };
    using Callback = std::function<void(Response)>;

    HttpClient() { detail::check(kui_http_client_create(&raw_), "HttpClient::HttpClient"); }
    ~HttpClient() {
        if (raw_) {
            kui_http_client_cancel_all(raw_);
            kui_http_client_release(raw_);
        }
    }
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    void setUserAgent(const String& value) {
        detail::check(kui_http_client_set_user_agent(raw_, value.data(), value.size()),
                      "HttpClient::setUserAgent");
    }
    Response fetchSync(const String& url, f64 timeoutSeconds = 30) {
        KuiHttpResponse* response = nullptr;
        detail::check(kui_http_client_fetch_sync(raw_, url.data(), url.size(),
                                                 timeoutSeconds, &response),
                      "HttpClient::fetchSync");
        return takeResponse(response);
    }
    // callback runs on an HTTP worker thread (like native kui::HttpClient::fetchAsync):
    // hand UI work to the main thread with Platform::postToMainThread, or use
    // fetchAsyncOnMainThread.
    void fetchAsync(const String& url, Callback callback, f64 timeoutSeconds = 30) {
        start(url, std::move(callback), timeoutSeconds, false);
    }
    // callback runs on the main thread inside the event loop and may touch the DOM.
    void fetchAsyncOnMainThread(const String& url, Callback callback, f64 timeoutSeconds = 30) {
        start(url, std::move(callback), timeoutSeconds, true);
    }
    void cancelAll() {
        detail::check(kui_http_client_cancel_all(raw_), "HttpClient::cancelAll");
    }

private:
    struct AsyncHolder { Callback callback; };
    // The holder belongs to the pending request, not to this client: a request
    // cancelled by the destructor still completes later (with an error), after
    // this object is gone. The trampoline takes ownership and frees it.
    void start(const String& url, Callback callback, f64 timeoutSeconds, bool onMainThread) {
        auto holder = std::make_unique<AsyncHolder>();
        holder->callback = std::move(callback);
        const KuiStatus status = onMainThread
            ? kui_http_client_fetch_async_on_main_thread(raw_, url.data(), url.size(), timeoutSeconds,
                                                         &HttpClient::asyncTrampoline, holder.get())
            : kui_http_client_fetch_async(raw_, url.data(), url.size(), timeoutSeconds,
                                          &HttpClient::asyncTrampoline, holder.get());
        detail::check(status, onMainThread ? "HttpClient::fetchAsyncOnMainThread"
                                           : "HttpClient::fetchAsync");
        holder.release();
    }
    static Response takeResponse(KuiHttpResponse* raw) {
        struct Releaser {
            void operator()(KuiHttpResponse* value) const {
                if (value) kui_http_response_release(value);
            }
        };
        std::unique_ptr<KuiHttpResponse, Releaser> owner(raw);
        Response result;
        int status = 0;
        detail::check(kui_http_response_get_status(raw, &status), "HttpClient::response(status)");
        result.statusCode = status;
        std::size_t bodySize = 0;
        detail::check(kui_http_response_get_body(raw, nullptr, 0, &bodySize),
                      "HttpClient::response(body-size)");
        result.body.resize(bodySize + (bodySize ? 1 : 0));
        if (bodySize) {
            detail::check(kui_http_response_get_body(
                raw, result.body.data(), result.body.size(), &bodySize),
                "HttpClient::response(body)");
            result.body.resize(bodySize);
        }
        result.contentType = detail::readString(
            [raw](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_http_response_get_content_type(raw, buffer, capacity, size);
            }, "HttpClient::response(content-type)");
        result.error = detail::readString(
            [raw](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_http_response_get_error(raw, buffer, capacity, size);
            }, "HttpClient::response(error)");
        return result;
    }
    static void asyncTrampoline(KuiHttpResponse* response, void* userData) noexcept {
        std::unique_ptr<AsyncHolder> holder(static_cast<AsyncHolder*>(userData));
        // NULL: the result could not be delivered (allocation failure, or dropped
        // at platform shutdown); only the holder is released then.
        if (!response) return;
        try { holder->callback(takeResponse(response)); }
        catch (...) {}
    }
    KuiHttpClient* raw_ = nullptr;
};

inline RefPtr<Element> Element::create(Document* owner, const String& tagName) {
    return owner ? owner->createElement(tagName) : RefPtr<Element>();
}

inline void Element::positionRelativeTo(const Element* anchor, const String& placement,
                                        const PositioningOptions& options) {
    if (!anchor) throw Error(KUI_ERR_INVALID_ARG, "Element::positionRelativeTo");
    KuiCPositioningOptions raw{options.flip ? 1 : 0,
                               options.clampToViewport ? 1 : 0,
                               options.offset, options.viewportMargin};
    detail::check(kui_element_position_relative_to(
                      raw_, anchor->raw_, placement.data(), placement.size(), &raw),
                  "Element::positionRelativeTo");
}

inline void Element::mediaPlay() { detail::check(kui_media_play(raw_), "Element::mediaPlay"); }
inline void Element::mediaPause() { detail::check(kui_media_pause(raw_), "Element::mediaPause"); }
inline void Element::mediaSetSrc(const String& value) { detail::check(kui_media_set_src(raw_, value.data(), value.size()), "Element::mediaSetSrc"); }
inline void Element::mediaSetControls(bool value) { detail::check(kui_media_set_controls(raw_, value ? 1 : 0), "Element::mediaSetControls"); }
inline void Element::mediaSetCurrentTime(f64 value) { detail::check(kui_media_set_current_time(raw_, value), "Element::mediaSetCurrentTime"); }
inline f64 Element::mediaCurrentTime() const { f64 value = 0; detail::check(kui_media_get_current_time(raw_, &value), "Element::mediaCurrentTime"); return value; }
inline f64 Element::mediaDuration() const { f64 value = 0; detail::check(kui_media_get_duration(raw_, &value), "Element::mediaDuration"); return value; }
inline void Element::mediaSetVolume(f32 value) { detail::check(kui_media_set_volume(raw_, value), "Element::mediaSetVolume"); }
inline f32 Element::mediaVolume() const { f32 value = 0; detail::check(kui_media_get_volume(raw_, &value), "Element::mediaVolume"); return value; }
inline void Element::mediaSetMuted(bool value) { detail::check(kui_media_set_muted(raw_, value ? 1 : 0), "Element::mediaSetMuted"); }
inline bool Element::mediaMuted() const { int value = 0; detail::check(kui_media_get_muted(raw_, &value), "Element::mediaMuted"); return value != 0; }
inline bool Element::mediaPaused() const { int value = 0; detail::check(kui_media_get_paused(raw_, &value), "Element::mediaPaused"); return value != 0; }
inline i32 Element::mediaReadyState() const { int value = 0; detail::check(kui_media_get_ready_state(raw_, &value), "Element::mediaReadyState"); return value; }
inline i32 Element::mediaNetworkState() const { int value = 0; detail::check(kui_media_get_network_state(raw_, &value), "Element::mediaNetworkState"); return value; }
inline bool Element::mediaAvailable() const { int value = 0; detail::check(kui_media_is_available(raw_, &value), "Element::mediaAvailable"); return value != 0; }
inline void Element::lottieSetSrc(const String& value) { detail::check(kui_lottie_set_src(raw_, value.data(), value.size()), "Element::lottieSetSrc"); }
inline void Element::lottiePlay() { detail::check(kui_lottie_play(raw_), "Element::lottiePlay"); }
inline void Element::lottiePause() { detail::check(kui_lottie_pause(raw_), "Element::lottiePause"); }
inline bool Element::lottieComplete() const { int value = 0; detail::check(kui_lottie_get_complete(raw_, &value), "Element::lottieComplete"); return value != 0; }
inline Size Element::lottieNaturalSize() const { Size value{}; detail::check(kui_lottie_get_natural_size(raw_, &value.width, &value.height), "Element::lottieNaturalSize"); return value; }
inline void Element::mediaLoad() { detail::check(kui_media_load(raw_), "Element::mediaLoad"); }
inline void Element::mediaSetLoop(bool loop) { detail::check(kui_media_set_loop(raw_, loop ? 1 : 0), "Element::mediaSetLoop"); }
inline void Element::mediaSetAutoplay(bool autoplay) { detail::check(kui_media_set_autoplay(raw_, autoplay ? 1 : 0), "Element::mediaSetAutoplay"); }
inline bool Element::mediaLoop() const { int value = 0; detail::check(kui_media_get_playback_flags(raw_, &value, nullptr, nullptr), "Element::mediaLoop"); return value != 0; }
inline bool Element::mediaAutoplay() const { int value = 0; detail::check(kui_media_get_playback_flags(raw_, nullptr, &value, nullptr), "Element::mediaAutoplay"); return value != 0; }
inline bool Element::mediaEnded() const { int value = 0; detail::check(kui_media_get_playback_flags(raw_, nullptr, nullptr, &value), "Element::mediaEnded"); return value != 0; }
inline void Element::lottieStop() { detail::check(kui_lottie_stop(raw_), "Element::lottieStop"); }
inline void Element::lottieSeek(f64 position) { detail::check(kui_lottie_seek(raw_, position), "Element::lottieSeek"); }
inline void Element::lottieSetSpeed(f32 speed) { detail::check(kui_lottie_set_speed(raw_, speed), "Element::lottieSetSpeed"); }
inline void Element::lottieSetLoop(bool loop) { detail::check(kui_lottie_set_loop(raw_, loop ? 1 : 0), "Element::lottieSetLoop"); }
inline KuiCLottieState Element::lottieState() const { KuiCLottieState value{}; detail::check(kui_lottie_get_state(raw_, &value), "Element::lottieState"); return value; }
inline bool Element::loop() const {
    int value = 0;
    if (kui_media_get_playback_flags(raw_, &value, nullptr, nullptr) == KUI_OK) return value != 0;
    return lottieState().looping != 0;
}
inline void Element::setLoop(bool loop) {
    const auto status = kui_media_set_loop(raw_, loop ? 1 : 0);
    if (status == KUI_ERR_NOT_SUPPORTED) lottieSetLoop(loop);
    else detail::check(status, "Element::setLoop");
}
inline bool Element::autoplay() const {
    int value = 0;
    if (kui_media_get_playback_flags(raw_, nullptr, &value, nullptr) == KUI_OK) return value != 0;
    return lottieState().autoplay != 0;
}
inline void Element::setAutoplay(bool autoplay) {
    const auto status = kui_media_set_autoplay(raw_, autoplay ? 1 : 0);
    if (status != KUI_ERR_NOT_SUPPORTED) {
        detail::check(status, "Element::setAutoplay");
    } else if (autoplay) {
        setAttribute("autoplay", "");
    } else {
        removeAttribute("autoplay");
    }
}
inline void Element::play() {
    const auto mediaStatus = kui_media_play(raw_);
    if (mediaStatus == KUI_ERR_NOT_SUPPORTED) {
        detail::check(kui_lottie_play(raw_), "Element::play");
    } else {
        detail::check(mediaStatus, "Element::play");
    }
}
inline void Element::pause() {
    const auto mediaStatus = kui_media_pause(raw_);
    if (mediaStatus == KUI_ERR_NOT_SUPPORTED) {
        detail::check(kui_lottie_pause(raw_), "Element::pause");
    } else {
        detail::check(mediaStatus, "Element::pause");
    }
}
inline void Element::setSrc(const String& source) {
    const auto mediaStatus = kui_media_set_src(raw_, source.data(), source.size());
    if (mediaStatus == KUI_ERR_NOT_SUPPORTED) {
        detail::check(kui_lottie_set_src(raw_, source.data(), source.size()), "Element::setSrc");
    } else {
        detail::check(mediaStatus, "Element::setSrc");
    }
}

inline Document::~Document() = default;
inline void Document::initExtensions(const String& directory) {
    std::size_t loaded = 0;
    detail::check(kui_document_init_extensions(state_->raw, directory.data(),
                                               directory.size(), &loaded),
                  "Document::initExtensions");
}
inline ExtensionManager* Document::extensionManager() const {
    if (!extensionManager_) extensionManager_ = std::make_unique<ExtensionManager>(state_);
    return extensionManager_.get();
}
inline CustomElementRegistry* Document::customElements() const {
    if (!customElementRegistry_)
        customElementRegistry_ = std::make_unique<CustomElementRegistry>(state_);
    return customElementRegistry_.get();
}
inline bool Document::printToPDF(const String& path, const PDFOptions& options) {
    KuiCPDFOptions raw{options.title.c_str(), options.author.c_str(),
                       options.subject.c_str(), options.keywords.c_str(),
                       options.creator.c_str(), options.creationDate.c_str(),
                       options.pageWidth_pt, options.pageHeight_pt,
                       options.rasterizePDFFonts ? 1 : 0,
                       options.generatePDFOutline ? 1 : 0};
    int written = 0;
    const auto status = kui_document_print_to_pdf(state_->raw, path.data(), path.size(),
                                                  &raw, &written);
    if (status == KUI_ERR_IO) return false;
    detail::check(status, "Document::printToPDF");
    return written != 0;
}

// ---- Process-wide host services: console, stack trace writer, crash reporter ------------------

enum class LogLevel : u8 { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// The in-process console (log messages shown by DevTools).
class Console {
public:
    static Console& instance() {
        static Console value;
        return value;
    }
    void setEnabled(bool enabled) { detail::check(kui_console_set_enabled(enabled ? 1 : 0), "Console::setEnabled"); }
    // Prints every message to stderr as it arrives.
    void setStderrOutput(bool enabled) {
        detail::check(kui_console_set_stderr_output(enabled ? 1 : 0), "Console::setStderrOutput");
    }
    // Messages below this level are dropped.
    void setMinLevel(LogLevel level) {
        detail::check(kui_console_set_min_level(static_cast<KuiCLogLevel>(level)), "Console::setMinLevel");
    }

private:
    Console() = default;
};

// Unhandled-exception / signal stack trace writer. Install once near the top of main, before
// starting threads. (The native SDK's per-crash callback is not available through the C ABI.)
class StackTrace {
public:
    enum OutputMode : u32 {
        WriteToFile = KUI_C_STACK_TRACE_FILE,
        WriteToStderr = KUI_C_STACK_TRACE_STDERR,
        ShowMessageBox = KUI_C_STACK_TRACE_MESSAGE_BOX,
    };
    struct Config {
        u32 outputMode = WriteToFile | WriteToStderr;
        String logFilePath;
        int maxFrames = 64;
        String appName = "kui_app";
        bool writeMiniDump = false;
        String dumpDir;
    };
    // Returns whether the writer is installed. (Two overloads, not `= {}`: clang rejects a nested class's
    // default member initializers inside a default argument of the enclosing class.)
    static bool install() { return install(Config{}); }
    static bool install(const Config& config) {
        KuiCStackTraceConfig raw{};
        raw.struct_size = sizeof(raw);
        raw.output_mode = config.outputMode;
        raw.log_file_path = config.logFilePath.c_str();
        raw.max_frames = config.maxFrames;
        raw.app_name = config.appName.c_str();
        raw.write_mini_dump = config.writeMiniDump ? 1 : 0;
        raw.dump_dir = config.dumpDir.c_str();
        int installed = 0;
        detail::check(kui_stack_trace_install(&raw, &installed), "StackTrace::install");
        return installed != 0;
    }
    static void uninstall() { detail::check(kui_stack_trace_uninstall(), "StackTrace::uninstall"); }
};

// Crashpad crash handler.
class CrashReporter {
public:
    struct Config {
        String handlerExe;    // empty: crashpad_handler next to the executable
        String dumpDir;       // empty: <exe dir>/crashes
        String appName = "kui_app";
        String appVersion = "0.0.0";
        String reportUrl;     // empty: never upload
        bool enableUpload = false;
    };
    // Returns whether the handler started (false when crashpad_handler is missing); the call itself succeeds.
    static bool install() { return install(Config{}); }
    static bool install(const Config& config) {
        KuiCCrashReporterConfig raw{};
        raw.struct_size = sizeof(raw);
        raw.handler_exe = config.handlerExe.c_str();
        raw.dump_dir = config.dumpDir.c_str();
        raw.app_name = config.appName.c_str();
        raw.app_version = config.appVersion.c_str();
        raw.report_url = config.reportUrl.c_str();
        raw.enable_upload = config.enableUpload ? 1 : 0;
        int installed = 0;
        detail::check(kui_crash_reporter_install(&raw, &installed), "CrashReporter::install");
        return installed != 0;
    }
    static void uninstall() { detail::check(kui_crash_reporter_uninstall(), "CrashReporter::uninstall"); }
};


}  // namespace kui
KUI_CPP_HIDDEN_END

#include <kui/desktop.h>
#include <kui/framework.h>
#include <kui/widgets.h>
#include <kui/components.h>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

inline Window::~Window() { reset(); }
inline void Window::setScriptEngine(ScriptEngine* engine) {
    detail::check(kui_window_set_script_engine(raw_, engine ? engine->cHandle() : nullptr),
                  "Window::setScriptEngine");
}

}  // namespace kui
KUI_CPP_HIDDEN_END
