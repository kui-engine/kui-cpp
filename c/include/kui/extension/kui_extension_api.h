// ===========================================================================
// KUI Extension API — C ABI stable interface for third-party DLL plugins
//
// This header defines the contract between KUI host and extension DLLs.
// All types use C linkage for ABI stability across compilers (MSVC/GCC/Clang).
//
// Extension DLLs must export:
//   kuiext_pluginInfo()  — plugin metadata
//   kuiext_init()        — receive Host API function pointers
//   kuiext_register()    — return callbacks + Lua method table
//   kuiext_shutdown()    — release all resources
//
// Version: 1.11 (2026-10-30, Q7 repaint contract)
//   v1.11: optional export kuiext_paintContract() — an extension may declare
//          KUI_EXT_PAINT_CONTRACT_REPAINT_ON_CHANGE (see the block below and
//          sdk docs/extension_repaint_contract.md).  Declared element types no
//          longer force full-page repaints of the host's small-change frames;
//          their scheduleRepaint / invalidateRect requests become exact damage
//          and their onPaint output is clipped to the element's border box.
//          No KuiExtHostAPI change (apiVersion stays 11); hosts older than
//          v1.11 ignore the export, extensions without it behave as before.
// Version: 1.10 (2026-07-25, HOST-020)
//   v1.10: GPU interop — nativeGLContext/nativeGLDC/nativeGpuContext/
//          markExternalGpuDirty (native GPU handle passthrough, mirrors
//          RenderBackend's zero-copy abstraction) + borrowGLTextureAsImage/
//          imageHandleNativeImage/releaseImageHandle/dlDrawCanvasSurface
//          (zero-copy GL texture borrowing + drawing it back into the
//          host DisplayList).  Lets a GPU-rendering extension (e.g. a 3D
//          engine) composite into KUI without a GPU->CPU->GPU readback
//          round-trip.  New capability bit KUI_CAP_GPU_INTEROP gates this
//          group.  apiVersion -> 11.
// Version: 1.9 (2026-06-13, HOST-018)
//   v1.9: dispatchInputEvent — host-side InputEvent dispatch (KUI-036).
//         Lets extension elements emit a DOM `input` event (or custom
//         `ghostaccept` / `ghostdismiss`) through the host EventDispatcher so
//         Lua `addEventListener("input", fn)` listeners fire with `e.data` /
//         `e.inputType` readable.  apiVersion -> 10.
// Version: 1.8 (2026-05-14, HOST-007a)
//   v1.8: clipboard access — getClipboardText / setClipboardText.  Allows
//         editor extensions to implement Ctrl+C/X/V properly without
//         platform-specific code.  Returned pointer is thread-local and
//         valid until the next call.
//   v1.7: input event routing — onKeyEvent / onMouseEvent / onWheelEvent /
//         onImeEvent callbacks on KuiExtRegistration.  Editor extensions can
//         finally receive keyboard / mouse / wheel / IME events directly.
//         Return 1 from a callback to "consume" the event and skip host's
//         default handling.
//   v1.6: setImeCandidatePos + isImeComposing (IME 候选框位置控制)
//   v1.5: invalidateRect (partial repaint hint, ~10× CPU saving vs full repaint)
//   v1.4: requestFocus / hasFocus / blur; KUI Element::focus stubs now
//         properly implemented (走 EventDispatcher::handleFocus)
//   v1.3: dlDrawTextStyled + dlDrawDecorationLine; KuiExtTextStyle struct;
//         KuiDecorationStyle enum
//   v1.2: measureText() + getFontMetrics() added; KuiExtTextMetrics struct
//   v1.1: getDpr() added; KuiExtPaintContext.dpr no longer hardcoded to 1.0
//   v1.0: initial release
// ===========================================================================

#ifndef KUI_EXTENSION_API_H
#define KUI_EXTENSION_API_H

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// DLL export/import macros
// ---------------------------------------------------------------------------
#if defined(_WIN32)
#  if defined(KUI_EXT_BUILDING_DLL)
#    define KUI_EXT_API __declspec(dllexport)
#  else
#    define KUI_EXT_API __declspec(dllimport)
#  endif
#else
#  define KUI_EXT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ===========================================================================
// Error codes
// ===========================================================================
typedef enum {
    KUI_EXT_OK            = 0,
    KUI_EXT_ERR_INIT      = -1,  // Initialization failed
    KUI_EXT_ERR_VERSION   = -2,  // Version incompatible
    KUI_EXT_ERR_RESOURCE  = -3,  // Resource loading failed
    KUI_EXT_ERR_RENDER    = -4,  // Render error
    KUI_EXT_ERR_OOM       = -5,  // Out of memory
    KUI_EXT_ERR_INVALID   = -6,  // Invalid parameter
    KUI_EXT_ERR_PERMISSION = -7, // IMPL-CR-008: load denied by host policy
                                 // (path allowlist / capability / api version)
} KuiExtError;

