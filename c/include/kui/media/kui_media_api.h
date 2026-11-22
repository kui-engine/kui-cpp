// ===========================================================================
// KUI Media Plugin — C ABI stable interface
//
// Analogous to kui_3d_api.h for 3D engine plugins.
// All functions use C linkage for ABI stability across compilers.
//
// Two independent DLLs:
//   kui_media_audio.dll  — pure audio (miniaudio backend)
//   kui_media_video.dll  — video + audio (FFmpeg + miniaudio backend)
//
// KUI main program does NOT link against miniaudio or FFmpeg.
// It only includes this header and loads DLLs at runtime via AudioAdapter /
// VideoAdapter (LoadLibrary / dlopen).
// ===========================================================================
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// DLL export/import macros
// ---------------------------------------------------------------------------
#if defined(_WIN32)
#  if defined(KUI_MEDIA_BUILDING_DLL)
#    define KUI_MEDIA_API __declspec(dllexport)
#  else
#    define KUI_MEDIA_API __declspec(dllimport)
#  endif
#else
#  define KUI_MEDIA_API __attribute__((visibility("default")))
#endif

// ---------------------------------------------------------------------------
// Error codes — all load/control functions return these
// ---------------------------------------------------------------------------
typedef enum {
    KUI_MEDIA_OK              =  0,
    KUI_MEDIA_ERR_FILE        = -1,  // file not found or cannot open
    KUI_MEDIA_ERR_CODEC       = -2,  // unsupported codec
    KUI_MEDIA_ERR_DECODE      = -3,  // decode error (corrupt stream)
    KUI_MEDIA_ERR_AUDIO       = -4,  // audio device error
    KUI_MEDIA_ERR_OOM         = -5,  // out of memory
    KUI_MEDIA_ERR_BUSY        = -6,  // resource busy or null handle
    KUI_MEDIA_ERR_TIMEOUT     = -7,  // operation timed out
    KUI_MEDIA_ERR_INVALID     = -8   // invalid argument
} KuiMediaError;

// ---------------------------------------------------------------------------
// Pixel format for video frames
// Reuse KuiPixelFormat from kui_extension_api.h if already defined;
// otherwise define compatible values here.
// ---------------------------------------------------------------------------
#ifndef KUI_PIXEL_FORMAT_DEFINED
#define KUI_PIXEL_FORMAT_DEFINED
typedef enum {
    KUI_PIXEL_RGBA_UNPREMUL = 0,  // R,G,B,A 8-bit each, non-premultiplied
    KUI_PIXEL_BGRA_PREMUL  = 1   // B,G,R,A 8-bit each, premultiplied (Skia native)
} KuiPixelFormat;
#endif

// ---------------------------------------------------------------------------
// Plugin info — every DLL must export kui_media_pluginInfo()
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t    major;
    uint32_t    minor;
    uint32_t    patch;
    const char* name;  // "kui_media_audio" or "kui_media_video"
} KuiMediaPluginInfo;

typedef KuiMediaPluginInfo (*KuiMediaPluginInfoFn)(void);

// Get human-readable description of last error for a player handle
typedef const char* (*KuiMediaGetErrorFn)(void* player);

// ---------------------------------------------------------------------------
// Optional: kui_media_shutdown()
//
// The host calls this immediately before it unloads the plugin, giving the
// plugin a chance to tear down process-wide state -- crucially, to join any
// thread it started. It is OPTIONAL for backwards compatibility: the host
// resolves the symbol and simply skips the call when a plugin does not export
// it, so plugins built against an older header keep working.
//
// Why this exists: dlclose() unmaps the plugin's code pages. A plugin that
// started a background thread (miniaudio spins up a CoreAudio/ALSA/WASAPI
// callback thread the first time an engine is initialised) leaves that thread
// running with its instruction pointer inside pages that no longer exist. The
// result is a crash that has nothing to do with the code that triggered it --
// on macOS it surfaced as SIGILL / SIGSEGV / SIGBUS, seemingly at random, in
// whatever the process happened to do next. Windows hid the bug because
// FreeLibrary runs DllMain(DLL_PROCESS_DETACH) first.
//
// Contract: after this returns, the plugin must have no live threads and must
// not touch any of its own globals again. Calling it twice must be safe.
typedef void (*KuiMediaShutdownFn)(void);

// ======================== Audio Plugin API ========================

typedef void* KuiAudioPlayer;  // opaque handle

// Lifecycle
typedef KuiAudioPlayer (*KuiAudioCreateFn)(void);
typedef void           (*KuiAudioDestroyFn)(KuiAudioPlayer player);

// File loading (returns KuiMediaError)
typedef KuiMediaError  (*KuiAudioLoadFn)(KuiAudioPlayer player, const char* path);

// Playback control
typedef void   (*KuiAudioPlayFn)(KuiAudioPlayer player);
typedef void   (*KuiAudioPauseFn)(KuiAudioPlayer player);
typedef void   (*KuiAudioStopFn)(KuiAudioPlayer player);

// State query
typedef int    (*KuiAudioIsPlayingFn)(KuiAudioPlayer player);
typedef int    (*KuiAudioIsEndedFn)(KuiAudioPlayer player);
typedef int    (*KuiAudioHasErrorFn)(KuiAudioPlayer player);

