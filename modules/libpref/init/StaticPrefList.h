/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file defines static prefs, i.e. those that are defined at startup and
// used entirely or mostly from C++ code.
//
// If a pref is listed here and also in a prefs data file such as all.js, the
// value from the latter will override the value given here. For vanilla
// browser builds such overrides are discouraged, but they are necessary for
// some configurations (e.g. Thunderbird).
//
// The file is separated into sections, where the sections are determined by
// the first segment of the prefnames within (e.g. "network.predictor.enabled"
// is within the "Network" section). Sections should be kept in alphabetical
// order, but prefs within sections need not be.
//
// Normal prefs
// ------------
// Definitions of normal prefs in this file have the following form.
//
//   PREF(<pref-name-string>, <cpp-type>, <default-value>)
//
// - <pref-name-string> is the name of the pref, as it appears in about:config.
//   It is used in most libpref API functions (from both C++ and JS code).
//
// - <cpp-type> is one of bool, int32_t, float, or String (which is just a
//   typedef for `const char*` in StaticPrefs.h). Note that float prefs are
//   stored internally as strings.
//
// - <default-value> is the default value. Its type should match <cpp-type>.
//
// VarCache prefs
// --------------
// A VarCache pref is a special type of pref. It can be accessed via the normal
// pref hash table lookup functions, but it also has an associated global
// variable (the VarCache) that mirrors the pref value in the prefs hash table,
// and a getter function that reads that global variable. Using the getter to
// read the pref's value has the two following advantages over the normal API
// functions.
//
// - A direct global variable access is faster than a hash table lookup.
//
// - A global variable can be accessed off the main thread. If a pref *is*
//   accessed off the main thread, it should use an atomic type. (But note that
//   many VarCaches that should be atomic are not, in particular because
//   Atomic<float> is not available, alas.)
//
// Definitions of VarCache prefs in this file has the following form.
//
//   VARCACHE_PREF(
//     <pref-name-string>,
//     <pref-name-id>,
//     <cpp-type>, <default-value>
//   )
//
// - <pref-name-string> is the same as for normal prefs.
//
// - <pref-name-id> is the name of the static getter function generated within
//   the StaticPrefs class. For consistency, the identifier for every pref
//   should be created by starting with <pref-name-string> and converting any
//   '.' or '-' chars to '_'. For example, "foo.bar_baz" becomes
//   |foo_bar_baz|. This is arguably ugly, but clear, and you can search for
//   both using the regexp /foo.bar.baz/.
//
// - <cpp-type> is one of bool, int32_t, uint32_t, float, or an Atomic version
//   of one of those. The C++ preprocessor doesn't like template syntax in a
//   macro argument, so use the typedefs defines in StaticPrefs.h; for example,
//   use `ReleaseAcquireAtomicBool` instead of `Atomic<bool, ReleaseAcquire>`.
//
// - <default-value> is the same as for normal prefs.
//
// Note that Rust code must access the global variable directly, rather than via
// the getter.

// clang-format off

//---------------------------------------------------------------------------
// Accessibility prefs
//---------------------------------------------------------------------------

VARCACHE_PREF(
  "accessibility.monoaudio.enable",
   accessibility_monoaudio_enable,
  bool, false
)

//---------------------------------------------------------------------------
// DOM prefs
//---------------------------------------------------------------------------

VARCACHE_PREF(
  "dom.webcomponents.shadowdom.report_usage",
   dom_webcomponents_shadowdom_report_usage,
  bool, false
)

// Whether we disable triggering mutation events for changes to style
// attribute via CSSOM.
VARCACHE_PREF(
  "dom.mutation-events.cssom.disabled",
   dom_mutation_events_cssom_disabled,
  bool, true
)

//---------------------------------------------------------------------------
// Full-screen prefs
//---------------------------------------------------------------------------

#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "full-screen-api.unprefix.enabled",
   full_screen_api_unprefix_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

//---------------------------------------------------------------------------
// Graphics prefs
//---------------------------------------------------------------------------

VARCACHE_PREF(
  "gfx.font_rendering.opentype_svg.enabled",
   gfx_font_rendering_opentype_svg_enabled,
  bool, true
)

//---------------------------------------------------------------------------
// HTML5 parser prefs
//---------------------------------------------------------------------------

// Toggle which thread the HTML5 parser uses for stream parsing.
VARCACHE_PREF(
  "html5.offmainthread",
   html5_offmainthread,
  bool, true
)