// ===========================================================================
// IMPL-CR-008: Capability flags (host ↔ extension permission negotiation)
//
// An extension declares which host capabilities it intends to use via the
// optional `kuiext_apiCompat` export.  The host has a corresponding
// `enabledCapabilities()` mask (default: all enabled) that an embedder can
// tighten — e.g. a kiosk app may disable NETWORK + FILESYSTEM for any
// extension it loads.  At load time the host enforces:
//
//     (extension.requiredCapabilities & ~host.enabledCapabilities) == 0
//
// Any extension that does NOT export `kuiext_apiCompat` is treated as
// requiring 0 capabilities (legacy behaviour, fully backward compatible
// with KuiExtHostAPI v9 extensions).
// ===========================================================================
#define KUI_CAP_LUA         0x00000001u  // Lua method registration / lua_State access
#define KUI_CAP_NETWORK     0x00000002u  // outbound HTTP / WebSocket (reserved)
#define KUI_CAP_FILESYSTEM  0x00000004u  // file IO outside KUI sandbox (reserved)
#define KUI_CAP_CLIPBOARD   0x00000008u  // getClipboardText / setClipboardText
#define KUI_CAP_IPC         0x00000010u  // sendMessage to other extensions
#define KUI_CAP_GPU_INTEROP 0x00000020u  // native GL/GPU handle passthrough + zero-copy texture borrowing (HOST-020)
#define KUI_CAP_ALL         0xFFFFFFFFu

// ===========================================================================
// IMPL-CR-008: Compat info — optional export `kuiext_apiCompat`
//
// Returned by extension's `kuiext_apiCompat()` (if exported) to declare:
//   * minimum host API version it expects (compared against
//     `KuiExtHostAPI::apiVersion`)
//   * bitmask of capabilities it intends to use (see KUI_CAP_*).
//
// `requiredApiVersion = 0` means "no minimum" (legacy default).
// `requiredCapabilities = 0` means "no host capabilities required".
//
// The struct is laid out for forward compatibility — extensions that target
// a newer ABI may zero-init unknown trailing fields.
// ===========================================================================
typedef struct {
    uint32_t requiredApiVersion;     // minimum KuiExtHostAPI::apiVersion
    uint32_t requiredCapabilities;   // KUI_CAP_* bitmask
    uint32_t reserved0;              // future use; init to 0
    uint32_t reserved1;              // future use; init to 0
} KuiExtCompatInfo;

// ===========================================================================
// Pixel format
// ===========================================================================
#ifndef KUI_PIXEL_FORMAT_DEFINED
#define KUI_PIXEL_FORMAT_DEFINED
typedef enum {
    KUI_PIXEL_RGBA_UNPREMUL = 0,  // Default for extensions (auto-converts)
    KUI_PIXEL_BGRA_PREMUL  = 1,  // Skia native (zero-copy)
} KuiPixelFormat;
#endif

// ===========================================================================
// Plugin info
// ===========================================================================
typedef struct {
    uint32_t    major, minor, patch;
    const char* name;         // e.g. "kui_ext_pdf"
    const char* tagName;      // e.g. "kui-pdf"
    const char* description;
    const char* author;
} KuiExtPluginInfo;

// ===========================================================================
// Paint context (passed to onPaint callback)
// ===========================================================================
typedef struct {
    void*  displayList;
    float  contentWidth;
    float  contentHeight;
    float  dpr;
    void*  element;
} KuiExtPaintContext;

// ===========================================================================
// Text metrics (HOST-001a / HOST-001c, v2)
//
// Returned by `measureText` (advance width + vertical metrics) and
// `getFontMetrics` (vertical metrics only, `width = 0`).
//
// All values are in CSS pixels (NOT device pixels — divide ctx.dpr if you
// want device-pixel units).  Ascent and descent are POSITIVE distances
// from the alphabetic baseline (typographic convention).  `lineHeight`
// follows the CSS-spec "normal" definition (sTypoAscender + sTypoDescender +
// sTypoLineGap from OS/2 table, matching Chromium/Firefox behaviour).
// ===========================================================================
typedef struct {
    float width;       // text advance width (px), 0 for getFontMetrics
    float ascent;      // px above baseline (positive)
    float descent;     // px below baseline (positive)
    float lineHeight;  // CSS-spec "normal" line height (px)
} KuiExtTextMetrics;

// ===========================================================================
// Rich text style (HOST-001b, v3)
//
// Used by `dlDrawTextStyled` to draw text with weight / italic / spacing.
// Pass `nullptr` to dlDrawTextStyled to use defaults:
//   fontFamily = "sans-serif", fontSize = 14, fontWeight = 400,
//   fontItalic = 0, letterSpacing = 0, wordSpacing = 0.
//
// `text-decoration` (underline / strikethrough / overline) is intentionally
// NOT in this struct — it is a separate paint op in KUI's internal model.
// Use `dlDrawDecorationLine` to draw decoration lines independently.
// ===========================================================================
typedef struct {
    const char* fontFamily;     // null = "sans-serif"
    float       fontSize;       // px; 0 = 14
    uint16_t    fontWeight;     // 0 = 400 (normal); 700 = bold
    uint8_t     fontItalic;     // 0 = upright, 1 = italic
    float       letterSpacing;  // px between characters (CSS letter-spacing)
    float       wordSpacing;    // px between words     (CSS word-spacing)
} KuiExtTextStyle;