// Time
typedef double (*KuiAudioGetTimeFn)(KuiAudioPlayer player);
typedef void   (*KuiAudioSeekFn)(KuiAudioPlayer player, double seconds);
typedef double (*KuiAudioGetDurationFn)(KuiAudioPlayer player);

// Volume
typedef void   (*KuiAudioSetVolumeFn)(KuiAudioPlayer player, float vol);
typedef float  (*KuiAudioGetVolumeFn)(KuiAudioPlayer player);
typedef void   (*KuiAudioSetMutedFn)(KuiAudioPlayer player, int muted);
typedef int    (*KuiAudioIsMutedFn)(KuiAudioPlayer player);

// Looping
typedef void   (*KuiAudioSetLoopingFn)(KuiAudioPlayer player, int loop);
typedef int    (*KuiAudioIsLoopingFn)(KuiAudioPlayer player);

// Panning (-1.0 = full left, 0.0 = center, 1.0 = full right)
typedef void   (*KuiAudioSetPanFn)(KuiAudioPlayer player, float pan);
typedef float  (*KuiAudioGetPanFn)(KuiAudioPlayer player);

// Pitch (playback speed multiplier, 1.0 = normal)
typedef void   (*KuiAudioSetPitchFn)(KuiAudioPlayer player, float pitch);
typedef float  (*KuiAudioGetPitchFn)(KuiAudioPlayer player);

// Per-frame tick (called by main thread each frame)
typedef void   (*KuiAudioTickFn)(KuiAudioPlayer player, double dt);

// ======================== Video Plugin API ========================

typedef void* KuiVideoPlayer;  // opaque handle

// Lifecycle
typedef KuiVideoPlayer (*KuiVideoCreateFn)(void);
typedef void           (*KuiVideoDestroyFn)(KuiVideoPlayer player);

// File loading (returns KuiMediaError)
typedef KuiMediaError  (*KuiVideoLoadFn)(KuiVideoPlayer player, const char* path);

// Playback control
typedef void   (*KuiVideoPlayFn)(KuiVideoPlayer player);
typedef void   (*KuiVideoPauseFn)(KuiVideoPlayer player);
typedef void   (*KuiVideoStopFn)(KuiVideoPlayer player);

// State query
typedef int    (*KuiVideoIsPlayingFn)(KuiVideoPlayer player);
typedef int    (*KuiVideoIsEndedFn)(KuiVideoPlayer player);
typedef int    (*KuiVideoHasErrorFn)(KuiVideoPlayer player);

// Time
typedef double (*KuiVideoGetTimeFn)(KuiVideoPlayer player);
typedef void   (*KuiVideoSeekFn)(KuiVideoPlayer player, double seconds);
typedef double (*KuiVideoGetDurationFn)(KuiVideoPlayer player);

// Volume
typedef void   (*KuiVideoSetVolumeFn)(KuiVideoPlayer player, float vol);
typedef float  (*KuiVideoGetVolumeFn)(KuiVideoPlayer player);
typedef void   (*KuiVideoSetMutedFn)(KuiVideoPlayer player, int muted);
typedef int    (*KuiVideoIsMutedFn)(KuiVideoPlayer player);

// Looping
typedef void   (*KuiVideoSetLoopingFn)(KuiVideoPlayer player, int loop);
typedef int    (*KuiVideoIsLoopingFn)(KuiVideoPlayer player);

// Per-frame tick
typedef void   (*KuiVideoTickFn)(KuiVideoPlayer player, double dt);

// ---- Video-specific ----

// Frame dimensions
typedef int    (*KuiVideoGetWidthFn)(KuiVideoPlayer player);
typedef int    (*KuiVideoGetHeightFn)(KuiVideoPlayer player);

// Zero-copy frame info (triple-buffered inside DLL)
typedef struct {
    const uint8_t* data;   // read-only pointer, DLL manages lifetime
    int            width;
    int            height;
    int            stride;    // row byte count (may include padding)
    int            newFrame;  // 1 = new frame, 0 = same as last (reuse SkImage)
    KuiPixelFormat format;   // pixel format of data
} KuiVideoFrameInfo;

// Get current video frame (main thread only).
// Returns: 1 = info filled, 0 = no frame available.
// info.data pointer valid until next getFrame() or releaseFrame() call.
typedef int    (*KuiVideoGetFrameFn)(KuiVideoPlayer player,
                                     KuiVideoFrameInfo* info);

// Release frame — notify DLL that Skia has finished using the frame buffer.
// Must be called after Skia endFrame() completes.
typedef void   (*KuiVideoReleaseFrameFn)(KuiVideoPlayer player);

// Legacy copy mode (fallback when zero-copy is not feasible).
// outRGBA: caller-allocated w*h*4 byte buffer.
// Returns: 1 = new frame written, 0 = no new frame.
typedef int    (*KuiVideoReadFrameFn)(KuiVideoPlayer player,
                                      uint8_t* outRGBA, int w, int h);

#ifdef __cplusplus
} // extern "C"
#endif