// Time in milliseconds between the time a network buffer is seen and the timer
// firing when the timer hasn't fired previously in this parse in the
// off-the-main-thread HTML5 parser.
VARCACHE_PREF(
  "html5.flushtimer.initialdelay",
   html5_flushtimer_initialdelay,
  int32_t, 120
)

// Time in milliseconds between the time a network buffer is seen and the timer
// firing when the timer has already fired previously in this parse.
VARCACHE_PREF(
  "html5.flushtimer.subsequentdelay",
   html5_flushtimer_subsequentdelay,
  int32_t, 120
)

//---------------------------------------------------------------------------
// Layout prefs
//---------------------------------------------------------------------------

// Is parallel CSS parsing enabled?
VARCACHE_PREF(
  "layout.css.parsing.parallel",
   layout_css_parsing_parallel,
  bool, true
)

// Is support for the font-display @font-face descriptor enabled?
VARCACHE_PREF(
  "layout.css.font-display.enabled",
   layout_css_font_display_enabled,
  bool, true
)

// Are webkit-prefixed properties & property-values supported?
VARCACHE_PREF(
  "layout.css.prefixes.webkit",
   layout_css_prefixes_webkit,
  bool, true
)

// Are "-webkit-{min|max}-device-pixel-ratio" media queries supported? (Note:
// this pref has no effect if the master 'layout.css.prefixes.webkit' pref is
// set to false.)
VARCACHE_PREF(
  "layout.css.prefixes.device-pixel-ratio-webkit",
   layout_css_prefixes_device_pixel_ratio_webkit,
  bool, false
)

// Is -moz-prefixed gradient functions enabled?
VARCACHE_PREF(
  "layout.css.prefixes.gradients",
   layout_css_prefixes_gradients,
  bool, true
)

// Should stray control characters be rendered visibly?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "layout.css.control-characters.visible",
   layout_css_control_characters_visible,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for the frames() timing function enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "layout.css.frames-timing.enabled",
   layout_css_frames_timing_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Should the :visited selector ever match (otherwise :link matches instead)?
VARCACHE_PREF(
  "layout.css.visited_links_enabled",
   layout_css_visited_links_enabled,
  bool, true
)

// Pref to control whether @-moz-document rules are enabled in content pages.
VARCACHE_PREF(
  "layout.css.moz-document.content.enabled",
   layout_css_moz_document_content_enabled,
  bool, false
)

// Pref to control whether @-moz-document url-prefix() is parsed in content
// pages. Only effective when layout.css.moz-document.content.enabled is false.
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE false
#else
#define PREF_VALUE true
#endif
VARCACHE_PREF(
  "layout.css.moz-document.url-prefix-hack.enabled",
   layout_css_moz_document_url_prefix_hack_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for CSS "grid-template-{columns,rows}: subgrid X" enabled?
VARCACHE_PREF(
  "layout.css.grid-template-subgrid-value.enabled",
   layout_css_grid_template_subgrid_value_enabled,
  bool, false
)

// Is support for variation fonts enabled?
VARCACHE_PREF(
  "layout.css.font-variations.enabled",
   layout_css_font_variations_enabled,
  bool, true
)

// Are we emulating -moz-{inline}-box layout using CSS flexbox?
VARCACHE_PREF(
  "layout.css.emulate-moz-box-with-flex",
   layout_css_emulate_moz_box_with_flex,
  bool, false
)

//---------------------------------------------------------------------------
// JavaScript prefs
//---------------------------------------------------------------------------

// nsJSEnvironmentObserver observes the memory-pressure notifications and
// forces a garbage collection and cycle collection when it happens, if the
// appropriate pref is set.
#ifdef ANDROID
  // Disable the JS engine's GC on memory pressure, since we do one in the
  // mobile browser (bug 669346).
  // XXX: this value possibly should be changed, or the pref removed entirely.
  //      See bug 1450787.
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "javascript.options.gc_on_memory_pressure",
   javascript_options_gc_on_memory_pressure,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  "javascript.options.compact_on_user_inactive",
   javascript_options_compact_on_user_inactive,
  bool, true
)