// Decoration line style — values MUST match KUI internal
// `PaintOp::DecorationLineData::style` enum (see display_list.h:172).
typedef enum {
    KUI_DECO_STYLE_SOLID  = 0,
    KUI_DECO_STYLE_DOUBLE = 1,
    KUI_DECO_STYLE_DOTTED = 2,
    KUI_DECO_STYLE_DASHED = 3,
    KUI_DECO_STYLE_WAVY   = 4,  // squiggle, used by diagnostic underlines
} KuiDecorationStyle;

// ===========================================================================
// Input events (HOST-010, v7) — routed to extension custom elements
//
// All event callbacks return `int`:
//   0 = event NOT consumed by extension; host continues default DOM dispatch
//   1 = event consumed; host skips its default handling (e.g. don't open
//       context menu on right-click, don't fire native KeyboardEvent)
//
// Coordinates in mouse/wheel events are in **element content-box pixels**
// (host has already subtracted absoluteContentRect origin), matching the
// painting coordinate convention.  modifiers is a bitmask:
//   bit 0 = Ctrl, bit 1 = Shift, bit 2 = Alt, bit 3 = Meta/Super
// ===========================================================================

// Modifier bitmask
#define KUI_MOD_CTRL  0x1
#define KUI_MOD_SHIFT 0x2
#define KUI_MOD_ALT   0x4
#define KUI_MOD_META  0x8

// ---- Keyboard event ----
typedef enum {
    KUI_KEY_DOWN = 0,
    KUI_KEY_UP   = 1,
} KuiKeyEventType;

typedef struct {
    int      eventType;     // KuiKeyEventType (Down/Up)
    // `key` and `code` follow the W3C UI Events naming:
    //   key   — character / name (e.g. "a", "ArrowDown", "Enter", "F1")
    //   code  — physical key code (e.g. "KeyA", "ArrowDown")
    // Both null-terminated UTF-8, valid for the callback scope only.
    const char* key;
    const char* code;
    int      modifiers;     // bitmask, see KUI_MOD_*
    int      isRepeat;      // 0 = first press, 1 = OS repeat
} KuiExtKeyEvent;

typedef int (*KuiExtKeyEventFn)(void* element, const KuiExtKeyEvent* ev);

// ---- Mouse event ----
typedef enum {
    KUI_MOUSE_DOWN  = 0,
    KUI_MOUSE_UP    = 1,
    KUI_MOUSE_MOVE  = 2,
    KUI_MOUSE_ENTER = 3,
    KUI_MOUSE_LEAVE = 4,
} KuiMouseEventType;

typedef enum {
    KUI_MBTN_NONE   = -1,
    KUI_MBTN_LEFT   = 0,
    KUI_MBTN_MIDDLE = 1,
    KUI_MBTN_RIGHT  = 2,
} KuiMouseButton;

typedef struct {
    int      eventType;     // KuiMouseEventType
    float    x, y;          // element content-box coords (CSS px)
    int      button;        // KuiMouseButton (Down/Up only; -1 for Move/Enter/Leave)
    int      buttons;       // bitmask of currently-pressed buttons
    int      modifiers;     // KUI_MOD_*
    int      clickCount;    // 1 = single, 2 = double, 3 = triple (Down only)
} KuiExtMouseEvent;

typedef int (*KuiExtMouseEventFn)(void* element, const KuiExtMouseEvent* ev);

// ---- Wheel event ----
typedef struct {
    float    x, y;          // element content-box coords
    float    deltaX;        // horizontal scroll (CSS px, +right)
    float    deltaY;        // vertical scroll (CSS px, +down)
    int      modifiers;
    int      momentum;      // 1 = inertial / trackpad coast, 0 = active wheel
} KuiExtWheelEvent;

typedef int (*KuiExtWheelEventFn)(void* element, const KuiExtWheelEvent* ev);

// ---- IME composition event ----
typedef enum {
    KUI_IME_START  = 0,    // composition start (text empty)
    KUI_IME_UPDATE = 1,    // composition string changed (preview text)
    KUI_IME_COMMIT = 2,    // user confirmed; committedText is final
    KUI_IME_CANCEL = 3,    // composition cancelled (Esc / focus lost)
} KuiImeEventType;

typedef struct {
    int         eventType;       // KuiImeEventType
    const char* text;            // current composition string (UTF-8)
                                 // - START / CANCEL: ""
                                 // - UPDATE: preview text (e.g. "zhong" while typing pinyin)
                                 // - COMMIT: also "" (use committedText)
    int         caretPos;        // byte offset within `text` of insertion caret (UPDATE only)
    const char* committedText;   // final inserted text (COMMIT only; "" otherwise)
} KuiExtImeEvent;

typedef int (*KuiExtImeEventFn)(void* element, const KuiExtImeEvent* ev);

// ===========================================================================
// Callback function types
// ===========================================================================
typedef void (*KuiExtConnectedFn)(void* element);
typedef void (*KuiExtDisconnectedFn)(void* element);
typedef void (*KuiExtAttributeChangedFn)(void* element, const char* name,
                                          const char* oldVal, const char* newVal);
typedef void (*KuiExtPaintFn)(KuiExtPaintContext* ctx);
typedef void (*KuiExtTickFn)(void* element, double dt);
typedef int  (*KuiExtNeedsTickFn)(void* element);

typedef struct { float width; float height; } KuiExtSize;
typedef KuiExtSize (*KuiExtIntrinsicSizeFn)(void* element, float availableWidth);

// ===========================================================================
// Typed value system — for Lua binding marshalling (main channel)
// ===========================================================================
typedef enum {
    KUI_VAL_NIL = 0,
    KUI_VAL_INT,          // 64-bit integer
    KUI_VAL_FLOAT,        // 64-bit float
    KUI_VAL_STRING,       // null-terminated UTF-8 (lifetime: callback scope only)
    KUI_VAL_BOOL,         // 0=false, non-zero=true
} KuiValueType;

typedef struct {
    KuiValueType type;
    union {
        int64_t     intVal;
        double      floatVal;
        const char* strVal;
        int         boolVal;
    };
} KuiValue;

// Extension method callback signature (main channel — zero Lua dependency)
typedef int (*KuiExtMethodCallback)(
    void*           element,
    int             argc,
    const KuiValue* argv,
    KuiValue*       results,
    int             maxResults
);

// Extension method definition
typedef struct {
    const char*           methodName;
    KuiExtMethodCallback  callback;
    int                   minArgs;
    int                   maxArgs;   // -1 = unlimited
} KuiExtLuaMethodDef;

// ===========================================================================
// Extension messaging — inter-extension communication
// ===========================================================================
typedef struct {
    const char* sourceTag;     // sender extension tag (e.g. "kui-pdf")
    const char* targetTag;     // receiver tag, or "*" for broadcast
    const char* messageType;   // application-defined message type
    const KuiValue* payload;   // typed payload values
    int             payloadCount;
} KuiExtMessage;

// Message handler callback — called when a message is received
typedef void (*KuiExtMessageFn)(const KuiExtMessage* msg);

// ===========================================================================
// Registration info (returned by kuiext_register)
// ===========================================================================
typedef struct {
    KuiExtPluginInfo         info;
    KuiExtConnectedFn        onConnected;
    KuiExtDisconnectedFn     onDisconnected;
    KuiExtAttributeChangedFn onAttributeChanged;
    KuiExtPaintFn            onPaint;
    KuiExtTickFn             onTick;
    KuiExtNeedsTickFn        onNeedsTick;
    KuiExtIntrinsicSizeFn    onIntrinsicSize;

    // Main channel: typed Lua method registration (recommended, zero Lua dependency)
    int                        luaMethodCount;
    const KuiExtLuaMethodDef*  luaMethods;

    // Advanced channel: raw lua_State* access (optional, requires lua54.dll)
    void (*onRegisterLua)(void* luaState);

    // Inter-extension messaging callback (optional)
    KuiExtMessageFn onMessage;

    // -- HOST-010 (v7) input event callbacks (all optional) --
    // Return 1 from these to consume the event and skip host's default
    // handling.  Return 0 (or leave callback null) to let the host
    // continue with normal DOM event dispatch.
    //
    // Coordinates are already in element content-box space (host has
    // subtracted absoluteContentRect origin).
    KuiExtKeyEventFn   onKeyEvent;     // KeyDown + KeyUp
    KuiExtMouseEventFn onMouseEvent;   // Down/Up/Move/Enter/Leave
    KuiExtWheelEventFn onWheelEvent;   // mouse wheel / trackpad
    KuiExtImeEventFn   onImeEvent;     // IME composition Start/Update/Commit/Cancel
} KuiExtRegistration;