// The default amount of time to wait from the user being idle to starting a
// shrinking GC.
#ifdef NIGHTLY_BUILD
# define PREF_VALUE  15000  // ms
#else
# define PREF_VALUE 300000  // ms
#endif
VARCACHE_PREF(
  "javascript.options.compact_on_user_inactive_delay",
   javascript_options_compact_on_user_inactive_delay,
   uint32_t, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  "javascript.options.mem.log",
   javascript_options_mem_log,
  bool, false
)

VARCACHE_PREF(
  "javascript.options.mem.notify",
   javascript_options_mem_notify,
  bool, false
)

//---------------------------------------------------------------------------
// Media prefs
//---------------------------------------------------------------------------

// These prefs use camel case instead of snake case for the getter because one
// reviewer had an unshakeable preference for that.

// File-backed MediaCache size.
#ifdef ANDROID
# define PREF_VALUE  32768  // Measured in KiB
#else
# define PREF_VALUE 512000  // Measured in KiB
#endif
VARCACHE_PREF(
  "media.cache_size",
   MediaCacheSize,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

// If a resource is known to be smaller than this size (in kilobytes), a
// memory-backed MediaCache may be used; otherwise the (single shared global)
// file-backed MediaCache is used.
VARCACHE_PREF(
  "media.memory_cache_max_size",
   MediaMemoryCacheMaxSize,
  uint32_t, 8192      // Measured in KiB
)

// Don't create more memory-backed MediaCaches if their combined size would go
// above this absolute size limit.
#ifdef ANDROID
# define PREF_VALUE  32768    // Measured in KiB
#else
# define PREF_VALUE 524288    // Measured in KiB
#endif
VARCACHE_PREF(
  "media.memory_caches_combined_limit_kb",
   MediaMemoryCachesCombinedLimitKb,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

// Don't create more memory-backed MediaCaches if their combined size would go
// above this relative size limit (a percentage of physical memory).
VARCACHE_PREF(
  "media.memory_caches_combined_limit_pc_sysmem",
   MediaMemoryCachesCombinedLimitPcSysmem,
  uint32_t, 5         // A percentage
)

// When a network connection is suspended, don't resume it until the amount of
// buffered data falls below this threshold (in seconds).
#ifdef ANDROID
# define PREF_VALUE 10  // Use a smaller limit to save battery.
#else
# define PREF_VALUE 30
#endif
VARCACHE_PREF(
  "media.cache_resume_threshold",
   MediaCacheResumeThreshold,
  int32_t, PREF_VALUE
)
#undef PREF_VALUE

// Stop reading ahead when our buffered data is this many seconds ahead of the
// current playback position. This limit can stop us from using arbitrary
// amounts of network bandwidth prefetching huge videos.
#ifdef ANDROID
# define PREF_VALUE 30  // Use a smaller limit to save battery.
#else
# define PREF_VALUE 60
#endif
VARCACHE_PREF(
  "media.cache_readahead_limit",
   MediaCacheReadaheadLimit,
  int32_t, PREF_VALUE
)
#undef PREF_VALUE

// AudioSink
VARCACHE_PREF(
  "media.resampling.enabled",
   MediaResamplingEnabled,
  bool, false
)

#if defined(XP_WIN) || defined(XP_DARWIN) || defined(MOZ_PULSEAUDIO)
// libcubeb backend implement .get_preferred_channel_layout
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "media.forcestereo.enabled",
   MediaForcestereoEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// VideoSink
VARCACHE_PREF(
  "media.ruin-av-sync.enabled",
   MediaRuinAvSyncEnabled,
  bool, false
)

// Encrypted Media Extensions
#if defined(ANDROID)
# if defined(NIGHTLY_BUILD)
#  define PREF_VALUE true
# else
#  define PREF_VALUE false
# endif
#elif defined(XP_LINUX)
  // On Linux EME is visible but disabled by default. This is so that the "Play
  // DRM content" checkbox in the Firefox UI is unchecked by default. DRM
  // requires downloading and installing proprietary binaries, which users on
  // an open source operating systems didn't opt into. The first time a site
  // using EME is encountered, the user will be prompted to enable DRM,
  // whereupon the EME plugin binaries will be downloaded if permission is
  // granted.
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  "media.eme.enabled",
   MediaEmeEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  "media.clearkey.persistent-license.enabled",
   MediaClearkeyPersistentLicenseEnabled,
  bool, false
)

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
// Whether to allow, on a Linux system that doesn't support the necessary
// sandboxing features, loading Gecko Media Plugins unsandboxed.  However, EME
// CDMs will not be loaded without sandboxing even if this pref is changed.
VARCACHE_PREF(
  "media.gmp.insecure.allow",
   MediaGmpInsecureAllow,
  bool, false
)
#endif

// Specifies whether the PDMFactory can create a test decoder that just outputs
// blank frames/audio instead of actually decoding. The blank decoder works on
// all platforms.
VARCACHE_PREF(
  "media.use-blank-decoder",
   MediaUseBlankDecoder,
  bool, false
)

#if defined(XP_WIN)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  "media.gpu-process-decoder",
   MediaGpuProcessDecoder,
  bool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef ANDROID

// Enable the MediaCodec PlatformDecoderModule by default.
VARCACHE_PREF(
  "media.android-media-codec.enabled",
   MediaAndroidMediaCodecEnabled,
  bool, true
)

VARCACHE_PREF(
  "media.android-media-codec.preferred",
   MediaAndroidMediaCodecPreferred,
  bool, true
)

#endif // ANDROID

// WebRTC
#ifdef MOZ_WEBRTC
#ifdef ANDROID

VARCACHE_PREF(
  "media.navigator.hardware.vp8_encode.acceleration_remote_enabled",
   MediaNavigatorHardwareVp8encodeAccelerationRemoteEnabled,
  bool, true
)

PREF("media.navigator.hardware.vp8_encode.acceleration_enabled", bool, true)

PREF("media.navigator.hardware.vp8_decode.acceleration_enabled", bool, false)

#endif // ANDROID

// Use MediaDataDecoder API for WebRTC. This includes hardware acceleration for
// decoding.
VARCACHE_PREF(
  "media.navigator.mediadatadecoder_enabled",
   MediaNavigatorMediadatadecoderEnabled,
  bool, false
)
#endif // MOZ_WEBRTC

#ifdef MOZ_OMX
VARCACHE_PREF(
  "media.omx.enabled",
   MediaOmxEnabled,
  bool, false
)
#endif

#ifdef MOZ_FFMPEG

# if defined(XP_MACOSX)
#  define PREF_VALUE false
# else
#  define PREF_VALUE true
# endif
VARCACHE_PREF(
  "media.ffmpeg.enabled",
   MediaFfmpegEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  "media.libavcodec.allow-obsolete",
   MediaLibavcodecAllowObsolete,
  bool, false
)

#endif // MOZ_FFMPEG

#ifdef MOZ_FFVPX
VARCACHE_PREF(
  "media.ffvpx.enabled",
   MediaFfvpxEnabled,
  bool, true
)
#endif

#if defined(MOZ_FFMPEG) || defined(MOZ_FFVPX)
VARCACHE_PREF(
  "media.ffmpeg.low-latency.enabled",
   MediaFfmpegLowLatencyEnabled,
  bool, false
)
#endif

#ifdef MOZ_WMF

VARCACHE_PREF(
  "media.wmf.enabled",
   MediaWmfEnabled,
  bool, true
)

// Whether DD should consider WMF-disabled a WMF failure, useful for testing.
VARCACHE_PREF(
  "media.decoder-doctor.wmf-disabled-is-failure",
   MediaDecoderDoctorWmfDisabledIsFailure,
  bool, false
)

VARCACHE_PREF(
  "media.wmf.vp9.enabled",
   MediaWmfVp9Enabled,
  bool, true
)

#endif // MOZ_WMF

// Whether to check the decoder supports recycling.
#ifdef ANDROID
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  "media.decoder.recycle.enabled",
   MediaDecoderRecycleEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Should MFR try to skip to the next key frame?
VARCACHE_PREF(
  "media.decoder.skip-to-next-key-frame.enabled",
   MediaDecoderSkipToNextKeyFrameEnabled,
  bool, true
)

VARCACHE_PREF(
  "media.gmp.decoder.enabled",
   MediaGmpDecoderEnabled,
  bool, false
)

VARCACHE_PREF(
  "media.eme.audio.blank",
   MediaEmeAudioBlank,
  bool, false
)
VARCACHE_PREF(
  "media.eme.video.blank",
   MediaEmeVideoBlank,
  bool, false
)

VARCACHE_PREF(
  "media.eme.chromium-api.video-shmems",
   MediaEmeChromiumApiVideoShmems,
  uint32_t, 6
)

// Whether to suspend decoding of videos in background tabs.
VARCACHE_PREF(
  "media.suspend-bkgnd-video.enabled",
   MediaSuspendBkgndVideoEnabled,
  bool, true
)

// Delay, in ms, from time window goes to background to suspending
// video decoders. Defaults to 10 seconds.
VARCACHE_PREF(
  "media.suspend-bkgnd-video.delay-ms",
   MediaSuspendBkgndVideoDelayMs,
  RelaxedAtomicUint32, 10000
)

VARCACHE_PREF(
  "media.dormant-on-pause-timeout-ms",
   MediaDormantOnPauseTimeoutMs,
  int32_t, 5000
)

VARCACHE_PREF(
  "media.webspeech.synth.force_global_queue",
   MediaWebspeechSynthForceGlobalQueue,
  bool, false
)

VARCACHE_PREF(
  "media.webspeech.test.enable",
   MediaWebspeechTestEnable,
  bool, false
)

VARCACHE_PREF(
  "media.webspeech.test.fake_fsm_events",
   MediaWebspeechTextFakeFsmEvents,
  bool, false
)

VARCACHE_PREF(
  "media.webspeech.test.fake_recognition_service",
   MediaWebspeechTextFakeRecognitionService,
  bool, false
)

#ifdef MOZ_WEBSPEECH
VARCACHE_PREF(
  "media.webspeech.recognition.enable",
   MediaWebspeechRecognitionEnable,
  bool, false
)
#endif

VARCACHE_PREF(
  "media.webspeech.recognition.force_enable",
   MediaWebspeechRecognitionForceEnable,
  bool, false
)

#if defined(RELEASE_OR_BETA)
# define PREF_VALUE 3
#else
  // Zero tolerance in pre-release builds to detect any decoder regression.
# define PREF_VALUE 0
#endif
VARCACHE_PREF(
  "media.audio-max-decode-error",
   MediaAudioMaxDecodeError,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

#if defined(RELEASE_OR_BETA)
# define PREF_VALUE 2
#else
  // Zero tolerance in pre-release builds to detect any decoder regression.
# define PREF_VALUE 0
#endif
VARCACHE_PREF(
  "media.video-max-decode-error",
   MediaVideoMaxDecodeError,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

// Ogg
VARCACHE_PREF(
  "media.ogg.enabled",
   MediaOggEnabled,
  bool, true
)

// AV1
VARCACHE_PREF(
  "media.av1.enabled",
   MediaAv1Enabled,
  bool, true
)

// Flac
// Use new MediaFormatReader architecture for plain ogg.
VARCACHE_PREF(
  "media.ogg.flac.enabled",
   MediaOggFlacEnabled,
  bool, true
)

VARCACHE_PREF(
  "media.flac.enabled",
   MediaFlacEnabled,
  bool, true
)

// Hls
#ifdef ANDROID
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  "media.hls.enabled",
   MediaHlsEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef MOZ_FMP4
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  "media.mp4.enabled",
   mediaMp4Enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Error/warning handling, Decoder Doctor.
//
// Set to true to force demux/decode warnings to be treated as errors.
VARCACHE_PREF(
  "media.playback.warnings-as-errors",
   MediaPlaybackWarningsAsErrors,
  bool, false
)

// Resume video decoding when the cursor is hovering on a background tab to
// reduce the resume latency and improve the user experience.
VARCACHE_PREF(
  "media.resume-bkgnd-video-on-tabhover",
   MediaResumeBkgndVideoOnTabhover,
  bool, true
)

#ifdef ANDROID
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  "media.videocontrols.lock-video-orientation",
   MediaVideocontrolsLockVideoOrientation,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Media Seamless Looping
VARCACHE_PREF(
  "media.seamless-looping",
   MediaSeamlessLooping,
  bool, true
)

//---------------------------------------------------------------------------
// Network prefs
//---------------------------------------------------------------------------

// Sub-resources HTTP-authentication:
//   0 - don't allow sub-resources to open HTTP authentication credentials
//       dialogs
//   1 - allow sub-resources to open HTTP authentication credentials dialogs,
//       but don't allow it for cross-origin sub-resources
//   2 - allow the cross-origin authentication as well.
VARCACHE_PREF(
  "network.auth.subresource-http-auth-allow",
   network_auth_subresource_http_auth_allow,
  uint32_t, 2
)

// Sub-resources HTTP-authentication for cross-origin images:
// - true: It is allowed to present http auth. dialog for cross-origin images.
// - false: It is not allowed.
// If network.auth.subresource-http-auth-allow has values 0 or 1 this pref does
// not have any effect.
VARCACHE_PREF(
  "network.auth.subresource-img-cross-origin-http-auth-allow",
   network_auth_subresource_img_cross_origin_http_auth_allow,
  bool, false
)

// Resources that are triggered by some non-web-content:
// - true: They are allow to present http auth. dialog
// - false: They are not allow to present http auth. dialog.
VARCACHE_PREF(
  "network.auth.non-web-content-triggered-resources-http-auth-allow",
   network_auth_non_web_content_triggered_resources_http_auth_allow,
  bool, false
)

// Enables the predictive service.
VARCACHE_PREF(
  "network.predictor.enabled",
   network_predictor_enabled,
  bool, true
)

VARCACHE_PREF(
  "network.predictor.enable-hover-on-ssl",
   network_predictor_enable_hover_on_ssl,
  bool, false
)

VARCACHE_PREF(
  "network.predictor.enable-prefetch",
   network_predictor_enable_prefetch,
  bool, false
)

VARCACHE_PREF(
  "network.predictor.page-degradation.day",
   network_predictor_page_degradation_day,
  int32_t, 0
)
VARCACHE_PREF(
  "network.predictor.page-degradation.week",
   network_predictor_page_degradation_week,
  int32_t, 5
)
VARCACHE_PREF(
  "network.predictor.page-degradation.month",
   network_predictor_page_degradation_month,
  int32_t, 10
)
VARCACHE_PREF(
  "network.predictor.page-degradation.year",
   network_predictor_page_degradation_year,
  int32_t, 25
)
VARCACHE_PREF(
  "network.predictor.page-degradation.max",
   network_predictor_page_degradation_max,
  int32_t, 50
)

VARCACHE_PREF(
  "network.predictor.subresource-degradation.day",
   network_predictor_subresource_degradation_day,
  int32_t, 1
)
VARCACHE_PREF(
  "network.predictor.subresource-degradation.week",
   network_predictor_subresource_degradation_week,
  int32_t, 10
)
VARCACHE_PREF(
  "network.predictor.subresource-degradation.month",
   network_predictor_subresource_degradation_month,
  int32_t, 25
)
VARCACHE_PREF(
  "network.predictor.subresource-degradation.year",
   network_predictor_subresource_degradation_year,
  int32_t, 50
)
VARCACHE_PREF(
  "network.predictor.subresource-degradation.max",
   network_predictor_subresource_degradation_max,
  int32_t, 100
)

VARCACHE_PREF(
  "network.predictor.prefetch-rolling-load-count",
   network_predictor_prefetch_rolling_load_count,
  int32_t, 10
)

VARCACHE_PREF(
  "network.predictor.prefetch-min-confidence",
   network_predictor_prefetch_min_confidence,
  int32_t, 100
)
VARCACHE_PREF(
  "network.predictor.preconnect-min-confidence",
   network_predictor_preconnect_min_confidence,
  int32_t, 90
)
VARCACHE_PREF(
  "network.predictor.preresolve-min-confidence",
   network_predictor_preresolve_min_confidence,
  int32_t, 60
)

VARCACHE_PREF(
  "network.predictor.prefetch-force-valid-for",
   network_predictor_prefetch_force_valid_for,
  int32_t, 10
)

VARCACHE_PREF(
  "network.predictor.max-resources-per-entry",
   network_predictor_max_resources_per_entry,
  int32_t, 100
)

// This is selected in concert with max-resources-per-entry to keep memory
// usage low-ish. The default of the combo of the two is ~50k.
VARCACHE_PREF(
  "network.predictor.max-uri-length",
   network_predictor_max_uri_length,
  uint32_t, 500
)

PREF("network.predictor.cleaned-up", bool, false)

// A testing flag.
VARCACHE_PREF(
  "network.predictor.doing-tests",
   network_predictor_doing_tests,
  bool, false
)

//---------------------------------------------------------------------------
// Preferences prefs
//---------------------------------------------------------------------------

PREF("preferences.allow.omt-write", bool, true)

//---------------------------------------------------------------------------
// View source prefs
//---------------------------------------------------------------------------

VARCACHE_PREF(
  "view_source.editor.external",
   view_source_editor_external,
  bool, false
)

//---------------------------------------------------------------------------
// End of prefs
//---------------------------------------------------------------------------

// clang-format on