// ===========================================================================
// Host API — function pointers provided by KUI to the extension DLL
// ===========================================================================
typedef struct {
    uint32_t apiVersion;  // Current: 11 (v11 adds GPU interop; HOST-020)

    // Element attribute operations
    const char* (*getAttribute)(void* el, const char* name);
    void  (*setAttribute)(void* el, const char* name, const char* value);
    void  (*removeAttribute)(void* el, const char* name);
    const char* (*getTagName)(void* el);
    const char* (*getId)(void* el);

    // Pixel buffer (format specifies input format, auto-converts internally)
    void  (*setPixelBuffer)(void* el, const uint32_t* pixels,
                            uint32_t w, uint32_t h, KuiPixelFormat format);
    void  (*clearPixelBuffer)(void* el);

    // Repaint scheduling
    void  (*scheduleRepaint)(void* el);

    // -- HOST-008 (v6) — Partial repaint hint --
    //
    // Mark a sub-rectangle (in element content-box coordinates) dirty.
    // The next paint will Skia-clip to the union of all dirty rects and
    // the Painter will skip subtrees outside the clip — saving ~10× CPU
    // vs `scheduleRepaint(el)` which re-paints the whole element.
    //
    // Coordinates:
    //   (x, y) are relative to the element's content box top-left
    //   (the same coordinate space as `KuiExtPaintContext.contentWidth/Height`
    //   and the host's `dlDrawText` / `dlFillRect` etc.).  The host
    //   converts to viewport coords internally via `absoluteContentRect()`.
    //
    // Behavior:
    //   * Silently no-op if (w <= 0 || h <= 0 || el == null).
    //   * If `el` has no layout box yet (e.g. before first layout pass),
    //     falls back to full repaint of the document.
    //   * Calls `Document::scheduleRepaint` internally — no need to
    //     additionally call scheduleRepaint.
    //
    // Editor use cases (saves O(viewport) → O(line) repaint):
    //   * caret blink:        invalidateRect(caretX, caretLineY, 2, lineH)
    //   * single-line edit:   invalidateRect(0, editLineY, contentW, lineH)
    //   * selection drag:     invalidateRect(0, selMinY, contentW, selHeight)
    void  (*invalidateRect)(void* el, float x, float y, float w, float h);

    // DisplayList API (only valid inside onPaint callback)
    void  (*dlFillRect)(void* dl, float x, float y, float w, float h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void  (*dlDrawPixelBuffer)(void* dl, const uint32_t* pixels, uint32_t pw, uint32_t ph,
                                float dx, float dy, float dw, float dh);
    void  (*dlDrawText)(void* dl, const char* text, float x, float y,
                        const char* font, float fontSize,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Logging
    void  (*logInfo)(const char* msg);
    void  (*logWarn)(const char* msg);
    void  (*logError)(const char* msg);

    // Query
    float (*getContentWidth)(void* el);
    float (*getContentHeight)(void* el);

    // -- HOST-015 (v2) --
    // Device pixel ratio query, available outside paint callback.
    // Inside paint, prefer KuiExtPaintContext.dpr (cached for the frame).
    // Returns 1.0 if no render backend is attached (e.g. headless tests).
    float (*getDpr)(void* el);

    // -- HOST-001a / HOST-001c (v3) — Text measurement --
    //
    // `measureText`: shape `text[0..textByteLen)` with the given font and
    // return advance width + vertical metrics in CSS px.  Uses the host's
    // HarfBuzz + Skia pipeline (same path the host itself uses for native
    // <input>/<textarea> caret positioning), so widths are exact-match with
    // what the host would render — caret coordinates align to within 1
    // device pixel.
    //
    // Parameters:
    //   el           — element (used to look up the active RenderBackend)
    //   text         — UTF-8 buffer, NOT required to be null-terminated
    //   textByteLen  — byte length; if 0, `strlen(text)` is used
    //   fontFamily   — CSS font-family list (e.g. "Segoe UI, sans-serif")
    //   fontSize     — px (NOT pt, NOT em)
    //   fontWeight   — CSS weight (100..900; 400 = normal, 700 = bold)
    //   fontItalic   — 0 = upright, 1 = italic
    //   outMetrics   — out param (must be non-null)
    //
    // Returns 0 on success, negative on error:
    //   -1  el / outMetrics is null
    //   -2  no render backend attached
    //   -3  font lookup failed (returned metrics fall back to size-derived
    //       estimate; width still computed via byte-count heuristic)
    int (*measureText)(void* el,
                       const char* text, size_t textByteLen,
                       const char* fontFamily, float fontSize,
                       uint16_t fontWeight, uint8_t fontItalic,
                       KuiExtTextMetrics* outMetrics);

    // `getFontMetrics`: vertical metrics for a font (no text shaping).
    // Faster than `measureText("",...)` because it skips HarfBuzz shaping.
    // `outMetrics->width` is always set to 0.
    int (*getFontMetrics)(void* el,
                          const char* fontFamily, float fontSize,
                          uint16_t fontWeight, uint8_t fontItalic,
                          KuiExtTextMetrics* outMetrics);

    // -- HOST-001b (v4) — Rich text drawing --
    //
    // `dlDrawTextStyled`: draw `text[0..textByteLen)` at (x, y) with full
    // style control (weight, italic, letter/word spacing).  Position (x, y)
    // is the top-left of the text's line box in element-local CSS pixels (NOT the baseline:
    // the glyphs hang below y and the baseline lies at y + ascent).
    //
    // Parameters:
    //   dl           — DisplayList* (only valid inside onPaint)
    //   text         — UTF-8 buffer
    //   textByteLen  — byte length; 0 → strlen(text)
    //   x, y         — line-box top-left (px, element-local); baseline = y + ascent
    //   style        — pointer to KuiExtTextStyle; null → default style
    //                  (sans-serif/14/400/upright)
    //   r,g,b,a      — text color (0-255 each)
    //
    // For unstyled text use the legacy `dlDrawText` (still available).
    void (*dlDrawTextStyled)(void* dl,
                             const char* text, size_t textByteLen,
                             float x, float y,
                             const KuiExtTextStyle* style,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // `dlDrawDecorationLine`: draw a text decoration line (underline,
    // strikethrough, overline, squiggle).  Position (x, y) is the line's
    // top-left corner; `width` is the horizontal extent.
    //
    // For underline: y = textY + ascent + 1 (the baseline plus a small offset; or
    // whatever offset the ext wants).  Thickness is independent; CSS default is ~max(1, em/16).
    //
    // To compose `text-decoration: underline wavy red` you typically:
    //   1. dlDrawTextStyled(...)
    //   2. dlDrawDecorationLine(x, textY + ascent + 1, width, 1.5,
    //                           KUI_DECO_STYLE_WAVY, 255, 0, 0, 255);
    void (*dlDrawDecorationLine)(void* dl,
                                 float x, float y, float width, float thickness,
                                 KuiDecorationStyle style,
                                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // -- HOST-017 (v5) — Focus management --
    //
    // KUI's EventDispatcher tracks one focused element per Document and
    // dispatches `focus` / `blur` / `focusin` / `focusout` events through
    // the standard DOM event path.  Extension elements participate by
    // setting `tabindex="0"` (so mouse clicks and Tab navigation reach
    // them) and using the three calls below for programmatic control.
    //
    // For mouse cursor shape: set the element's inline style instead
    // (e.g. `setAttribute(el, "style", "cursor: text")`), which routes
    // through KUI's normal CSS cursor path (WM_SETCURSOR → IDC_IBEAM).
    //
    // Focus change callbacks: not in v5 — extension elements receive
    // `focus` / `blur` events via the DOM event system (which currently
    // only the Lua/JS scripting layer can subscribe to); a C ABI callback
    // hook will arrive with HOST-010 (input event routing).

    // requestFocus: programmatically grab keyboard focus.
    // Returns 0 on success, -1 if `el` is null, -2 if focus was rejected
    // (e.g. element is disabled).  After return, call `hasFocus(el)` to
    // confirm — `EventDispatcher::handleFocus` silently rejects disabled
    // elements without surfacing an error code.
    int (*requestFocus)(void* el);

    // hasFocus: returns 1 if `el` currently owns keyboard focus, 0 otherwise.
    int (*hasFocus)(void* el);

    // blur: drop focus (no-op if `el` doesn't currently own focus).
    // Fires `blur` and `focusout` events.
    void (*blur)(void* el);

    // -- HOST-013 (v7) — IME composition support --
    //
    // KUI's existing IME pipeline (`@/.../src/platform/window_win32.cpp:745+`)
    // routes WM_IME_STARTCOMPOSITION / WM_IME_COMPOSITION through the
    // EventDispatcher into the focused HTMLInputElement.  Extension
    // elements participate by:
    //
    //   1. Setting `tabindex="0"` and acquiring focus (HOST-017)
    //   2. Calling `setImeCandidatePos` whenever caret moves so the IME
    //      candidate popup (where Chinese/Japanese show pinyin choices)
    //      follows the caret
    //   3. Reading composition text via existing event paths (留 HOST-010)
    //
    // For caret COLOR: use `dlFillRect` with whatever color you like —
    // no host API needed.  KUI's CSS `caret-color` is only consumed by
    // built-in `<input>` / `<textarea>`.

    // setImeCandidatePos: move IME candidate popup to (x, y) in element
    // content-box coordinates.  Host converts to viewport via
    // `absoluteContentRect()` and calls `Window::setImeCandidatePos`
    // (Windows: ImmSetCompositionWindow CFS_POINT).
    //
    // Typical call site: in your `onKeyEvent` / `setValue` etc., AFTER
    // updating caret, call:
    //   api->setImeCandidatePos(el, caretX, caretY + lineHeight);
    //
    // Silently no-op on platforms without IME support (macOS / Linux
    // stubs).  Silently no-op if `el` has no layout box yet.
    void (*setImeCandidatePos)(void* el, float x, float y);

    // isImeComposing: returns 1 if the element's window is currently in
    // IME composition mode (user is typing pinyin / hiragana etc.,
    // candidate popup is showing).  Useful for extensions that want to
    // hide the caret or show a "composing..." indicator during IME.
    int (*isImeComposing)(void* el);

    // Lua shared library availability query
    int (*isLuaSharedAvailable)(void);

    // Runtime dynamic method registration (main channel extension)
    void (*registerElementMethod)(const char* tagName, const char* methodName,
                                  KuiExtMethodCallback callback, int minArgs, int maxArgs);
    void (*unregisterElementMethod)(const char* tagName, const char* methodName);

    // Inter-extension messaging
    // targetTag: specific tag name, or "*" for broadcast to all extensions.
    // Returns 0 on success, negative on error.
    int (*sendMessage)(const char* sourceTag, const char* targetTag,
                       const char* messageType,
                       const KuiValue* payload, int payloadCount);

    // -----------------------------------------------------------------------
    // HOST-007a v9: clipboard text access for extensions implementing
    // copy/cut/paste (Ctrl+C/X/V).  Plain UTF-8 only; rich content (HTML,
    // images) is NOT supported by this API — the host wraps the platform
    // native text-only clipboard channel.
    //
    // Lifetime contract for `getClipboardText`:
    //   The returned pointer points to a thread-local buffer owned by the
    //   host.  It is valid ONLY until the next call to `getClipboardText`
    //   on the same thread.  Extensions MUST copy the bytes immediately if
    //   they need to retain them across any other host API calls.
    //
    //   `*outLen` (when non-NULL) receives the byte length, NOT including
    //   any optional trailing NUL terminator that the host may append for
    //   convenience.  Extensions should use `*outLen` rather than strlen
    //   when the text may contain embedded NUL bytes (rare but possible).
    //
    //   Returns NULL when the clipboard contains no text or the platform
    //   call failed (denied access, lock contention, etc.).  `*outLen` is
    //   set to 0 in that case.
    //
    // For `setClipboardText`:
    //   `len` is the byte count.  Pass `len = -1` to use strlen-style
    //   auto-detection.  Pass `len = 0` (or `text = NULL`) to clear the
    //   clipboard.  The host normalises line endings to platform native
    //   on paste-out (CRLF on Windows, LF on macOS/Linux) but stores the
    //   text as-given internally.
    //
    // Both functions silently no-op on platforms that lack a clipboard.
    // -----------------------------------------------------------------------
    const char* (*getClipboardText)(int* outLen);
    void        (*setClipboardText)(const char* utf8, int len);

    // -----------------------------------------------------------------------
    // HOST-018 v10 (KUI-036): dispatch an InputEvent to an extension element
    // through the host EventDispatcher.  The host constructs a real
    // `InputEvent(type, data, inputType)` and dispatches it along the normal
    // capture -> target -> bubble path, so Lua/JS listeners registered via
    // `element:addEventListener(type, fn)` fire with `e.data` / `e.inputType`
    // readable (the same event surface the host's <input>/<textarea> use).
    //
    // Editor use cases (KUI-036, <kui-editor> autocomplete ergonomics):
    //   * content change:  dispatchInputEvent(el, "input", insertedText,
    //                         "insertText" | "deleteContentBackward" | ...)
    //   * ghost accepted:  dispatchInputEvent(el, "ghostaccept", text,
    //                         "full" | "partial")
    //   * ghost dismissed: dispatchInputEvent(el, "ghostdismiss", "",
    //                         "escape" | "typing" | "caretmove")
    //
    //   el        - extension element (host casts to Element*)
    //   type      - DOM event type, null-terminated UTF-8 (must be non-null)
    //   data      - InputEvent.data payload; null is treated as "" (empty)
    //   inputType - InputEvent.inputType; null is treated as "insertText"
    //
    // Silently no-op when el / type is null or the element has no owner
    // Document.  Event bubbles + is cancelable (a listener may preventDefault,
    // though callers currently ignore the dispatch result).
    void (*dispatchInputEvent)(void* el, const char* type,
                               const char* data, const char* inputType);

    // -----------------------------------------------------------------------
    // HOST-020 v11 (2026-07-25): GPU interop.
    //
    // Lets a GPU-rendering extension (e.g. a 3D engine using Filament, or
    // any other self-rendering GPU content) composite into KUI without a
    // GPU->CPU->GPU readback round-trip.  Mirrors the abstraction KUI's own
    // <canvas> WebGL path already uses internally
    // (`RenderBackend::nativeGLContext()/nativeGLDC()/nativeGpuContext()/
    // borrowGLTextureAsImage()/markExternalGpuDirty()`,
    // `include/kui/render/render_backend.h`) -- this group is a thin C ABI
    // passthrough of that existing, already-validated abstraction, not a new
    // design.  Gated by `KUI_CAP_GPU_INTEROP`; extensions that don't declare
    // this capability should not call these (behavior on an incompatible
    // host is a null return / silent no-op, same convention as the rest of
    // this API -- but a host that doesn't grant the capability may also
    // simply not populate these fields on older builds; always null-check
    // before calling).
    //
    // nativeGLContext / nativeGLDC / nativeGpuContext: native GPU handle
    // query, returns nullptr if no GPU backend is attached (e.g. headless
    // test, or a non-GL render backend) or capability was not granted.  The
    // concrete pointer type is platform/backend-dependent -- see
    // `RenderBackend::nativeGLContext()` doc comment for the exact mapping
    // (HGLRC/GLXContext/NSOpenGLContext*/EGLContext etc).
    void* (*nativeGLContext)(void* el);
    void* (*nativeGLDC)(void* el);
    void* (*nativeGpuContext)(void* el);

    // markExternalGpuDirty: call after the extension has mutated host GL
    // global state (blend mode, bound FBO, etc.) while rendering into a
    // shared GL context, so the host knows to restore its own GL state
    // before its next paint.  No-op if `el` has no attached GPU backend.
    void (*markExternalGpuDirty)(void* el);

    // borrowGLTextureAsImage: zero-copy wrap an externally-rendered GL
    // texture (owned and kept alive by the extension) as an opaque host
    // image handle.  Returns nullptr on failure (no GPU backend attached,
    // capability not granted, or the backend doesn't support texture
    // borrowing).  The returned handle must be released via
    // `releaseImageHandle` when no longer needed -- it does NOT take
    // ownership of the GL texture itself, which remains the extension's
    // responsibility to keep alive for the handle's lifetime.
    void* (*borrowGLTextureAsImage)(void* el, uint32_t glTextureId,
                                    uint32_t width, uint32_t height);

    // imageHandleNativeImage: extract the backend-specific raw pointer
    // (e.g. `SkImage*`) from a handle returned by `borrowGLTextureAsImage`,
    // for use with `dlDrawCanvasSurface`.  Returns nullptr if `handle` is
    // null.
    void* (*imageHandleNativeImage)(void* handle);

    // releaseImageHandle: release a handle returned by
    // `borrowGLTextureAsImage`.  Safe to call with nullptr (no-op).  Does
    // NOT touch the underlying GL texture -- only releases the host-side
    // wrapper object.
    void  (*releaseImageHandle)(void* handle);

    // dlDrawCanvasSurface: draw a native image pointer (from
    // `imageHandleNativeImage`) into the DisplayList at `(dx, dy, dw, dh)`
    // (element-local CSS pixels, same coordinate space as `dlFillRect`).
    // Only valid inside the `onPaint` callback, same restriction as the
    // other `dl*` functions.  This is the same path KUI's own <canvas>
    // WebGL snapshot compositing uses internally
    // (`Painter::paintCanvasElement()` -> `DisplayList::drawCanvasSurface()`).
    void  (*dlDrawCanvasSurface)(void* dl, void* nativeImagePtr,
                                 float dx, float dy, float dw, float dh);
} KuiExtHostAPI;

// ===========================================================================
// DLL export function types
// ===========================================================================
typedef KuiExtPluginInfo   (*KuiExtPluginInfoFn)(void);
typedef KuiExtError        (*KuiExtInitFn)(const KuiExtHostAPI* api);
typedef KuiExtRegistration (*KuiExtRegisterFn)(void);
typedef void               (*KuiExtShutdownFn)(void);
typedef const char*        (*KuiExtGetErrorFn)(void);
// IMPL-CR-008: optional export — host treats absence as legacy v0 / 0-cap.
typedef KuiExtCompatInfo   (*KuiExtApiCompatFn)(void);

// ===========================================================================
// v1.11 (Q7): Repaint contract — optional export `kuiext_paintContract`
//
//   KUI_EXT_API uint32_t kuiext_paintContract(void);
//
// Returns a bitmask of KUI_EXT_PAINT_CONTRACT_* flags for ALL element types
// this extension registers.  Absent export (or 0) = no contract = legacy
// behaviour.  Normative spec: sdk docs/extension_repaint_contract.md.
//
// KUI_EXT_PAINT_CONTRACT_REPAINT_ON_CHANGE — the extension guarantees that:
//   1. every state change that alters what onPaint draws (input callbacks,
//      onTick, Lua methods, messages, attribute changes, results from worker
//      threads ...) is followed — on the host UI thread, before control
//      returns to the host — by scheduleRepaint(el) or by invalidateRect(el,
//      ...) covering the union of the old and the new pixels;
//   2. onPaint output depends only on the element's own state and attributes
//      (external state may be read only if every change of it triggers 1.);
//   3. onPaint draws only inside the element's border box (the host clips
//      declared elements to it);
//   4. onPaint does not mutate DOM / styles / attributes;
//   5. focus gain / loss that changes the painting (caret, focus highlight)
//      is handled by the extension itself (e.g. tracked in onTick) under 1.
// In exchange the host stops treating the element as an unknown repaint
// source: hover / focus / typing elsewhere on the page are no longer forced
// to full-page re-recording, and invalidateRect becomes exact damage.
// ===========================================================================
#define KUI_EXT_PAINT_CONTRACT_REPAINT_ON_CHANGE 0x00000001u
typedef uint32_t           (*KuiExtPaintContractFn)(void);

#ifdef __cplusplus
}
#endif

#endif // KUI_EXTENSION_API_H
