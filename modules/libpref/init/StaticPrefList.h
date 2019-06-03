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
// is within the `Prefs starting with "network."` section). Sections must be
// kept in alphabetical order, but prefs within sections need not be. Please
// follow the existing naming convention when considering adding a new pref and
// whether you need a new section.
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
// pref hash table lookup functions, but it also has:
//
// - an associated global variable (the VarCache) that mirrors the pref value
//   in the prefs hash table (unless the update policy is `Skip` or `Once`, see
//   below); and
//
// - a getter function that reads that global variable.
//
// Using the getter to read the pref's value has the two following advantages
// over the normal API functions.
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
//     <update-policy>,
//     <pref-name-string>,
//     <pref-name-id>,
//     <cpp-type>, <default-value>
//   )
//
// - <update-policy> is one of the following:
//
//   * Live: Evaluate the pref and set callback so it stays current/live. This
//     is the normal policy.
//
//   * Skip: Set the value to <default-value>, skip any Preferences calls. This
//     policy should be rarely used and its use is discouraged.
//
//   * Once: Evaluate the pref once, and unchanged during the session once
//     enterprise policies have been loaded. This is useful for features where
//     you want to ignore any pref changes until the start of the next browser
//     session.
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
//   Pref with a Skip or Once policy can be non-atomic as they are only ever
//   written to once during the parent process startup.
//   Pref with a Live policy must be made Atomic if ever accessed outside the
//   main thread; assertions are in place to prevent incorrect usage.
//
// - <default-value> is the same as for normal prefs.
//
// Note that Rust code must access the global variable directly, rather than via
// the getter.

// clang-format off

//---------------------------------------------------------------------------
// Prefs starting with "accessibility."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "accessibility.monoaudio.enable",
  accessibility_monoaudio_enable,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "accessibility.browsewithcaret",
  AccessibilityBrowseWithCaret,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "apz."
// The apz prefs are explained in AsyncPanZoomController.cpp
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "apz.allow_double_tap_zooming",
  APZAllowDoubleTapZooming,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.allow_immediate_handoff",
  APZAllowImmediateHandoff,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.allow_zooming",
  APZAllowZooming,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.enabled",
  APZUseChromeFlingPhysics,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.friction",
  APZChromeFlingPhysicsFriction,
  AtomicFloat, 0.015f
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.inflexion",
  APZChromeFlingPhysicsInflexion,
  AtomicFloat, 0.35f
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.stop_threshold",
  APZChromeFlingPhysicsStopThreshold,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "apz.autoscroll.enabled",
  APZAutoscrollEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.breakout_angle",
  APZAxisBreakoutAngle,
  AtomicFloat, float(M_PI / 8.0) /* 22.5 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.breakout_threshold",
  APZAxisBreakoutThreshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.direct_pan_angle",
  APZAllowedDirectPanAngle,
  AtomicFloat, float(M_PI / 3.0) /* 60 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.lock_angle",
  APZAxisLockAngle,
  AtomicFloat, float(M_PI / 6.0) /* 30 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.mode",
  APZAxisLockMode,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.content_response_timeout",
  APZContentResponseTimeout,
  RelaxedAtomicInt32, 400
)

VARCACHE_PREF(
  Live,
  "apz.danger_zone_x",
  APZDangerZoneX,
  RelaxedAtomicInt32, 50
)

VARCACHE_PREF(
  Live,
  "apz.danger_zone_y",
  APZDangerZoneY,
  RelaxedAtomicInt32, 100
)

VARCACHE_PREF(
  Live,
  "apz.disable_for_scroll_linked_effects",
  APZDisableForScrollLinkedEffects,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.displayport_expiry_ms",
  APZDisplayPortExpiryTime,
  RelaxedAtomicUint32, 15000
)

VARCACHE_PREF(
  Live,
  "apz.drag.enabled",
  APZDragEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.drag.initial.enabled",
  APZDragInitiationEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.drag.touch.enabled",
  APZTouchDragEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.enlarge_displayport_when_clipped",
  APZEnlargeDisplayPortWhenClipped,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.enabled",
  APZFixedMarginOverrideEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.bottom",
  APZFixedMarginOverrideBottom,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.top",
  APZFixedMarginOverrideTop,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_base_mult",
  APZFlingAccelBaseMultiplier,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_interval_ms",
  APZFlingAccelInterval,
  RelaxedAtomicInt32, 500
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_supplemental_mult",
  APZFlingAccelSupplementalMultiplier,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_min_velocity",
  APZFlingAccelMinVelocity,
  AtomicFloat, 1.5f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_x1",
  APZCurveFunctionX1,
  float, 0.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_x2",
  APZCurveFunctionX2,
  float, 1.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_y1",
  APZCurveFunctionY1,
  float, 0.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_y2",
  APZCurveFunctionY2,
  float, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_curve_threshold_inches_per_ms",
  APZCurveThreshold,
  AtomicFloat, -1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_friction",
  APZFlingFriction,
  AtomicFloat, 0.002f
)

VARCACHE_PREF(
  Live,
  "apz.fling_min_velocity_threshold",
  APZFlingMinVelocityThreshold,
  AtomicFloat, 0.5f
)

VARCACHE_PREF(
  Live,
  "apz.fling_stop_on_tap_threshold",
  APZFlingStopOnTapThreshold,
  AtomicFloat, 0.05f
)

VARCACHE_PREF(
  Live,
  "apz.fling_stopped_threshold",
  APZFlingStoppedThreshold,
  AtomicFloat, 0.01f
)

VARCACHE_PREF(
  Live,
  "apz.frame_delay.enabled",
  APZFrameDelayEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "apz.keyboard.enabled",
  APZKeyboardEnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "apz.keyboard.passive-listeners",
  APZKeyboardPassiveListeners,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.max_tap_time",
  APZMaxTapTime,
  RelaxedAtomicInt32, 300
)

VARCACHE_PREF(
  Live,
  "apz.max_velocity_inches_per_ms",
  APZMaxVelocity,
  AtomicFloat, -1.0f
)

VARCACHE_PREF(
  Once,
  "apz.max_velocity_queue_size",
  APZMaxVelocityQueueSize,
  uint32_t, 5
)

VARCACHE_PREF(
  Live,
  "apz.min_skate_speed",
  APZMinSkateSpeed,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.minimap.enabled",
  APZMinimap,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.one_touch_pinch.enabled",
  APZOneTouchPinchEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.enabled",
  APZOverscrollEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.min_pan_distance_ratio",
  APZMinPanDistanceRatio,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.spring_stiffness",
  APZOverscrollSpringStiffness,
  AtomicFloat, 0.001f
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.stop_distance_threshold",
  APZOverscrollStopDistanceThreshold,
  AtomicFloat, 5.0f
)

VARCACHE_PREF(
  Live,
  "apz.paint_skipping.enabled",
  APZPaintSkipping,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.peek_messages.enabled",
  APZPeekMessages,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.mode",
  APZPinchLockMode,
  RelaxedAtomicInt32, 1
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.scroll_lock_threshold",
  APZPinchLockScrollLockThreshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.span_breakout_threshold",
  APZPinchLockSpanBreakoutThreshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.span_lock_threshold",
  APZPinchLockSpanLockThreshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Once,
  "apz.pinch_lock.buffer_max_age",
  APZPinchLockBufferMaxAge,
  int32_t, 50
)

VARCACHE_PREF(
  Live,
  "apz.popups.enabled",
  APZPopupsEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.printtree",
  APZPrintTree,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.record_checkerboarding",
  APZRecordCheckerboarding,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.second_tap_tolerance",
  APZSecondTapTolerance,
  AtomicFloat, 0.5f
)

VARCACHE_PREF(
  Live,
  "apz.test.fails_with_native_injection",
  APZTestFailsWithNativeInjection,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.test.logging_enabled",
  apz_test_logging_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.touch_move_tolerance",
  APZTouchMoveTolerance,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "apz.touch_start_tolerance",
  APZTouchStartTolerance,
  AtomicFloat, 1.0f/4.5f
)

VARCACHE_PREF(
  Live,
  "apz.velocity_bias",
  APZVelocityBias,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.velocity_relevance_time_ms",
  APZVelocityRelevanceTime,
  RelaxedAtomicUint32, 150
)

VARCACHE_PREF(
  Live,
  "apz.x_skate_highmem_adjust",
  APZXSkateHighMemAdjust,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.x_skate_size_multiplier",
  APZXSkateSizeMultiplier,
  AtomicFloat, 1.5f
)

VARCACHE_PREF(
  Live,
  "apz.x_stationary_size_multiplier",
  APZXStationarySizeMultiplier,
  AtomicFloat, 3.0f
)

VARCACHE_PREF(
  Live,
  "apz.y_skate_highmem_adjust",
  APZYSkateHighMemAdjust,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.y_skate_size_multiplier",
  APZYSkateSizeMultiplier,
  AtomicFloat, 2.5f
)

VARCACHE_PREF(
  Live,
  "apz.y_stationary_size_multiplier",
  APZYStationarySizeMultiplier,
  AtomicFloat, 3.5f
)

VARCACHE_PREF(
  Live,
  "apz.zoom_animation_duration_ms",
  APZZoomAnimationDuration,
  RelaxedAtomicInt32, 250
)

VARCACHE_PREF(
  Live,
  "apz.scale_repaint_delay_ms",
  APZScaleRepaintDelay,
  RelaxedAtomicInt32, 500
)

VARCACHE_PREF(
  Live,
  "apz.relative-update.enabled",
  APZRelativeUpdate,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "full-screen-api.mouse-event-allow-left-button-only",
  full_screen_api_mouse_event_allow_left_button_only,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "browser."
//---------------------------------------------------------------------------

PREF("browser.active_color", String, "")
PREF("browser.anchor_color", String, "")

// See http://dev.w3.org/html5/spec/forms.html#attr-fe-autofocus
VARCACHE_PREF(
  Live,
  "browser.autofocus",
  browser_autofocus,
  bool, true
)

VARCACHE_PREF(
  Live,
  "browser.cache.offline.enable",
  browser_cache_offline_enable,
  bool, true
)

// Whether Content Blocking Third-Party Cookies UI has been enabled.
VARCACHE_PREF(
  Live,
  "browser.contentblocking.allowlist.storage.enabled",
  browser_contentblocking_allowlist_storage_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "browser.contentblocking.allowlist.annotations.enabled",
  browser_contentblocking_allowlist_annotations_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "browser.contentblocking.database.enabled",
  browser_contentblocking_database_enabled,
  bool, true
)

// How many recent block/unblock actions per origins we remember in the
// Content Blocking log for each top-level window.
VARCACHE_PREF(
  Live,
  "browser.contentblocking.originlog.length",
  browser_contentblocking_originlog_length,
  uint32_t, 32
)

VARCACHE_PREF(
  Live,
  "browser.contentblocking.rejecttrackers.control-center.ui.enabled",
  browser_contentblocking_rejecttrackers_control_center_ui_enabled,
  bool, false
)

PREF("browser.display.background_color", String, "")

// 0 = default: always, except in high contrast mode
// 1 = always
// 2 = never
VARCACHE_PREF(
  Live,
  "browser.display.document_color_use",
  browser_display_document_color_use,
  uint32_t, 0
)

VARCACHE_PREF(
  Live,
  "browser.display.focus_ring_on_anything",
  browser_display_focus_ring_on_anything,
  bool, false
)

VARCACHE_PREF(
  Live,
  "browser.display.focus_ring_width",
  browser_display_focus_ring_width,
  uint32_t, 1
)

PREF("browser.display.focus_background_color", String, "")

// 0=solid, 1=dotted
VARCACHE_PREF(
  Live,
  "browser.display.focus_ring_style",
  browser_display_focus_ring_style,
  uint32_t, 1
)

PREF("browser.display.focus_text_color", String, "")
PREF("browser.display.foreground_color", String, "")

VARCACHE_PREF(
  Live,
  "browser.display.show_focus_rings",
  browser_display_show_focus_rings,
  bool,
  // Focus rings are not shown on Mac or Android by default.
#if defined(XP_MACOSX) || defined(ANDROID)
  false
#else
  true
#endif
)

// In theory: 0 = never, 1 = quick, 2 = always, though we always just use it as
// a bool!
VARCACHE_PREF(
  Live,
  "browser.display.use_document_fonts",
  browser_display_use_document_fonts,
  RelaxedAtomicInt32, 1
)

VARCACHE_PREF(
  Live,
  "browser.display.use_focus_colors",
  browser_display_use_focus_colors,
  bool, false
)

VARCACHE_PREF(
  Live,
  "browser.display.use_system_colors",
  browser_display_use_system_colors,
  bool, true
)

// IMPORTANT: Keep this in condition in sync with all.js. The value
// of MOZILLA_OFFICIAL is different between full and artifact builds, so without
// it being specified there, dump is disabled in artifact builds (see Bug 1490412).
#ifdef MOZILLA_OFFICIAL
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "browser.dom.window.dump.enabled",
  browser_dom_window_dump_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Render animations and videos as a solid color
VARCACHE_PREF(
  Live,
  "browser.measurement.render_anims_and_video_solid",
  browser_measurement_render_anims_and_video_solid,
  RelaxedAtomicBool, false
)

// Blocked plugin content
VARCACHE_PREF(
  Live,
  "browser.safebrowsing.blockedURIs.enabled",
  browser_safebrowsing_blockedURIs_enabled,
  bool, true
)

// Malware protection
VARCACHE_PREF(
  Live,
  "browser.safebrowsing.malware.enabled",
  browser_safebrowsing_malware_enabled,
  bool, true
)

// Password protection
VARCACHE_PREF(
  Live,
  "browser.safebrowsing.passwords.enabled",
  browser_safebrowsing_passwords_enabled,
  bool, false
)

// Phishing protection
VARCACHE_PREF(
  Live,
  "browser.safebrowsing.phishing.enabled",
  browser_safebrowsing_phishing_enabled,
  bool, true
)

// Maximum size for an array to store the safebrowsing prefixset.
VARCACHE_PREF(
  Live,
  "browser.safebrowsing.prefixset_max_array_size",
  browser_safebrowsing_prefixset_max_array_size,
  RelaxedAtomicUint32, 512*1024
)

// ContentSessionStore prefs
// Maximum number of bytes of DOMSessionStorage data we collect per origin.
VARCACHE_PREF(
  Live,
  "browser.sessionstore.dom_storage_limit",
  browser_sessionstore_dom_storage_limit,
  uint32_t, 2048
)

// When this pref is enabled document loads with a mismatched
// Cross-Origin header will fail to load
VARCACHE_PREF(
  Live,
  "browser.tabs.remote.useCrossOriginPolicy",
  browser_tabs_remote_useCrossOriginPolicy,
  bool, false
)

VARCACHE_PREF(
  Live,
  "browser.ui.scroll-toolbar-threshold",
  ToolbarScrollThreshold,
  RelaxedAtomicInt32, 10
)

VARCACHE_PREF(
  Live,
  "browser.ui.zoom.force-user-scalable",
  ForceUserScalable,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "browser.underline_anchors",
  browser_underline_anchors,
  bool, true
)

VARCACHE_PREF(
  Live,
  "browser.viewport.desktopWidth",
  DesktopViewportWidth,
  RelaxedAtomicInt32, 980
)

PREF("browser.visited_color", String, "")

//---------------------------------------------------------------------------
// Prefs starting with "canvas."
//---------------------------------------------------------------------------

// Add support for canvas path objects
VARCACHE_PREF(
  Live,
  "canvas.path.enabled",
  canvas_path_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "canvas.capturestream.enabled",
  canvas_capturestream_enabled,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "channelclassifier."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "channelclassifier.allowlist_example",
  channelclassifier_allowlist_example,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "clipboard."
//---------------------------------------------------------------------------

#if !defined(ANDROID) && !defined(XP_MACOSX) && defined(XP_UNIX)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "clipboard.autocopy",
  clipboard_autocopy,
  bool, PREF_VALUE
)
#undef PREF_VALUE

//---------------------------------------------------------------------------
// Prefs starting with "device."
//---------------------------------------------------------------------------
VARCACHE_PREF(
  Live,
  "device.sensors.ambientLight.enabled",
  device_sensors_ambientLight_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "device.sensors.motion.enabled",
  device_sensors_motion_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "device.sensors.orientation.enabled",
  device_sensors_orientation_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "device.sensors.proximity.enabled",
  device_sensors_proximity_enabled,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "devtools."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "devtools.enabled",
  devtools_enabled,
  RelaxedAtomicBool, false
)

#ifdef MOZILLA_OFFICIAL
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "devtools.console.stdout.chrome",
  devtools_console_stdout_chrome,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "devtools.console.stdout.content",
  devtools_console_stdout_content,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "dom."
//---------------------------------------------------------------------------

// Allow cut/copy
VARCACHE_PREF(
  Live,
  "dom.allow_cut_copy",
  dom_allow_cut_copy,
  bool, true
)

// Is support for automatically removing replaced filling animations enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.animations-api.autoremove.enabled",
  dom_animations_api_autoremove_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for composite operations from the Web Animations API enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.animations-api.compositing.enabled",
  dom_animations_api_compositing_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for the core interfaces of Web Animations API enabled?
VARCACHE_PREF(
  Live,
  "dom.animations-api.core.enabled",
  dom_animations_api_core_enabled,
  bool, true
)

// Is support for Document.getAnimations() and Element.getAnimations()
// supported?
//
// Before enabling this by default, make sure also CSSPseudoElement interface
// has been spec'ed properly, or we should add a separate pref for
// CSSPseudoElement interface. See Bug 1174575 for further details.
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.animations-api.getAnimations.enabled",
  dom_animations_api_getAnimations_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for animations from the Web Animations API without 0%/100%
// keyframes enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.animations-api.implicit-keyframes.enabled",
  dom_animations_api_implicit_keyframes_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for timelines from the Web Animations API enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.animations-api.timelines.enabled",
  dom_animations_api_timelines_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for AudioWorklet enabled?
VARCACHE_PREF(
  Live,
  "dom.audioworklet.enabled",
  dom_audioworklet_enabled,
  bool, false
)

// Block multiple external protocol URLs in iframes per single event.
VARCACHE_PREF(
  Live,
  "dom.block_external_protocol_in_iframes",
  dom_block_external_protocol_in_iframes,
  bool, true
)

// Block multiple window.open() per single event.
VARCACHE_PREF(
  Live,
  "dom.block_multiple_popups",
  dom_block_multiple_popups,
  bool, true
)

// SW Cache API
VARCACHE_PREF(
  Live,
  "dom.caches.enabled",
  dom_caches_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "dom.caches.testing.enabled",
  dom_caches_testing_enabled,
  RelaxedAtomicBool, false
)

// Whether Mozilla specific "text" event should be dispatched only in the
// system group or not in content.
VARCACHE_PREF(
  Live,
  "dom.compositionevent.text.dispatch_only_system_group_in_content",
  dom_compositionevent_text_dispatch_only_system_group_in_content,
  bool, true
)

// Any how many seconds we allow external protocol URLs in iframe when not in
// single events
VARCACHE_PREF(
  Live,
  "dom.delay.block_external_protocol_in_iframes",
  dom_delay_block_external_protocol_in_iframes,
  uint32_t, 10 // in seconds
)

// HTML <dialog> element
VARCACHE_PREF(
  Live,
  "dom.dialog_element.enabled",
  dom_dialog_element_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.disable_open_during_load",
  dom_disable_open_during_load,
  bool, false
)

// Enable Performance API
// Whether nonzero values can be returned from performance.timing.*
VARCACHE_PREF(
  Live,
  "dom.enable_performance",
  dom_enable_performance,
  RelaxedAtomicBool, true
)

// Enable Performance Observer API
VARCACHE_PREF(
  Live,
  "dom.enable_performance_observer",
  dom_enable_performance_observer,
  RelaxedAtomicBool, true
)

// Whether resource timing will be gathered and returned by performance.GetEntries*
VARCACHE_PREF(
  Live,
  "dom.enable_resource_timing",
  dom_enable_resource_timing,
  bool, true
)

// Whether performance.GetEntries* will contain an entry for the active document
VARCACHE_PREF(
  Live,
  "dom.enable_performance_navigation_timing",
  dom_enable_performance_navigation_timing,
  bool, true
)

// If this is true, it's allowed to fire "cut", "copy" and "paste" events.
// Additionally, "input" events may expose clipboard content when inputType
// is "insertFromPaste" or something.
VARCACHE_PREF(
  Live,
  "dom.event.clipboardevents.enabled",
  dom_event_clipboardevents_enabled,
  bool, true
)

// Time limit, in milliseconds, for EventStateManager::IsHandlingUserInput().
// Used to detect long running handlers of user-generated events.
VARCACHE_PREF(
  Live,
  "dom.event.handling-user-input-time-limit",
  dom_event_handling_user_input_time_limit,
  uint32_t, 1000
)

// Enable clipboard readText() and writeText() by default
VARCACHE_PREF(
  Live,
  "dom.events.asyncClipboard",
  dom_events_asyncClipboard,
  bool, true
)

// Disable clipboard read() and write() by default
VARCACHE_PREF(
  Live,
  "dom.events.asyncClipboard.dataTransfer",
  dom_events_asyncClipboard_dataTransfer,
  bool, false
)

// Whether to expose test interfaces of various sorts
VARCACHE_PREF(
  Live,
  "dom.expose_test_interfaces",
  dom_expose_test_interfaces,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.fetchObserver.enabled",
  dom_fetchObserver_enabled,
  RelaxedAtomicBool, false
)

// Allow the content process to create a File from a path. This is allowed just
// on parent process, on 'file' Content process, or for testing.
VARCACHE_PREF(
  Live,
  "dom.file.createInChild",
  dom_file_createInChild,
  RelaxedAtomicBool, false
)

// Support @autocomplete values for form autofill feature.
VARCACHE_PREF(
  Live,
  "dom.forms.autocomplete.formautofill",
  dom_forms_autocomplete_formautofill,
  bool, false
)

// Whether the Gamepad API is enabled
VARCACHE_PREF(
  Live,
  "dom.gamepad.enabled",
  dom_gamepad_enabled,
  bool, true
)

// Is Gamepad Extension API enabled?
VARCACHE_PREF(
  Live,
  "dom.gamepad.extensions.enabled",
  dom_gamepad_extensions_enabled,
  bool, true
)

// Is LightIndicator API enabled in Gamepad Extension API?
VARCACHE_PREF(
  Live,
  "dom.gamepad.extensions.lightindicator",
  dom_gamepad_extensions_lightindicator,
  bool, false
)

// Is MultiTouch API enabled in Gamepad Extension API?
VARCACHE_PREF(
  Live,
  "dom.gamepad.extensions.multitouch",
  dom_gamepad_extensions_multitouch,
  bool, false
)

// Is Gamepad vibrate haptic feedback function enabled?
VARCACHE_PREF(
  Live,
  "dom.gamepad.haptic_feedback.enabled",
  dom_gamepad_haptic_feedback_enabled,
  bool, true
)

#ifdef RELEASE_OR_BETA
# define PREF_VALUE  false
#else
# define PREF_VALUE  true
#endif
VARCACHE_PREF(
  Live,
  "dom.gamepad.non_standard_events.enabled",
  dom_gamepad_non_standard_events_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.gamepad.test.enabled",
  dom_gamepad_test_enabled,
  bool, false
)

// W3C draft ImageCapture API
VARCACHE_PREF(
  Live,
  "dom.imagecapture.enabled",
  dom_imagecapture_enabled,
  bool, false
)

// Enable passing the "storage" option to indexedDB.open.
VARCACHE_PREF(
  Live,
  "dom.indexedDB.storageOption.enabled",
  dom_indexedDB_storageOption_enabled,
  RelaxedAtomicBool, false
)

// Whether we conform to Input Events Level 1 or Input Events Level 2.
// true:  conforming to Level 1
// false: conforming to Level 2
VARCACHE_PREF(
  Live,
  "dom.input_events.conform_to_level_1",
  dom_input_events_conform_to_level_1,
  bool, true
)

// Enable not moving the cursor to end when a text input or textarea has .value
// set to the value it already has.  By default, enabled.
VARCACHE_PREF(
  Live,
  "dom.input.skip_cursor_move_for_same_value_set",
  dom_input_skip_cursor_move_for_same_value_set,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.IntersectionObserver.enabled",
  dom_IntersectionObserver_enabled,
  bool, true
)

#ifdef NIGHTLY_BUILD
# define PREF_VALUE  true
#else
# define PREF_VALUE  false
#endif
VARCACHE_PREF(
  Live,
  "dom.ipc.cancel_content_js_when_navigating",
  dom_ipc_cancel_content_js_when_navigating,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.ipc.plugins.asyncdrawing.enabled",
  PluginAsyncDrawingEnabled,
  RelaxedAtomicBool, false
)

// How long a content process can take before closing its IPC channel
// after shutdown is initiated.  If the process exceeds the timeout,
// we fear the worst and kill it.
#if !defined(DEBUG) && !defined(MOZ_ASAN) && !defined(MOZ_VALGRIND) && \
    !defined(MOZ_TSAN)
# define PREF_VALUE 5
#else
# define PREF_VALUE 0
#endif
VARCACHE_PREF(
  Live,
  "dom.ipc.tabs.shutdownTimeoutSecs",
  dom_ipc_tabs_shutdownTimeoutSecs,
  RelaxedAtomicUint32, PREF_VALUE
)
#undef PREF_VALUE

// If this is true, "keypress" event's keyCode value and charCode value always
// become same if the event is not created/initialized by JS.
VARCACHE_PREF(
  Live,
  "dom.keyboardevent.keypress.set_keycode_and_charcode_to_same_value",
  dom_keyboardevent_keypress_set_keycode_and_charcode_to_same_value,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.largeAllocation.forceEnable",
  dom_largeAllocation_forceEnable,
  bool, false
)

// Whether the disabled attribute in HTMLLinkElement disables the sheet loading
// altogether, or forwards to the inner stylesheet method without attribute
// reflection.
//
// Historical behavior is the second, the first is being discussed at:
// https://github.com/whatwg/html/issues/3840
VARCACHE_PREF(
  Live,
  "dom.link.disabled_attribute.enabled",
  dom_link_disabled_attribute_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.meta-viewport.enabled",
  MetaViewportEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.metaElement.setCookie.allowed",
  dom_metaElement_setCookie_allowed,
  bool, false
)

// Whether we disable triggering mutation events for changes to style
// attribute via CSSOM.
// NOTE: This preference is used in unit tests. If it is removed or its default
// value changes, please update test_sharedMap_var_caches.js accordingly.
VARCACHE_PREF(
  Live,
  "dom.mutation-events.cssom.disabled",
  dom_mutation_events_cssom_disabled,
  bool, true
)

// Network Information API
#if defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.netinfo.enabled",
  dom_netinfo_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Enable/disable the PaymentRequest API
VARCACHE_PREF(
  Live,
  "dom.payments.request.enabled",
  dom_payments_request_enabled,
  bool, false
)

// Whether a user gesture is required to call PaymentRequest.prototype.show().
VARCACHE_PREF(
  Live,
  "dom.payments.request.user_interaction_required",
  dom_payments_request_user_interaction_required,
  bool, true
)

// Time in milliseconds for PaymentResponse to wait for
// the Web page to call complete().
VARCACHE_PREF(
  Live,
  "dom.payments.response.timeout",
  dom_payments_response_timeout,
  uint32_t, 5000
)

// Enable printing performance marks/measures to log
VARCACHE_PREF(
  Live,
  "dom.performance.enable_user_timing_logging",
  dom_performance_enable_user_timing_logging,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.performance.children_results_ipc_timeout",
  dom_performance_children_results_ipc_timeout,
  uint32_t, 1000
)
#undef PREF_VALUE

// Enable notification of performance timing
VARCACHE_PREF(
  Live,
  "dom.performance.enable_notify_performance_timing",
  dom_performance_enable_notify_performance_timing,
  bool, false
)

// Whether we should show the placeholder when the element is focused but empty.
VARCACHE_PREF(
  Live,
  "dom.placeholder.show_on_focus",
  dom_placeholder_show_on_focus,
  bool, true
)

// Presentation API
VARCACHE_PREF(
  Live,
  "dom.presentation.enabled",
  dom_presentation_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.presentation.controller.enabled",
  dom_presentation_controller_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.presentation.receiver.enabled",
  dom_presentation_receiver_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.presentation.testing.simulate-receiver",
  dom_presentation_testing_simulate_receiver,
  bool, false
)

// WHATWG promise rejection events. See
// https://html.spec.whatwg.org/multipage/webappapis.html#promiserejectionevent
// TODO: Enable the event interface once actually firing it (bug 1362272).
VARCACHE_PREF(
  Live,
  "dom.promise_rejection_events.enabled",
  dom_promise_rejection_events_enabled,
  RelaxedAtomicBool, false
)

// This currently only affects XHTML. For XUL the cache is always allowed.
VARCACHE_PREF(
  Live,
  "dom.prototype_document_cache.enabled",
  dom_prototype_document_cache_enabled,
  bool, true
)

// Push
VARCACHE_PREF(
  Live,
  "dom.push.enabled",
  dom_push_enabled,
  RelaxedAtomicBool, false
)

// Reporting API.
#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.reporting.enabled",
  dom_reporting_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.reporting.testing.enabled",
  dom_reporting_testing_enabled,
  RelaxedAtomicBool, false
)

#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.reporting.featurePolicy.enabled",
  dom_reporting_featurePolicy_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.reporting.header.enabled",
  dom_reporting_header_enabled,
  RelaxedAtomicBool, false
)

// In seconds. The timeout to remove not-active report-to endpoints.
VARCACHE_PREF(
  Live,
  "dom.reporting.cleanup.timeout",
  dom_reporting_cleanup_timeout,
  uint32_t, 3600
)

// Any X seconds the reports are dispatched to endpoints.
VARCACHE_PREF(
  Live,
  "dom.reporting.delivering.timeout",
  dom_reporting_delivering_timeout,
  uint32_t, 5
)

// How many times the delivering of a report should be tried.
VARCACHE_PREF(
  Live,
  "dom.reporting.delivering.maxFailures",
  dom_reporting_delivering_maxFailures,
  uint32_t, 3
)

// How many reports should be stored in the report queue before being delivered.
VARCACHE_PREF(
  Live,
  "dom.reporting.delivering.maxReports",
  dom_reporting_delivering_maxReports,
  uint32_t, 100
)

// Enable requestIdleCallback API
VARCACHE_PREF(
  Live,
  "dom.requestIdleCallback.enabled",
  dom_requestIdleCallback_enabled,
  bool, true
)

#ifdef JS_BUILD_BINAST
VARCACHE_PREF(
  Live,
  "dom.script_loader.binast_encoding.domain.restrict",
  dom_script_loader_binast_encoding_domain_restrict,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.script_loader.binast_encoding.enabled",
  dom_script_loader_binast_encoding_enabled,
  RelaxedAtomicBool, false
)
#endif

// Whether to enable the JavaScript start-up cache. This causes one of the first
// execution to record the bytecode of the JavaScript function used, and save it
// in the existing cache entry. On the following loads of the same script, the
// bytecode would be loaded from the cache instead of being generated once more.
VARCACHE_PREF(
  Live,
  "dom.script_loader.bytecode_cache.enabled",
  dom_script_loader_bytecode_cache_enabled,
  bool, true
)

// Ignore the heuristics of the bytecode cache, and always record on the first
// visit. (used for testing purposes).

// Choose one strategy to use to decide when the bytecode should be encoded and
// saved. The following strategies are available right now:
//   * -2 : (reader mode) The bytecode cache would be read, but it would never
//          be saved.
//   * -1 : (eager mode) The bytecode would be saved as soon as the script is
//          seen for the first time, independently of the size or last access
//          time.
//   *  0 : (default) The bytecode would be saved in order to minimize the
//          page-load time.
//
// Other values might lead to experimental strategies. For more details, have a
// look at: ScriptLoader::ShouldCacheBytecode function.
VARCACHE_PREF(
  Live,
  "dom.script_loader.bytecode_cache.strategy",
  dom_script_loader_bytecode_cache_strategy,
  int32_t, 0
)

#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
// This pref enables FeaturePolicy logic and the parsing of 'allow' attribute in
// HTMLIFrameElement objects.
VARCACHE_PREF(
  Live,
  "dom.security.featurePolicy.enabled",
  dom_security_featurePolicy_enabled,
  bool, PREF_VALUE
)

// This pref enables the featurePolicy header support.
VARCACHE_PREF(
  Live,
  "dom.security.featurePolicy.header.enabled",
  dom_security_featurePolicy_header_enabled,
  bool, PREF_VALUE
)

// Expose the 'policy' attribute in document and HTMLIFrameElement
VARCACHE_PREF(
  Live,
  "dom.security.featurePolicy.webidl.enabled",
  dom_security_featurePolicy_webidl_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.separate_event_queue_for_post_message.enabled",
  dom_separate_event_queue_for_post_message_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.serviceWorkers.enabled",
  dom_serviceWorkers_enabled,
  RelaxedAtomicBool, false
)

// If true. then the service worker interception and the ServiceWorkerManager
// will live in the parent process.  This only takes effect on browser start.
// Note, this is not currently safe to use for normal browsing yet.
PREF("dom.serviceWorkers.parent_intercept", bool, false)

VARCACHE_PREF(
  Live,
  "dom.serviceWorkers.testing.enabled",
  dom_serviceWorkers_testing_enabled,
  RelaxedAtomicBool, false
)

// Are shared memory User Agent style sheets enabled?
VARCACHE_PREF(
  Live,
  "dom.storage_access.auto_grants.delayed",
  dom_storage_access_auto_grants_delayed,
  bool, true
)

// Storage-access API.
VARCACHE_PREF(
  Live,
  "dom.storage_access.enabled",
  dom_storage_access_enabled,
  bool, false
)

// Enable Storage API for all platforms except Android.
#if !defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.storageManager.enabled",
  dom_storageManager_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// For area and anchor elements with target=_blank and no rel set to
// opener/noopener.
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.targetBlankNoOpener.enabled",
  dom_targetBlankNoOpener_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.testing.structuredclonetester.enabled",
  dom_testing_structuredclonetester_enabled,
  RelaxedAtomicBool, false
)

// Should we defer timeouts and intervals while loading a page.  Released
// on Idle or when the page is loaded.
VARCACHE_PREF(
  Live,
  "dom.timeout.defer_during_load",
  dom_timeout_defer_during_load,
  bool, true
)

// Maximum deferral time for setTimeout/Interval in milliseconds
VARCACHE_PREF(
  Live,
  "dom.timeout.max_idle_defer_ms",
  dom_timeout_max_idle_defer_ms,
  uint32_t, 10*1000
)

// UDPSocket API
VARCACHE_PREF(
  Live,
  "dom.udpsocket.enabled",
  dom_udpsocket_enabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "dom.vr.enabled",
  dom_vr_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.autoactivate.enabled",
  VRAutoActivateEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.controller.enumerate.interval",
  VRControllerEnumerateInterval,
  RelaxedAtomicInt32, 1000
)

VARCACHE_PREF(
  Live,
  "dom.vr.controller_trigger_threshold",
  VRControllerTriggerThreshold,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "dom.vr.display.enumerate.interval",
  VRDisplayEnumerateInterval,
  RelaxedAtomicInt32, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.display.rafMaxDuration",
  VRDisplayRafMaxDuration,
  RelaxedAtomicUint32, 50
)

VARCACHE_PREF(
  Once,
  "dom.vr.external.enabled",
  VRExternalEnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.external.notdetected.timeout",
  VRExternalNotDetectedTimeout,
  RelaxedAtomicInt32, 60000
)

VARCACHE_PREF(
  Live,
  "dom.vr.external.quit.timeout",
  VRExternalQuitTimeout,
  RelaxedAtomicInt32, 10000
)

VARCACHE_PREF(
  Live,
  "dom.vr.inactive.timeout",
  VRInactiveTimeout,
  RelaxedAtomicInt32, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.navigation.timeout",
  VRNavigationTimeout,
  RelaxedAtomicInt32, 1000
)

VARCACHE_PREF(
  Once,
  "dom.vr.oculus.enabled",
  VROculusEnabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.vr.oculus.invisible.enabled",
  VROculusInvisibleEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "dom.vr.oculus.present.timeout",
  VROculusPresentTimeout,
  RelaxedAtomicInt32, 500
)

VARCACHE_PREF(
  Live,
  "dom.vr.oculus.quit.timeout",
  VROculusQuitTimeout,
  RelaxedAtomicInt32, 10000
)

VARCACHE_PREF(
  Once,
  "dom.vr.openvr.enabled",
  VROpenVREnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "dom.vr.openvr.action_input",
  VROpenVRActionInputEnabled,
  bool, true
)

VARCACHE_PREF(
  Once,
  "dom.vr.osvr.enabled",
  VROSVREnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.poseprediction.enabled",
  VRPosePredictionEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Once,
  "dom.vr.process.enabled",
  VRProcessEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "dom.vr.process.startup_timeout_ms",
  VRProcessTimeoutMs,
  int32_t, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.puppet.enabled",
  VRPuppetEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.puppet.submitframe",
  VRPuppetSubmitFrame,
  RelaxedAtomicUint32, 0
)

// VR test system.
VARCACHE_PREF(
  Live,
  "dom.vr.test.enabled",
  dom_vr_test_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.require-gesture",
  VRRequireGesture,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Once,
  "dom.vr.service.enabled",
  VRServiceEnabled,
  bool, true
)

// W3C draft pointer events
#ifdef ANDROID
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.w3c_pointer_events.enabled",
  dom_w3c_pointer_events_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// In case Touch API is enabled, this pref controls whether to support
// ontouch* event handlers, document.createTouch, document.createTouchList and
// document.createEvent("TouchEvent").
#ifdef ANDROID
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.w3c_touch_events.legacy_apis.enabled",
  dom_w3c_touch_events_legacy_apis_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for the Web Audio API enabled?
VARCACHE_PREF(
  Live,
  "dom.webaudio.enabled",
  dom_webaudio_enabled,
  bool, true
)

#if !defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.webkitBlink.dirPicker.enabled",
  dom_webkitBlink_dirPicker_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// NOTE: This preference is used in unit tests. If it is removed or its default
// value changes, please update test_sharedMap_var_caches.js accordingly.
VARCACHE_PREF(
  Live,
  "dom.webcomponents.shadowdom.report_usage",
  dom_webcomponents_shadowdom_report_usage,
  bool, false
)

// Is support for the Web GPU API enabled?
VARCACHE_PREF(
  Live,
  "dom.webgpu.enable",
  dom_webgpu_enable,
  bool, false
)

// Whether the WebMIDI API is enabled
VARCACHE_PREF(
  Live,
  "dom.webmidi.enabled",
  dom_webmidi_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.webnotifications.allowinsecure",
  dom_webnotifications_allowinsecure,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.webnotifications.enabled",
  dom_webnotifications_enabled,
  RelaxedAtomicBool, true
)

#ifdef EARLY_BETA_OR_EARLIER
# define PREF_VALUE  true
#else
# define PREF_VALUE  false
#endif
VARCACHE_PREF(
  Live,
  "dom.webnotifications.requireuserinteraction",
  dom_webnotifications_requireuserinteraction,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef NIGHTLY_BUILD
# define PREF_VALUE  true
#else
# define PREF_VALUE  false
#endif
VARCACHE_PREF(
  Live,
  "dom.webnotifications.requireinteraction.enabled",
  dom_webnotifications_requireinteraction_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.webnotifications.serviceworker.enabled",
  dom_webnotifications_serviceworker_enabled,
  RelaxedAtomicBool, true
)

// Enable the "noreferrer" feature argument for window.open()
VARCACHE_PREF(
  Live,
  "dom.window.open.noreferrer.enabled",
  dom_window_open_noreferrer_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "dom.worker.canceling.timeoutMilliseconds",
  dom_worker_canceling_timeoutMilliseconds,
  RelaxedAtomicUint32, 30000 /* 30 seconds */
)

// Is support for compiling DOM worker scripts directly from UTF-8 (without ever
// inflating to UTF-16) enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "dom.worker.script_loader.utf8_parsing.enabled",
  dom_worker_script_loader_utf8_parsing_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.worker.use_medium_high_event_queue",
  dom_worker_use_medium_high_event_queue,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "dom.worklet.enabled",
  dom_worklet_enabled,
  bool, false
)

// Enable content type normalization of XHR uploads via MIME Sniffing standard
VARCACHE_PREF(
  Live,
  "dom.xhr.standard_content_type_normalization",
  dom_xhr_standard_content_type_normalization,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "extensions."
//---------------------------------------------------------------------------

#ifdef ANDROID
// Private browsing opt-in is only supported on Firefox desktop.
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "extensions.allowPrivateBrowsingByDefault",
  extensions_allowPrivateBrowsingByDefault,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// This pref should be set to true only in case of regression related to the
// changes applied in Bug 152591 (to be removed as part of Bug 1537753).
VARCACHE_PREF(
  Live,
  "extensions.cookiesBehavior.overrideOnTopLevel",
  extensions_cookiesBehavior_overrideOnTopLevel,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "full-screen-api."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "full-screen-api.enabled",
  full_screen_api_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "full-screen-api.unprefix.enabled",
  full_screen_api_unprefix_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "full-screen-api.allow-trusted-requests-only",
  full_screen_api_allow_trusted_requests_only,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "fuzzing.". It's important that these can only be
// checked in fuzzing builds (when FUZZING is defined), otherwise you could
// enable the fuzzing stuff on your regular build which would be bad :)
//---------------------------------------------------------------------------

#ifdef FUZZING
VARCACHE_PREF(
  Live,
  "fuzzing.enabled",
  fuzzing_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "fuzzing.necko.enabled",
  fuzzing_necko_enabled,
  RelaxedAtomicBool, false
)
#endif

//---------------------------------------------------------------------------
// Prefs starting with "general."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "general.smoothScroll",
  SmoothScrollEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.currentVelocityWeighting",
  SmoothScrollCurrentVelocityWeighting,
  AtomicFloat, 0.25
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.durationToIntervalRatio",
  SmoothScrollDurationToIntervalRatio,
  RelaxedAtomicInt32, 200
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.lines.durationMaxMS",
  LineSmoothScrollMaxDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.lines.durationMinMS",
  LineSmoothScrollMinDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel",
  WheelSmoothScrollEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel.durationMaxMS",
  WheelSmoothScrollMaxDurationMs,
  RelaxedAtomicInt32, 400
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel.durationMinMS",
  WheelSmoothScrollMinDurationMs,
  RelaxedAtomicInt32, 200
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.other.durationMaxMS",
  OtherSmoothScrollMaxDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.other.durationMinMS",
  OtherSmoothScrollMinDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages",
  PageSmoothScrollEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages.durationMaxMS",
  PageSmoothScrollMaxDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages.durationMinMS",
  PageSmoothScrollMinDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pixels.durationMaxMS",
  PixelSmoothScrollMaxDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pixels.durationMinMS",
  PixelSmoothScrollMinDurationMs,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.stopDecelerationWeighting",
  SmoothScrollStopDecelerationWeighting,
  AtomicFloat, 0.4f
)


VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.enabled",
  SmoothScrollMSDPhysicsEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.continuousMotionMaxDeltaMS",
  SmoothScrollMSDPhysicsContinuousMotionMaxDeltaMS,
  RelaxedAtomicInt32, 120
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.motionBeginSpringConstant",
  SmoothScrollMSDPhysicsMotionBeginSpringConstant,
  RelaxedAtomicInt32, 1250
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownMinDeltaMS",
  SmoothScrollMSDPhysicsSlowdownMinDeltaMS,
  RelaxedAtomicInt32, 12
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownMinDeltaRatio",
  SmoothScrollMSDPhysicsSlowdownMinDeltaRatio,
  AtomicFloat, 1.3f
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownSpringConstant",
  SmoothScrollMSDPhysicsSlowdownSpringConstant,
  RelaxedAtomicInt32, 2000
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.regularSpringConstant",
  SmoothScrollMSDPhysicsRegularSpringConstant,
  RelaxedAtomicInt32, 1000
)

//---------------------------------------------------------------------------
// Prefs starting with "gfx."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Once,
  "gfx.allow-texture-direct-mapping",
  AllowTextureDirectMapping,
  bool, true
)

VARCACHE_PREF(
  Once,
  "gfx.android.rgb16.force",
  AndroidRGB16Force,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.apitrace.enabled",
  UseApitrace,
  bool, false
)

#if defined(RELEASE_OR_BETA)
// "Skip" means this is locked to the default value in beta and release.
VARCACHE_PREF(
  Skip,
  "gfx.blocklist.all",
  BlocklistAll,
  int32_t, 0
)
#else
VARCACHE_PREF(
  Once,
  "gfx.blocklist.all",
  BlocklistAll,
  int32_t, 0
)
#endif

// 0x7fff is the maximum supported xlib surface size and is more than enough for canvases.
VARCACHE_PREF(
  Live,
  "gfx.canvas.max-size",
  MaxCanvasSize,
  RelaxedAtomicInt32, 0x7fff
)

VARCACHE_PREF(
  Live,
  "gfx.color_management.enablev4",
  CMSEnableV4,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.color_management.mode",
  CMSMode,
  RelaxedAtomicInt32, -1
)

// The zero default here should match QCMS_INTENT_DEFAULT from qcms.h
VARCACHE_PREF(
  Live,
  "gfx.color_management.rendering_intent",
  CMSRenderingIntent,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.clearstate",
  CompositorClearState,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.glcontext.opaque",
  CompositorGLContextOpaque,
  RelaxedAtomicBool, false
)

#if defined(MOZ_WIDGET_ANDROID)
// Overrides the glClear color used when the surface origin is not (0, 0)
// Used for drawing a border around the content.
VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.r",
  CompositorOverrideClearColorR,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.g",
  CompositorOverrideClearColorG,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.b",
  CompositorOverrideClearColorB,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.a",
  CompositorOverrideClearColorA,
  AtomicFloat, 0.0f
)
#endif // defined(MOZ_WIDGET_ANDROID)

VARCACHE_PREF(
  Live,
  "gfx.content.always-paint",
  AlwaysPaint,
  RelaxedAtomicBool, false
)

// Size in megabytes
VARCACHE_PREF(
  Once,
  "gfx.content.skia-font-cache-size",
  SkiaContentFontCacheSize,
  int32_t, 5
)

VARCACHE_PREF(
  Once,
  "gfx.device-reset.limit",
  DeviceResetLimitCount,
  int32_t, 10
)

VARCACHE_PREF(
  Once,
  "gfx.device-reset.threshold-ms",
  DeviceResetThresholdMilliseconds,
  int32_t, -1
)


VARCACHE_PREF(
  Once,
  "gfx.direct2d.disabled",
  Direct2DDisabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct2d.force-enabled",
  Direct2DForceEnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.direct2d.destroy-dt-on-paintthread",
  Direct2DDestroyDTOnPaintThread,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.reuse-decoder-device",
  Direct3D11ReuseDecoderDevice,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.allow-keyed-mutex",
  Direct3D11AllowKeyedMutex,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.use-double-buffering",
  Direct3D11UseDoubleBuffering,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.enable-debug-layer",
  Direct3D11EnableDebugLayer,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.break-on-error",
  Direct3D11BreakOnError,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.sleep-on-create-device",
  Direct3D11SleepOnCreateDevice,
  int32_t, 0
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.keep_color_bitmaps",
  KeepColorBitmaps,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.validate_variation_tables",
  ValidateVariationTables,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.otl_validation",
  ValidateOTLTables,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.draw-color-bars",
  CompositorDrawColorBars,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.e10s.hide-plugins-for-scroll",
  HidePluginsForScroll,
  bool, true
)

VARCACHE_PREF(
  Once,
  "gfx.e10s.font-list.shared",
  SharedFontList,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.font_ahem_antialias_none",
  gfx_font_ahem_antialias_none,
  RelaxedAtomicBool, false
)

#if defined(XP_MACOSX)
VARCACHE_PREF(
  Live,
  "gfx.font_rendering.coretext.enabled",
  CoreTextEnabled,
  RelaxedAtomicBool, false
)
#endif

VARCACHE_PREF(
  Live,
  "gfx.font_rendering.opentype_svg.enabled",
  gfx_font_rendering_opentype_svg_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "gfx.layerscope.enabled",
  LayerScopeEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.layerscope.port",
  LayerScopePort,
  RelaxedAtomicInt32, 23456
)

// Note that        "gfx.logging.level" is defined in Logging.h.
VARCACHE_PREF(
  Live,
  "gfx.logging.level",
  GfxLoggingLevel,
  RelaxedAtomicInt32, mozilla::gfx::LOG_DEFAULT
)

VARCACHE_PREF(
  Once,
  "gfx.logging.crash.length",
  GfxLoggingCrashLength,
  uint32_t, 16
)

VARCACHE_PREF(
  Live,
  "gfx.logging.painted-pixel-count.enabled",
  GfxLoggingPaintedPixelCountEnabled,
  RelaxedAtomicBool, false
)

// The maximums here are quite conservative, we can tighten them if problems show up.
VARCACHE_PREF(
  Once,
  "gfx.logging.texture-usage.enabled",
  GfxLoggingTextureUsageEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.logging.peak-texture-usage.enabled",
  GfxLoggingPeakTextureUsageEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.logging.slow-frames.enabled",
  LoggingSlowFramesEnabled,
  bool, false
)

// Use gfxPlatform::MaxAllocSize instead of the pref directly
VARCACHE_PREF(
  Once,
  "gfx.max-alloc-size",
  MaxAllocSizeDoNotUseDirectly,
  int32_t, (int32_t)500000000
)

// Use gfxPlatform::MaxTextureSize instead of the pref directly
VARCACHE_PREF(
  Once,
  "gfx.max-texture-size",
  MaxTextureSizeDoNotUseDirectly,
  int32_t, (int32_t)32767
)

VARCACHE_PREF(
  Live,
  "gfx.offscreencanvas.enabled",
  gfx_offscreencanvas_enabled,
  RelaxedAtomicBool, false
)

#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "gfx.omta.background-color",
  gfx_omta_background_color,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "gfx.partialpresent.force",
  PartialPresent,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "gfx.perf-warnings.enabled",
  PerfWarnings,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.testing.device-fail",
  DeviceFailForTesting,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.testing.device-reset",
  DeviceResetForTesting,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Once,
  "gfx.text.disable-aa",
  DisableAllTextAA,
  bool, false
)

// Disable surface sharing due to issues with compatible FBConfigs on
// NVIDIA drivers as described in bug 1193015.
VARCACHE_PREF(
  Live,
  "gfx.use-glx-texture-from-pixmap",
  UseGLXTextureFromPixmap,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-iosurface-textures",
  UseIOSurfaceTextures,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-mutex-on-present",
  UseMutexOnPresent,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-surfacetexture-textures",
  UseSurfaceTextureTextures,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.vsync.collect-scroll-transforms",
  CollectScrollTransforms,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.vsync.compositor.unobserve-count",
  CompositorUnobserveCount,
  int32_t, 10
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.all",
  WebRenderAll,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.blob-images",
  WebRenderBlobImages,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.blob.paint-flashing",
  WebRenderBlobPaintFlashing,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.dl.dump-parent",
  WebRenderDLDumpParent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.dl.dump-content",
  WebRenderDLDumpContent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.enabled",
  WebRenderEnabledDoNotUseDirectly,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.force-disabled",
  WebRenderForceDisabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.highlight-painted-layers",
  WebRenderHighlightPaintedLayers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.late-scenebuild-threshold",
  WebRenderLateSceneBuildThreshold,
  RelaxedAtomicInt32, 4
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.max-filter-ops-per-chain",
  WebRenderMaxFilterOpsPerChain,
  RelaxedAtomicUint32, 64
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.picture-caching",
  WebRenderPictureCaching,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.split-render-roots",
  WebRenderSplitRenderRoots,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.start-debug-server",
  WebRenderStartDebugServer,
  RelaxedAtomicBool, false
)

// Use vsync events generated by hardware
VARCACHE_PREF(
  Once,
  "gfx.work-around-driver-bugs",
  WorkAroundDriverBugs,
  bool, true
)

VARCACHE_PREF(
  Live,
  "gfx.ycbcr.accurate-conversion",
  YCbCrAccurateConversion,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "gl." (OpenGL)
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "gl.allow-high-power",
  GLAllowHighPower,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gl.ignore-dx-interop2-blacklist",
  IgnoreDXInterop2Blacklist,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gl.msaa-level",
  MSAALevel,
  RelaxedAtomicUint32, 2
)

#if defined(XP_MACOSX)
VARCACHE_PREF(
  Live,
  "gl.multithreaded",
  GLMultithreaded,
  RelaxedAtomicBool, false
)
#endif

VARCACHE_PREF(
  Live,
  "gl.require-hardware",
  RequireHardwareGL,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gl.use-tls-is-current",
  UseTLSIsCurrent,
  RelaxedAtomicInt32, 0
)

//---------------------------------------------------------------------------
// Prefs starting with "html5."
//---------------------------------------------------------------------------

// Toggle which thread the HTML5 parser uses for stream parsing.
VARCACHE_PREF(
  Live,
  "html5.offmainthread",
  html5_offmainthread,
  bool, true
)

// Time in milliseconds between the time a network buffer is seen and the timer
// firing when the timer hasn't fired previously in this parse in the
// off-the-main-thread HTML5 parser.
VARCACHE_PREF(
  Live,
  "html5.flushtimer.initialdelay",
  html5_flushtimer_initialdelay,
  RelaxedAtomicInt32, 16
)

// Time in milliseconds between the time a network buffer is seen and the timer
// firing when the timer has already fired previously in this parse.
VARCACHE_PREF(
  Live,
  "html5.flushtimer.subsequentdelay",
  html5_flushtimer_subsequentdelay,
  RelaxedAtomicInt32, 16
)

//---------------------------------------------------------------------------
// Prefs starting with "image."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "image.animated.decode-on-demand.threshold-kb",
  ImageAnimatedDecodeOnDemandThresholdKB,
  RelaxedAtomicUint32, 20480
)

VARCACHE_PREF(
  Live,
  "image.animated.decode-on-demand.batch-size",
  ImageAnimatedDecodeOnDemandBatchSize,
  RelaxedAtomicUint32, 6
)

VARCACHE_PREF(
  Once,
  "image.animated.decode-on-demand.recycle",
  ImageAnimatedDecodeOnDemandRecycle,
  bool, false
)

VARCACHE_PREF(
  Live,
  "image.animated.resume-from-last-displayed",
  ImageAnimatedResumeFromLastDisplayed,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.cache.factor2.threshold-surfaces",
  ImageCacheFactor2ThresholdSurfaces,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "image.cache.max-rasterized-svg-threshold-kb",
  ImageCacheMaxRasterizedSVGThresholdKB,
  RelaxedAtomicInt32, 90*1024
)

VARCACHE_PREF(
  Once,
  "image.cache.size",
  ImageCacheSize,
  int32_t, 5*1024*1024
)

VARCACHE_PREF(
  Once,
  "image.cache.timeweight",
  ImageCacheTimeWeight,
  int32_t, 500
)

VARCACHE_PREF(
  Live,
  "image.decode-immediately.enabled",
  ImageDecodeImmediatelyEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.downscale-during-decode.enabled",
  ImageDownscaleDuringDecodeEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "image.infer-src-animation.threshold-ms",
  ImageInferSrcAnimationThresholdMS,
  RelaxedAtomicUint32, 2000
)

VARCACHE_PREF(
  Live,
  "image.layout_network_priority",
  ImageLayoutNetworkPriority,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Once,
  "image.mem.decode_bytes_at_a_time",
  ImageMemDecodeBytesAtATime,
  uint32_t, 200000
)

VARCACHE_PREF(
  Live,
  "image.mem.discardable",
  ImageMemDiscardable,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "image.mem.animated.discardable",
  ImageMemAnimatedDiscardable,
  bool, false
)

VARCACHE_PREF(
  Live,
  "image.mem.animated.use_heap",
  ImageMemAnimatedUseHeap,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.mem.debug-reporting",
  ImageMemDebugReporting,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.mem.shared",
  ImageMemShared,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.discard_factor",
  ImageMemSurfaceCacheDiscardFactor,
  uint32_t, 1
)

VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.max_size_kb",
  ImageMemSurfaceCacheMaxSizeKB,
  uint32_t, 100 * 1024
)

VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.min_expiration_ms",
  ImageMemSurfaceCacheMinExpirationMS,
  uint32_t, 60*1000
)

VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.size_factor",
  ImageMemSurfaceCacheSizeFactor,
  uint32_t, 64
)

VARCACHE_PREF(
  Live,
  "image.mem.volatile.min_threshold_kb",
  ImageMemVolatileMinThresholdKB,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Once,
  "image.multithreaded_decoding.idle_timeout",
  ImageMTDecodingIdleTimeout,
  int32_t, -1
)

VARCACHE_PREF(
  Once,
  "image.multithreaded_decoding.limit",
  ImageMTDecodingLimit,
  int32_t, -1
)

VARCACHE_PREF(
  Live,
  "image.webp.enabled",
  ImageWebPEnabled,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "javascript."
//---------------------------------------------------------------------------

// BigInt API
VARCACHE_PREF(
  Live,
  "javascript.options.bigint",
  javascript_options_bigint,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
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
  Live,
  "javascript.options.compact_on_user_inactive_delay",
  javascript_options_compact_on_user_inactive_delay,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "javascript.options.experimental.fields",
  javascript_options_experimental_fields,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "javascript.options.experimental.await_fix",
  javascript_options_experimental_await_fix,
  RelaxedAtomicBool, true
)

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
  Live,
  "javascript.options.gc_on_memory_pressure",
  javascript_options_gc_on_memory_pressure,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "javascript.options.mem.log",
  javascript_options_mem_log,
  bool, false
)

VARCACHE_PREF(
  Live,
  "javascript.options.mem.notify",
  javascript_options_mem_notify,
  bool, false
)

// Streams API
VARCACHE_PREF(
  Live,
  "javascript.options.streams",
  javascript_options_streams,
  RelaxedAtomicBool, false
)

// Whether ISO-2022-JP is a permitted content-based encoding detection
// outcome.
VARCACHE_PREF(
  Live,
  "intl.charset.detector.iso2022jp.allowed",
   intl_charset_detector_iso2022jp_allowed,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "layers."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Once,
  "layers.acceleration.disabled",
  LayersAccelerationDisabledDoNotUseDirectly,
  bool, false
)
// Instead, use gfxConfig::IsEnabled(Feature::HW_COMPOSITING).

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps",
  LayersDrawFPS,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps.print-histogram",
  FPSPrintHistogram,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps.write-to-file",
  WriteFPSToFile,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.acceleration.force-enabled",
  LayersAccelerationForceEnabledDoNotUseDirectly,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.advanced.basic-layer.enabled",
  LayersAdvancedBasicLayerEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.amd-switchable-gfx.enabled",
  LayersAMDSwitchableGfxEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.async-pan-zoom.enabled",
  AsyncPanZoomEnabledDoNotUseDirectly,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layers.bench.enabled",
  LayersBenchEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.bufferrotation.enabled",
  BufferRotationEnabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layers.child-process-shutdown",
  ChildProcessShutdown,
  RelaxedAtomicBool, true
)

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
// If MOZ_GFX_OPTIMIZE_MOBILE is defined, we force component alpha off
// and ignore the preference.
VARCACHE_PREF(
  Skip,
  "layers.componentalpha.enabled",
  ComponentAlphaEnabled,
  bool, false
)
#else
// If MOZ_GFX_OPTIMIZE_MOBILE is not defined, we actually take the
// preference value, defaulting to true.
VARCACHE_PREF(
  Once,
  "layers.componentalpha.enabled",
  ComponentAlphaEnabled,
  bool, true
)
#endif

VARCACHE_PREF(
  Once,
  "layers.d3d11.force-warp",
  LayersD3D11ForceWARP,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.deaa.enabled",
  LayersDEAAEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-bigimage-borders",
  DrawBigImageBorders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-borders",
  DrawLayerBorders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-tile-borders",
  DrawTileBorders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-layer-info",
  DrawLayerInfo,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump",
  LayersDump,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump-texture",
  LayersDumpTexture,
  RelaxedAtomicBool, false
)

#ifdef MOZ_DUMP_PAINTING
VARCACHE_PREF(
  Live,
  "layers.dump-client-layers",
  DumpClientLayers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump-decision",
  LayersDumpDecision,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump-host-layers",
  DumpHostLayers,
  RelaxedAtomicBool, false
)
#endif

// 0 is "no change" for contrast, positive values increase it, negative values
// decrease it until we hit mid gray at -1 contrast, after that it gets weird.
VARCACHE_PREF(
  Live,
  "layers.effect.contrast",
  LayersEffectContrast,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "layers.effect.grayscale",
  LayersEffectGrayscale,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.effect.invert",
  LayersEffectInvert,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.enable-tiles",
  LayersTilesEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.enable-tiles-if-skia-pomtp",
  LayersTilesEnabledIfSkiaPOMTP,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.flash-borders",
  FlashLayerBorders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.force-shmem-tiles",
  ForceShmemTiles,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-mask-debug",
  DrawMaskLayer,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.force-synchronous-resize",
  LayersForceSynchronousResize,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layers.geometry.opengl.enabled",
  OGLLayerGeometry,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.geometry.basic.enabled",
  BasicLayerGeometry,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.geometry.d3d11.enabled",
  D3D11LayerGeometry,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.allow-software",
  GPUProcessAllowSoftware,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.enabled",
  GPUProcessEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.force-enabled",
  GPUProcessForceEnabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.ipc_reply_timeout_ms",
  GPUProcessIPCReplyTimeoutMs,
  int32_t, 10000
)

VARCACHE_PREF(
  Live,
  "layers.gpu-process.max_restarts",
  GPUProcessMaxRestarts,
  RelaxedAtomicInt32, 1
)

// Note: This pref will only be used if it is less than layers.gpu-process.max_restarts.
VARCACHE_PREF(
  Live,
  "layers.gpu-process.max_restarts_with_decoder",
  GPUProcessMaxRestartsWithDecoder,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.startup_timeout_ms",
  GPUProcessTimeoutMs,
  int32_t, 5000
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-buffer",
  UseLowPrecisionBuffer,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-opacity",
  LowPrecisionOpacity,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-resolution",
  LowPrecisionResolution,
  AtomicFloat, 0.25f
)

VARCACHE_PREF(
  Live,
  "layers.max-active",
  MaxActiveLayers,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enabled",
  AdvancedLayersEnabledDoNotUseDirectly,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-buffer-cache",
  AdvancedLayersEnableBufferCache,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-buffer-sharing",
  AdvancedLayersEnableBufferSharing,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-clear-view",
  AdvancedLayersEnableClearView,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-cpu-occlusion",
  AdvancedLayersEnableCPUOcclusion,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-depth-buffer",
  AdvancedLayersEnableDepthBuffer,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.mlgpu.enable-invalidation",
  AdvancedLayersUseInvalidation,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-on-windows7",
  AdvancedLayersEnableOnWindows7,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-container-resizing",
  AdvancedLayersEnableContainerResizing,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.offmainthreadcomposition.force-disabled",
  LayersOffMainThreadCompositionForceDisabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.offmainthreadcomposition.frame-rate",
  LayersCompositionFrameRate,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Once,
  "layers.omtp.capture-limit",
  LayersOMTPCaptureLimit,
  uint32_t, 25 * 1024 * 1024
)

VARCACHE_PREF(
  Live,
  "layers.omtp.dump-capture",
  LayersOMTPDumpCapture,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.omtp.paint-workers",
  LayersOMTPPaintWorkers,
  int32_t, 1
)

VARCACHE_PREF(
  Live,
  "layers.omtp.release-capture-on-main-thread",
  LayersOMTPReleaseCaptureOnMainThread,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.orientation.sync.timeout",
  OrientationSyncMillis,
  RelaxedAtomicUint32, (uint32_t)0
)

VARCACHE_PREF(
  Once,
  "layers.prefer-opengl",
  LayersPreferOpenGL,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.progressive-paint",
  ProgressivePaint,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.shared-buffer-provider.enabled",
  PersistentBufferProviderSharedEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.single-tile.enabled",
  LayersSingleTileEnabled,
  RelaxedAtomicBool, true
)

// We allow for configurable and rectangular tile size to avoid wasting memory
// on devices whose screen size does not align nicely to the default tile size.
// Although layers can be any size, they are often the same size as the screen,
// especially for width.
VARCACHE_PREF(
  Once,
  "layers.tile-width",
  LayersTileWidth,
  int32_t, 256
)

VARCACHE_PREF(
  Once,
  "layers.tile-height",
  LayersTileHeight,
  int32_t, 256
)

VARCACHE_PREF(
  Once,
  "layers.tile-initial-pool-size",
  LayersTileInitialPoolSize,
  uint32_t, (uint32_t)50
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-unused-size",
  LayersTilePoolUnusedSize,
  uint32_t, (uint32_t)10
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-shrink-timeout",
  LayersTilePoolShrinkTimeout,
  uint32_t, (uint32_t)50
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-clear-timeout",
  LayersTilePoolClearTimeout,
  uint32_t, (uint32_t)5000
)

VARCACHE_PREF(
  Once,
  "layers.tiles.adjust",
  LayersTilesAdjust,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.tiles.edge-padding",
  TileEdgePaddingEnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.tiles.fade-in.enabled",
  LayerTileFadeInEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.tiles.fade-in.duration-ms",
  LayerTileFadeInDuration,
  RelaxedAtomicUint32, 250
)

VARCACHE_PREF(
  Live,
  "layers.tiles.retain-back-buffer",
  LayersTileRetainBackBuffer,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layers.transaction.warning-ms",
  LayerTransactionWarning,
  RelaxedAtomicUint32, 200
)

VARCACHE_PREF(
  Once,
  "layers.uniformity-info",
  UniformityInfo,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.use-image-offscreen-surfaces",
  UseImageOffscreenSurfaces,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "layout."
//---------------------------------------------------------------------------

// Debug-only pref to force enable the AccessibleCaret. If you want to
// control AccessibleCaret by mouse, you'll need to set
// "layout.accessiblecaret.hide_carets_for_mouse_input" to false.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.enabled",
  layout_accessiblecaret_enabled,
  bool, false
)

// Enable the accessible caret on platforms/devices
// that we detect have touch support. Note that this pref is an
// additional way to enable the accessible carets, rather than
// overriding the layout.accessiblecaret.enabled pref.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.enabled_on_touch",
  layout_accessiblecaret_enabled_on_touch,
  bool, true
)

// By default, carets become tilt only when they are overlapping.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.always_tilt",
  layout_accessiblecaret_always_tilt,
  bool, false
)

// Show caret in cursor mode when long tapping on an empty content. This
// also changes the default update behavior in cursor mode, which is based
// on the emptiness of the content, into something more heuristic. See
// AccessibleCaretManager::UpdateCaretsForCursorMode() for the details.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content",
  layout_accessiblecaret_caret_shown_when_long_tapping_on_empty_content,
  bool, false
)

// 0 = by default, always hide carets for selection changes due to JS calls.
// 1 = update any visible carets for selection changes due to JS calls,
//     but don't show carets if carets are hidden.
// 2 = always show carets for selection changes due to JS calls.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.script_change_update_mode",
  layout_accessiblecaret_script_change_update_mode,
  int32_t, 0
)

// Allow one caret to be dragged across the other caret without any limitation.
// This matches the built-in convention for all desktop platforms.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.allow_dragging_across_other_caret",
  layout_accessiblecaret_allow_dragging_across_other_caret,
  bool, true
)

// Optionally provide haptic feedback on long-press selection events.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.hapticfeedback",
  layout_accessiblecaret_hapticfeedback,
  bool, false
)

// Smart phone-number selection on long-press is not enabled by default.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.extend_selection_for_phone_number",
  layout_accessiblecaret_extend_selection_for_phone_number,
  bool, false
)

// Keep the accessible carets hidden when the user is using mouse input (as
// opposed to touch/pen/etc.).
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.hide_carets_for_mouse_input",
  layout_accessiblecaret_hide_carets_for_mouse_input,
  bool, true
)

// CSS attributes (width, height, margin-left) of the AccessibleCaret in CSS
// pixels.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.width",
  layout_accessiblecaret_width,
  float, 34.0f
)

VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.height",
  layout_accessiblecaret_height,
  float, 36.0f
)

VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.margin-left",
  layout_accessiblecaret_margin_left,
  float, -18.5f
)

// Simulate long tap events to select words. Mainly used in manual testing
// with mouse.
VARCACHE_PREF(
  Live,
  "layout.accessiblecaret.use_long_tap_injector",
  layout_accessiblecaret_use_long_tap_injector,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.partial",
  PartiallyPrerenderAnimatedContent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.viewport-ratio-limit-x",
  AnimationPrerenderViewportRatioLimitX,
  AtomicFloat, 1.125f
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.viewport-ratio-limit-y",
  AnimationPrerenderViewportRatioLimitY,
  AtomicFloat, 1.125f
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.absolute-limit-x",
  AnimationPrerenderAbsoluteLimitX,
  RelaxedAtomicUint32, 4096
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.absolute-limit-y",
  AnimationPrerenderAbsoluteLimitY,
  RelaxedAtomicUint32, 4096
)

// Is path() supported in clip-path?
VARCACHE_PREF(
  Live,
  "layout.css.clip-path-path.enabled",
  layout_css_clip_path_path_enabled,
  bool, false
)

// Is support for CSS column-span enabled?
VARCACHE_PREF(
  Live,
  "layout.css.column-span.enabled",
  layout_css_column_span_enabled,
  bool, false
)

// Is support for CSS contain enabled?
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.contain.enabled",
  layout_css_contain_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Should stray control characters be rendered visibly?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "layout.css.control-characters.visible",
  layout_css_control_characters_visible,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for DOMMatrix enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMMatrix.enabled",
  layout_css_DOMMatrix_enabled,
  bool, true
)

// Is support for DOMQuad enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMQuad.enabled",
  layout_css_DOMQuad_enabled,
  bool, true
)

// Is support for DOMPoint enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMPoint.enabled",
  layout_css_DOMPoint_enabled,
  bool, true
)

// Are we emulating -moz-{inline}-box layout using CSS flexbox?
VARCACHE_PREF(
  Live,
  "layout.css.emulate-moz-box-with-flex",
  layout_css_emulate_moz_box_with_flex,
  bool, false
)

// Is support for the font-display @font-face descriptor enabled?
VARCACHE_PREF(
  Live,
  "layout.css.font-display.enabled",
  layout_css_font_display_enabled,
  bool, true
)

// Is support for document.fonts enabled?
VARCACHE_PREF(
  Live,
  "layout.css.font-loading-api.enabled",
  layout_css_font_loading_api_enabled,
  bool, true
)

// Is support for variation fonts enabled?
VARCACHE_PREF(
  Live,
  "layout.css.font-variations.enabled",
  layout_css_font_variations_enabled,
  RelaxedAtomicBool, true
)

// Is support for GeometryUtils.getBoxQuads enabled?
#ifdef RELEASE_OR_BETA
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "layout.css.getBoxQuads.enabled",
  layout_css_getBoxQuads_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for CSS "grid-template-{columns,rows}: subgrid X" enabled?
#ifdef NIGHTLY_BUILD
# define PREF_VALUE  true
#else
# define PREF_VALUE  false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.grid-template-subgrid-value.enabled",
  layout_css_grid_template_subgrid_value_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Pref to control whether line-height: -moz-block-height is exposed to content.
VARCACHE_PREF(
  Live,
  "layout.css.line-height-moz-block-height.content.enabled",
  layout_css_line_height_moz_block_height_content_enabled,
  bool, false
)

// Pref to control whether @-moz-document rules are enabled in content pages.
VARCACHE_PREF(
  Live,
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
  Live,
  "layout.css.moz-document.url-prefix-hack.enabled",
  layout_css_moz_document_url_prefix_hack_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Whether the offset-* logical property aliases are enabled.
VARCACHE_PREF(
  Live,
  "layout.css.offset-logical-properties.enabled",
  layout_css_offset_logical_properties_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layout.css.paint-order.enabled",
  PaintOrderEnabled,
  RelaxedAtomicBool, false
)

// Is parallel CSS parsing enabled?
VARCACHE_PREF(
  Live,
  "layout.css.parsing.parallel",
  layout_css_parsing_parallel,
  bool, true
)

// Are "-webkit-{min|max}-device-pixel-ratio" media queries supported? (Note:
// this pref has no effect if the master 'layout.css.prefixes.webkit' pref is
// set to false.)
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.device-pixel-ratio-webkit",
  layout_css_prefixes_device_pixel_ratio_webkit,
  bool, true
)

// Are webkit-prefixed properties & property-values supported?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.webkit",
  layout_css_prefixes_webkit,
  bool, true
)

// Is CSS error reporting enabled?
VARCACHE_PREF(
  Live,
  "layout.css.report_errors",
  layout_css_report_errors,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.damping-ratio",
  ScrollBehaviorDampingRatio,
  AtomicFloat, 1.0f
)

// Are -moz-prefixed gradients restricted to a simpler syntax? (with an optional
// <angle> or <position>, but not both)?
VARCACHE_PREF(
  Live,
  "layout.css.simple-moz-gradient.enabled",
  layout_css_simple_moz_gradient_enabled,
  bool, true
)

#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.supports-selector.enabled",
  layout_css_supports_selector_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.enabled",
  ScrollBehaviorEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.spring-constant",
  ScrollBehaviorSpringConstant,
  AtomicFloat, 250.0f
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.prediction-max-velocity",
  ScrollSnapPredictionMaxVelocity,
  RelaxedAtomicInt32, 2000
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.prediction-sensitivity",
  ScrollSnapPredictionSensitivity,
  AtomicFloat, 0.750f
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.proximity-threshold",
  ScrollSnapProximityThreshold,
  RelaxedAtomicInt32, 200
)

// Is steps(jump-*) supported in easing functions?
VARCACHE_PREF(
  Live,
  "layout.css.step-position-jump.enabled",
  layout_css_step_position_jump_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.touch_action.enabled",
  TouchActionEnabled,
  RelaxedAtomicBool, false
)

// Does arbitrary ::-webkit-* pseudo-element parsed?
VARCACHE_PREF(
  Live,
  "layout.css.unknown-webkit-pseudo-element",
  layout_css_unknown_webkit_pseudo_element,
  bool, true
)

// Are style system use counters enabled?
#ifdef RELEASE_OR_BETA
#define PREF_VALUE false
#else
#define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "layout.css.use-counters.enabled",
  layout_css_use_counters_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Should the :visited selector ever match (otherwise :link matches instead)?
VARCACHE_PREF(
  Live,
  "layout.css.visited_links_enabled",
  layout_css_visited_links_enabled,
  bool, true
)

// Is the '-webkit-appearance' alias for '-moz-appearance' enabled?
VARCACHE_PREF(
  Live,
  "layout.css.webkit-appearance.enabled",
  layout_css_webkit_appearance_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.xul-display-values.content.enabled",
  layout_css_xul_display_values_content_enabled,
  bool, false
)

// Pref to control whether display: -moz-box and display: -moz-inline-box are
// parsed in content pages.
VARCACHE_PREF(
  Live,
  "layout.css.xul-box-display-values.content.enabled",
  layout_css_xul_box_display_values_content_enabled,
  bool, false
)

// Pref to control whether XUL ::-tree-* pseudo-elements are parsed in content
// pages.
VARCACHE_PREF(
  Live,
  "layout.css.xul-tree-pseudos.content.enabled",
  layout_css_xul_tree_pseudos_content_enabled,
  bool, false
)

// Whether to block large cursors intersecting UI.
VARCACHE_PREF(
  Live,
  "layout.cursor.block.enabled",
  layout_cursor_block_enabled,
  bool, true
)

// The maximum width or height of the cursor we should allow when intersecting
// the UI, in CSS pixels.
VARCACHE_PREF(
  Live,
  "layout.cursor.block.max-size",
  layout_cursor_block_max_size,
  uint32_t, 32
)

VARCACHE_PREF(
  Live,
  "layout.display-list.build-twice",
  LayoutDisplayListBuildTwice,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain",
  LayoutRetainDisplayList,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.chrome",
  LayoutRetainDisplayListChrome,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.verify",
  LayoutVerifyRetainDisplayList,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.verify.order",
  LayoutVerifyRetainDisplayListOrder,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.rebuild-frame-limit",
  LayoutRebuildFrameLimit,
  RelaxedAtomicUint32, 500
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump",
  LayoutDumpDisplayList,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump-content",
  LayoutDumpDisplayListContent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump-parent",
  LayoutDumpDisplayListParent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.show-rebuild-area",
  LayoutDisplayListShowArea,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.flatten-transform",
  LayoutFlattenTransform,
  RelaxedAtomicBool, true
)

// Are dynamic reflow roots enabled?
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.dynamic-reflow-roots.enabled",
  layout_dynamic_reflow_roots_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "layout.frame_rate",
  LayoutFrameRate,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "layout.min-active-layer-size",
  LayoutMinActiveLayerSize,
  int, 64
)

VARCACHE_PREF(
  Once,
  "layout.paint_rects_separately",
  LayoutPaintRectsSeparately,
  bool, true
)

// This and code dependent on it should be removed once containerless scrolling looks stable.
VARCACHE_PREF(
  Live,
  "layout.scroll.root-frame-containers",
  LayoutUseContainersForRootFrames,
  RelaxedAtomicBool, false
)

// This pref is to be set by test code only.
VARCACHE_PREF(
  Live,
  "layout.scrollbars.always-layerize-track",
  AlwaysLayerizeScrollbarTrackTestOnly,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.smaller-painted-layers",
  LayoutSmallerPaintedLayers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.lower_priority_refresh_driver_during_load",
  layout_lower_priority_refresh_driver_during_load,
  bool, true
)

// Pref to control enabling scroll anchoring.
VARCACHE_PREF(
  Live,
  "layout.css.scroll-anchoring.enabled",
  layout_css_scroll_anchoring_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-anchoring.highlight",
  layout_css_scroll_anchoring_highlight,
  bool, false
)

// Is the CSS Scroll Snap Module Level 1 enabled?
VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap-v1.enabled",
  layout_css_scroll_snap_v1_enabled,
  RelaxedAtomicBool, true
)

// Is support for the old unspecced scroll-snap enabled?
// E.g. scroll-snap-points-{x,y}, scroll-snap-coordinate, etc.
VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.enabled",
  layout_css_scroll_snap_enabled,
  bool, false
)

// Are shared memory User Agent style sheets enabled?
VARCACHE_PREF(
  Live,
  "layout.css.shared-memory-ua-sheets.enabled",
  layout_css_shared_memory_ua_sheets_enabled,
  bool, false
)

#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.resizeobserver.enabled",
  layout_css_resizeobserver_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for -webkit-line-clamp enabled?
VARCACHE_PREF(
  Live,
  "layout.css.webkit-line-clamp.enabled",
  layout_css_webkit_line_clamp_enabled,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "media."
//---------------------------------------------------------------------------

// These prefs use camel case instead of snake case for the getter because one
// reviewer had an unshakeable preference for that. Who could that be?

VARCACHE_PREF(
  Live,
  "media.autoplay.allow-muted",
  MediaAutoplayAllowMuted,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.autoplay.blackList-override-default",
  MediaAutoplayBlackListOverrideDefault,
  RelaxedAtomicBool, true
)

// File-backed MediaCache size.
VARCACHE_PREF(
  Live,
  "media.cache_size",
  MediaCacheSize,
  RelaxedAtomicUint32, 512000 // Measured in KiB
)

// Size of file backed MediaCache while on a connection which is cellular (3G,
// etc), and thus assumed to be "expensive".
VARCACHE_PREF(
  Live,
  "media.cache_size.cellular",
  MediaCacheCellularSize,
  RelaxedAtomicUint32, 32768 // Measured in KiB
)

// If a resource is known to be smaller than this size (in kilobytes), a
// memory-backed MediaCache may be used; otherwise the (single shared global)
// file-backed MediaCache is used.
VARCACHE_PREF(
  Live,
  "media.memory_cache_max_size",
  MediaMemoryCacheMaxSize,
  uint32_t, 8192      // Measured in KiB
)

// Don't create more memory-backed MediaCaches if their combined size would go
// above this absolute size limit.
VARCACHE_PREF(
  Live,
  "media.memory_caches_combined_limit_kb",
  MediaMemoryCachesCombinedLimitKb,
  uint32_t, 524288
)

// Don't create more memory-backed MediaCaches if their combined size would go
// above this relative size limit (a percentage of physical memory).
VARCACHE_PREF(
  Live,
  "media.memory_caches_combined_limit_pc_sysmem",
  MediaMemoryCachesCombinedLimitPcSysmem,
  uint32_t, 5         // A percentage
)

// When a network connection is suspended, don't resume it until the amount of
// buffered data falls below this threshold (in seconds).
VARCACHE_PREF(
  Live,
  "media.cache_resume_threshold",
  MediaCacheResumeThreshold,
  RelaxedAtomicUint32, 30
)
VARCACHE_PREF(
  Live,
  "media.cache_resume_threshold.cellular",
  MediaCacheCellularResumeThreshold,
  RelaxedAtomicUint32, 10
)

// Stop reading ahead when our buffered data is this many seconds ahead of the
// current playback position. This limit can stop us from using arbitrary
// amounts of network bandwidth prefetching huge videos.
VARCACHE_PREF(
  Live,
  "media.cache_readahead_limit",
  MediaCacheReadaheadLimit,
  RelaxedAtomicUint32, 60
)
VARCACHE_PREF(
  Live,
  "media.cache_readahead_limit.cellular",
  MediaCacheCellularReadaheadLimit,
  RelaxedAtomicUint32, 30
)

// AudioSink
VARCACHE_PREF(
  Live,
  "media.resampling.enabled",
  MediaResamplingEnabled,
  RelaxedAtomicBool, false
)

#if defined(XP_WIN) || defined(XP_DARWIN) || defined(MOZ_PULSEAUDIO)
// libcubeb backend implement .get_preferred_channel_layout
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "media.forcestereo.enabled",
  MediaForcestereoEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// VideoSink
VARCACHE_PREF(
  Live,
  "media.ruin-av-sync.enabled",
  MediaRuinAvSyncEnabled,
  RelaxedAtomicBool, false
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
  Live,
  "media.eme.enabled",
  MediaEmeEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.clearkey.persistent-license.enabled",
  MediaClearkeyPersistentLicenseEnabled,
  bool, false
)

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
// Whether to allow, on a Linux system that doesn't support the necessary
// sandboxing features, loading Gecko Media Plugins unsandboxed.  However, EME
// CDMs will not be loaded without sandboxing even if this pref is changed.
VARCACHE_PREF(
  Live,
  "media.gmp.insecure.allow",
  MediaGmpInsecureAllow,
  RelaxedAtomicBool, false
)
#endif

// Specifies whether the PDMFactory can create a test decoder that just outputs
// blank frames/audio instead of actually decoding. The blank decoder works on
// all platforms.
VARCACHE_PREF(
  Live,
  "media.use-blank-decoder",
  MediaUseBlankDecoder,
  RelaxedAtomicBool, false
)

#if defined(XP_WIN)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.gpu-process-decoder",
  MediaGpuProcessDecoder,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#if defined(XP_WIN)
# if defined(_ARM64_) || defined(__MINGW32__)
#  define PREF_VALUE false
# else
#  define PREF_VALUE true
# endif
#elif defined(XP_MACOSX)
# define PREF_VALUE true
#elif defined(XP_LINUX) && !defined(ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.rdd-process.enabled",
  MediaRddProcessEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.rdd-process.startup_timeout_ms",
  MediaRddProcessStartupTimeoutMs,
  RelaxedAtomicInt32, 5000
)

#if defined(XP_LINUX) && !defined(ANDROID)
# define PREF_VALUE true
#elif defined(XP_WIN) && !defined(_ARM64_)
# define PREF_VALUE false
#elif defined(XP_MACOSX)
# define PREF_VALUE false
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.rdd-vorbis.enabled",
  MediaRddVorbisEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef ANDROID

// Enable the MediaCodec PlatformDecoderModule by default.
VARCACHE_PREF(
  Live,
  "media.android-media-codec.enabled",
  MediaAndroidMediaCodecEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.android-media-codec.preferred",
  MediaAndroidMediaCodecPreferred,
  RelaxedAtomicBool, true
)

#endif // ANDROID

#ifdef MOZ_OMX
VARCACHE_PREF(
  Live,
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
  Live,
  "media.ffmpeg.enabled",
  MediaFfmpegEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.libavcodec.allow-obsolete",
  MediaLibavcodecAllowObsolete,
  bool, false
)

#endif // MOZ_FFMPEG

#ifdef MOZ_FFVPX
VARCACHE_PREF(
  Live,
  "media.ffvpx.enabled",
  MediaFfvpxEnabled,
  RelaxedAtomicBool, true
)
#endif

#if defined(MOZ_FFMPEG) || defined(MOZ_FFVPX)
VARCACHE_PREF(
  Live,
  "media.ffmpeg.low-latency.enabled",
  MediaFfmpegLowLatencyEnabled,
  RelaxedAtomicBool, false
)
#endif

#ifdef MOZ_WMF

VARCACHE_PREF(
  Live,
  "media.wmf.enabled",
  MediaWmfEnabled,
  RelaxedAtomicBool, true
)

// Whether DD should consider WMF-disabled a WMF failure, useful for testing.
VARCACHE_PREF(
  Live,
  "media.decoder-doctor.wmf-disabled-is-failure",
  MediaDecoderDoctorWmfDisabledIsFailure,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.dxva.d3d11.enabled",
  PDMWMFAllowD3D11,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.dxva.max-videos",
  PDMWMFMaxDXVAVideos,
  RelaxedAtomicUint32, 8
)

VARCACHE_PREF(
  Live,
  "media.wmf.use-nv12-format",
  PDMWMFUseNV12Format,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.force.allow-p010-format",
  PDMWMFForceAllowP010Format,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "media.wmf.use-sync-texture",
  PDMWMFUseSyncTexture,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.low-latency.enabled",
  PDMWMFLowLatencyEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.low-latency.force-disabled",
  PDMWMFLowLatencyForceDisabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.skip-blacklist",
  PDMWMFSkipBlacklist,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.deblacklisting-for-telemetry-in-gpu-process",
  PDMWMFDeblacklistingForTelemetryInGPUProcess,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.amd.highres.enabled",
  PDMWMFAMDHighResEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.allow-unsupported-resolutions",
  PDMWMFAllowUnsupportedResolutions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "media.wmf.vp9.enabled",
  MediaWmfVp9Enabled,
  bool, true
)

#endif // MOZ_WMF

VARCACHE_PREF(
  Once,
  "media.hardware-video-decoding.force-enabled",
  HardwareVideoDecodingForceEnabled,
  bool, false
)

// Whether to check the decoder supports recycling.
#ifdef ANDROID
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.decoder.recycle.enabled",
  MediaDecoderRecycleEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Should MFR try to skip to the next key frame?
VARCACHE_PREF(
  Live,
  "media.decoder.skip-to-next-key-frame.enabled",
  MediaDecoderSkipToNextKeyFrameEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.gmp.decoder.enabled",
  MediaGmpDecoderEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.eme.audio.blank",
  MediaEmeAudioBlank,
  RelaxedAtomicBool, false
)
VARCACHE_PREF(
  Live,
  "media.eme.video.blank",
  MediaEmeVideoBlank,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.eme.chromium-api.video-shmems",
  MediaEmeChromiumApiVideoShmems,
  RelaxedAtomicUint32, 6
)

// Whether to suspend decoding of videos in background tabs.
VARCACHE_PREF(
  Live,
  "media.suspend-bkgnd-video.enabled",
  MediaSuspendBkgndVideoEnabled,
  RelaxedAtomicBool, true
)

// Delay, in ms, from time window goes to background to suspending
// video decoders. Defaults to 10 seconds.
VARCACHE_PREF(
  Live,
  "media.suspend-bkgnd-video.delay-ms",
  MediaSuspendBkgndVideoDelayMs,
  RelaxedAtomicUint32, 10000
)

VARCACHE_PREF(
  Live,
  "media.dormant-on-pause-timeout-ms",
  MediaDormantOnPauseTimeoutMs,
  RelaxedAtomicInt32, 5000
)

// AudioTrack and VideoTrack support
VARCACHE_PREF(
  Live,
  "media.track.enabled",
  media_track_enabled,
  bool, false
)

// TextTrack WebVTT Region extension support.
VARCACHE_PREF(
  Live,
  "media.webvtt.regions.enabled",
  media_webvtt_regions_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.webspeech.synth.force_global_queue",
  MediaWebspeechSynthForceGlobalQueue,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.enable",
  MediaWebspeechTestEnable,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.fake_fsm_events",
  MediaWebspeechTextFakeFsmEvents,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.fake_recognition_service",
  MediaWebspeechTextFakeRecognitionService,
  bool, false
)

#ifdef MOZ_WEBSPEECH
VARCACHE_PREF(
  Live,
  "media.webspeech.recognition.enable",
  media_webspeech_recognition_enable,
  bool, false
)
#endif

VARCACHE_PREF(
  Live,
  "media.webspeech.recognition.force_enable",
  MediaWebspeechRecognitionForceEnable,
  bool, false
)

#ifdef MOZ_WEBSPEECH
VARCACHE_PREF(
  Live,
  "media.webspeech.synth.enabled",
  media_webspeech_synth_enabled,
  bool, false
)
#endif // MOZ_WEBSPEECH

#if defined(MOZ_WEBM_ENCODER)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.encoder.webm.enabled",
  MediaEncoderWebMEnabled,
  RelaxedAtomicBool, true
)
#undef PREF_VALUE

#if defined(RELEASE_OR_BETA)
# define PREF_VALUE 3
#else
  // Zero tolerance in pre-release builds to detect any decoder regression.
# define PREF_VALUE 0
#endif
VARCACHE_PREF(
  Live,
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
  Live,
  "media.video-max-decode-error",
  MediaVideoMaxDecodeError,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

// Opus
VARCACHE_PREF(
  Live,
  "media.opus.enabled",
  MediaOpusEnabled,
  RelaxedAtomicBool, true
)

// Wave
VARCACHE_PREF(
  Live,
  "media.wave.enabled",
  MediaWaveEnabled,
  RelaxedAtomicBool, true
)

// Ogg
VARCACHE_PREF(
  Live,
  "media.ogg.enabled",
  MediaOggEnabled,
  RelaxedAtomicBool, true
)

// WebM
VARCACHE_PREF(
  Live,
  "media.webm.enabled",
  MediaWebMEnabled,
  RelaxedAtomicBool, true
)

// AV1
#if defined(XP_WIN) && !defined(_ARM64_)
# define PREF_VALUE true
#elif defined(XP_MACOSX)
# define PREF_VALUE true
#elif defined(XP_UNIX) && !defined(Android)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.av1.enabled",
  MediaAv1Enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.av1.use-dav1d",
  MediaAv1UseDav1d,
#if defined(XP_WIN) && !defined(_ARM64_)
  RelaxedAtomicBool, true
#elif defined(XP_MACOSX)
  RelaxedAtomicBool, true
#elif defined(XP_UNIX) && !defined(Android)
  RelaxedAtomicBool, true
#else
  RelaxedAtomicBool, false
#endif
)

VARCACHE_PREF(
  Live,
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
  Live,
  "media.hls.enabled",
  MediaHlsEnabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Max number of HLS players that can be created concurrently. Used only on
// Android and when "media.hls.enabled" is true.
#ifdef ANDROID
VARCACHE_PREF(
  Live,
  "media.hls.max-allocations",
  MediaHlsMaxAllocations,
  uint32_t, 20
)
#endif

#ifdef MOZ_FMP4
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.mp4.enabled",
  MediaMp4Enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Error/warning handling, Decoder Doctor.
//
// Set to true to force demux/decode warnings to be treated as errors.
VARCACHE_PREF(
  Live,
  "media.playback.warnings-as-errors",
  MediaPlaybackWarningsAsErrors,
  RelaxedAtomicBool, false
)

// Resume video decoding when the cursor is hovering on a background tab to
// reduce the resume latency and improve the user experience.
VARCACHE_PREF(
  Live,
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
  Live,
  "media.videocontrols.lock-video-orientation",
  MediaVideocontrolsLockVideoOrientation,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Media Seamless Looping
VARCACHE_PREF(
  Live,
  "media.seamless-looping",
  MediaSeamlessLooping,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.autoplay.block-event.enabled",
  MediaBlockEventEnabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.media-capabilities.enabled",
  MediaCapabilitiesEnabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.media-capabilities.screen.enabled",
  MediaCapabilitiesScreenEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.fps",
  MediaBenchmarkVp9Fps,
  RelaxedAtomicUint32, 0
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.threshold",
  MediaBenchmarkVp9Threshold,
  RelaxedAtomicUint32, 150
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.versioncheck",
  MediaBenchmarkVp9Versioncheck,
  RelaxedAtomicUint32, 0
)

VARCACHE_PREF(
  Live,
  "media.benchmark.frames",
  MediaBenchmarkFrames,
  RelaxedAtomicUint32, 300
)

VARCACHE_PREF(
  Live,
  "media.benchmark.timeout",
  MediaBenchmarkTimeout,
  RelaxedAtomicUint32, 1000
)

VARCACHE_PREF(
  Live,
  "media.test.video-suspend",
  MediaTestVideoSuspend,
  RelaxedAtomicBool, false
)

// MediaCapture prefs follow

// Enables navigator.mediaDevices and getUserMedia() support. See also
// media.peerconnection.enabled
VARCACHE_PREF(
  Live,
  "media.navigator.enabled",
  media_navigator_enabled,
  bool, true
)

// This pref turns off [SecureContext] on the navigator.mediaDevices object, for
// more compatible legacy behavior.
VARCACHE_PREF(
  Live,
  "media.devices.insecure.enabled",
  media_devices_insecure_enabled,
  bool, true
)

// If the above pref is also enabled, this pref enabled getUserMedia() support
// in http, bypassing the instant NotAllowedError you get otherwise.
VARCACHE_PREF(
  Live,
  "media.getusermedia.insecure.enabled",
  media_getusermedia_insecure_enabled,
  bool, false
)

// WebRTC prefs follow

// Enables RTCPeerConnection support. Note that, when true, this pref enables
// navigator.mediaDevices and getUserMedia() support as well.
// See also media.navigator.enabled
VARCACHE_PREF(
  Live,
  "media.peerconnection.enabled",
  media_peerconnection_enabled,
  bool, true
)

#ifdef MOZ_WEBRTC
#ifdef ANDROID

VARCACHE_PREF(
  Live,
  "media.navigator.hardware.vp8_encode.acceleration_remote_enabled",
  MediaNavigatorHardwareVp8encodeAccelerationRemoteEnabled,
  bool, true
)

PREF("media.navigator.hardware.vp8_encode.acceleration_enabled", bool, true)

PREF("media.navigator.hardware.vp8_decode.acceleration_enabled", bool, false)

#endif // ANDROID

// Use MediaDataDecoder API for VP8/VP9 in WebRTC. This includes hardware
// acceleration for decoding.
// disable on android bug 1509316
#if defined(NIGHTLY_BUILD) && !defined(ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.navigator.mediadatadecoder_vpx_enabled",
  MediaNavigatorMediadatadecoderVPXEnabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Use MediaDataDecoder API for H264 in WebRTC. This includes hardware
// acceleration for decoding.
# if defined(ANDROID)
#  define PREF_VALUE false // Bug 1509316
# else
#  define PREF_VALUE true
# endif

VARCACHE_PREF(
  Live,
  "media.navigator.mediadatadecoder_h264_enabled",
  MediaNavigatorMediadatadecoderH264Enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#endif // MOZ_WEBRTC

//---------------------------------------------------------------------------
// Prefs starting with "mousewheel."
//---------------------------------------------------------------------------

// These affect how line scrolls from wheel events will be accelerated.
VARCACHE_PREF(
  Live,
  "mousewheel.acceleration.factor",
  MouseWheelAccelerationFactor,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "mousewheel.acceleration.start",
  MouseWheelAccelerationStart,
  RelaxedAtomicInt32, -1
)

// This affects whether events will be routed through APZ or not.
VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.enabled",
  MouseWheelHasRootScrollDeltaOverride,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.horizontal.factor",
  MouseWheelRootScrollHorizontalFactor,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.vertical.factor",
  MouseWheelRootScrollVerticalFactor,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "mousewheel.transaction.ignoremovedelay",
  MouseWheelIgnoreMoveDelayMs,
  RelaxedAtomicInt32, (int32_t)100
)

VARCACHE_PREF(
  Live,
  "mousewheel.transaction.timeout",
  MouseWheelTransactionTimeoutMs,
  RelaxedAtomicInt32, (int32_t)1500
)

//---------------------------------------------------------------------------
// Prefs starting with "network."
//---------------------------------------------------------------------------

// Sub-resources HTTP-authentication:
//   0 - don't allow sub-resources to open HTTP authentication credentials
//       dialogs
//   1 - allow sub-resources to open HTTP authentication credentials dialogs,
//       but don't allow it for cross-origin sub-resources
//   2 - allow the cross-origin authentication as well.
VARCACHE_PREF(
  Live,
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
  Live,
  "network.auth.subresource-img-cross-origin-http-auth-allow",
  network_auth_subresource_img_cross_origin_http_auth_allow,
  bool, false
)

// Resources that are triggered by some non-web-content:
// - true: They are allow to present http auth. dialog
// - false: They are not allow to present http auth. dialog.
VARCACHE_PREF(
  Live,
  "network.auth.non-web-content-triggered-resources-http-auth-allow",
  network_auth_non_web_content_triggered_resources_http_auth_allow,
  bool, false
)

// 0-Accept, 1-dontAcceptForeign, 2-dontAcceptAny, 3-limitForeign,
// 4-rejectTracker
// Keep the old default of accepting all cookies
VARCACHE_PREF(
  Live,
  "network.cookie.cookieBehavior",
  network_cookie_cookieBehavior,
  RelaxedAtomicInt32, 0
)

// Stale threshold for cookies in seconds.
VARCACHE_PREF(
  Live,
  "network.cookie.staleThreshold",
  network_cookie_staleThreshold,
  uint32_t, 60
)

// Cookie lifetime policy. Possible values:
// 0 - accept all cookies
// 1 - deprecated. don't use it.
// 2 - accept as session cookies
// 3 - deprecated. don't use it.
VARCACHE_PREF(
  Live,
  "network.cookie.lifetimePolicy",
  network_cookie_lifetimePolicy,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "network.cookie.thirdparty.sessionOnly",
   network_cookie_thirdparty_sessionOnly,
  bool, false
)

VARCACHE_PREF(
  Live,
  "network.cookie.thirdparty.nonsecureSessionOnly",
   network_cookie_thirdparty_nonsecureSessionOnly,
  bool, false
)

// Enables the predictive service.
VARCACHE_PREF(
  Live,
  "network.predictor.enabled",
  network_predictor_enabled,
  bool, true
)

// Allow CookieSettings to be unblocked for channels without a document.
// This is for testing only.
VARCACHE_PREF(
  Live,
  "network.cookieSettings.unblocked_for_testing",
  network_cookieSettings_unblocked_for_testing,
  bool, false
)

VARCACHE_PREF(
  Live,
  "network.predictor.enable-hover-on-ssl",
  network_predictor_enable_hover_on_ssl,
  bool, false
)

VARCACHE_PREF(
  Live,
  "network.predictor.enable-prefetch",
  network_predictor_enable_prefetch,
  bool, false
)

VARCACHE_PREF(
  Live,
  "network.predictor.page-degradation.day",
  network_predictor_page_degradation_day,
  int32_t, 0
)
VARCACHE_PREF(
  Live,
  "network.predictor.page-degradation.week",
  network_predictor_page_degradation_week,
  int32_t, 5
)
VARCACHE_PREF(
  Live,
  "network.predictor.page-degradation.month",
  network_predictor_page_degradation_month,
  int32_t, 10
)
VARCACHE_PREF(
  Live,
  "network.predictor.page-degradation.year",
  network_predictor_page_degradation_year,
  int32_t, 25
)
VARCACHE_PREF(
  Live,
  "network.predictor.page-degradation.max",
  network_predictor_page_degradation_max,
  int32_t, 50
)

VARCACHE_PREF(
  Live,
  "network.predictor.subresource-degradation.day",
  network_predictor_subresource_degradation_day,
  int32_t, 1
)
VARCACHE_PREF(
  Live,
  "network.predictor.subresource-degradation.week",
  network_predictor_subresource_degradation_week,
  int32_t, 10
)
VARCACHE_PREF(
  Live,
  "network.predictor.subresource-degradation.month",
  network_predictor_subresource_degradation_month,
  int32_t, 25
)
VARCACHE_PREF(
  Live,
  "network.predictor.subresource-degradation.year",
  network_predictor_subresource_degradation_year,
  int32_t, 50
)
VARCACHE_PREF(
  Live,
  "network.predictor.subresource-degradation.max",
  network_predictor_subresource_degradation_max,
  int32_t, 100
)

VARCACHE_PREF(
  Live,
  "network.predictor.prefetch-rolling-load-count",
  network_predictor_prefetch_rolling_load_count,
  int32_t, 10
)

VARCACHE_PREF(
  Live,
  "network.predictor.prefetch-min-confidence",
  network_predictor_prefetch_min_confidence,
  int32_t, 100
)
VARCACHE_PREF(
  Live,
  "network.predictor.preconnect-min-confidence",
  network_predictor_preconnect_min_confidence,
  int32_t, 90
)
VARCACHE_PREF(
  Live,
  "network.predictor.preresolve-min-confidence",
  network_predictor_preresolve_min_confidence,
  int32_t, 60
)

VARCACHE_PREF(
  Live,
  "network.predictor.prefetch-force-valid-for",
  network_predictor_prefetch_force_valid_for,
  int32_t, 10
)

VARCACHE_PREF(
  Live,
  "network.predictor.max-resources-per-entry",
  network_predictor_max_resources_per_entry,
  int32_t, 100
)

// This is selected in concert with max-resources-per-entry to keep memory
// usage low-ish. The default of the combo of the two is ~50k.
VARCACHE_PREF(
  Live,
  "network.predictor.max-uri-length",
  network_predictor_max_uri_length,
  uint32_t, 500
)

PREF("network.predictor.cleaned-up", bool, false)

// A testing flag.
VARCACHE_PREF(
  Live,
  "network.predictor.doing-tests",
  network_predictor_doing_tests,
  bool, false
)

// Telemetry of traffic categories
VARCACHE_PREF(
  Live,
  "network.traffic_analyzer.enabled",
  network_traffic_analyzer_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "network.delay.tracking.load",
  network_delay_tracking_load,
  uint32_t, 0
)

// Max time to shutdown the resolver threads
VARCACHE_PREF(
  Live,
  "network.dns.resolver_shutdown_timeout_ms",
  network_dns_resolver_shutdown_timeout_ms,
  uint32_t, 2000
)

// Some requests during a page load are marked as "tail", mainly trackers, but not only.
// This pref controls whether such requests are put to the tail, behind other requests
// emerging during page loading process.
VARCACHE_PREF(
  Live,
  "network.http.tailing.enabled",
  network_http_tailing_enabled,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "nglayout."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "nglayout.debug.widget_update_flashing",
  WidgetUpdateFlashing,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "plugins."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "plugins.flashBlock.enabled",
  plugins_flashBlock_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "plugins.http_https_only",
  plugins_http_https_only,
  bool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "preferences."
//---------------------------------------------------------------------------

PREF("preferences.allow.omt-write", bool, true)

//---------------------------------------------------------------------------
// Prefs starting with "print."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "print.font-variations-as-paths",
  PrintFontVariationsAsPaths,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "privacy."
//---------------------------------------------------------------------------

// Annotate trackers using the strict list. If set to false, the basic list will
// be used instead.
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "privacy.annotate_channels.strict_list.enabled",
  privacy_annotate_channels_strict_list_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Annotate channels based on the tracking protection list in all modes
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.annotate_channels",
  privacy_trackingprotection_annotate_channels,
  bool, true
)

// Block 3rd party fingerprinting resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.fingerprinting.enabled",
  privacy_trackingprotection_fingerprinting_enabled,
  bool, false
)

// Annotate fingerprinting resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.fingerprinting.annotate.enabled",
  privacy_trackingprotection_fingerprinting_annotate_enabled,
  bool, true
)

// Block 3rd party cryptomining resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.cryptomining.enabled",
  privacy_trackingprotection_cryptomining_enabled,
  bool, false
)

// Annotate cryptomining resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.cryptomining.annotate.enabled",
  privacy_trackingprotection_cryptomining_annotate_enabled,
  bool, true
)

// Whether origin telemetry should be enabled
// NOTE: if telemetry.origin_telemetry_test_mode.enabled is enabled, this pref
//       won't have any effect.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.origin_telemetry.enabled",
  privacy_trackingprotection_origin_telemetry_enabled,
  RelaxedAtomicBool, false
)

// Spoof user locale to English
VARCACHE_PREF(
  Live,
  "privacy.spoof_english",
  privacy_spoof_english,
  RelaxedAtomicUint32, 0
)

// send "do not track" HTTP header, disabled by default
VARCACHE_PREF(
  Live,
  "privacy.donottrackheader.enabled",
  privacy_donottrackheader_enabled,
  bool, false
)

// Lower the priority of network loads for resources on the tracking protection
// list.  Note that this requires the
// privacy.trackingprotection.annotate_channels pref to be on in order to have
// any effect.
#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.lower_network_priority",
  privacy_trackingprotection_lower_network_priority,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "privacy.resistFingerprinting",
  ResistFingerprinting,
  RelaxedAtomicBool, false
)

// Anti-tracking permission expiration
VARCACHE_PREF(
  Live,
  "privacy.restrict3rdpartystorage.expiration",
  privacy_restrict3rdpartystorage_expiration,
  uint32_t, 2592000 // 30 days (in seconds)
)

// Anti-tracking user-interaction expiration
VARCACHE_PREF(
  Live,
  "privacy.userInteraction.expiration",
  privacy_userInteraction_expiration,
  uint32_t, 2592000 // 30 days (in seconds)
)

// Anti-tracking user-interaction document interval
VARCACHE_PREF(
  Live,
  "privacy.userInteraction.document.interval",
  privacy_userInteraction_document_interval,
  uint32_t, 1800 // 30 minutes (in seconds)
)

// Maximum client-side cookie life-time cap
#ifdef NIGHTLY_BUILD
# define PREF_VALUE 604800 // 7 days
#else
# define PREF_VALUE 0
#endif
VARCACHE_PREF(
  Live,
  "privacy.documentCookies.maxage",
  privacy_documentCookies_maxage,
  uint32_t, PREF_VALUE // (in seconds, set to 0 to disable)
)
#undef PREF_VALUE

// Anti-fingerprinting, disabled by default
VARCACHE_PREF(
  Live,
  "privacy.resistFingerprinting",
  privacy_resistFingerprinting,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts",
  privacy_resistFingerprinting_autoDeclineNoUserInputCanvasPrompts,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "privacy.storagePrincipal.enabledForTrackers",
  privacy_storagePrincipal_enabledForTrackers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "privacy.window.maxInnerWidth",
  privacy_window_maxInnerWidth,
  int32_t, 1000
)

VARCACHE_PREF(
  Live,
  "privacy.window.maxInnerHeight",
  privacy_window_maxInnerHeight,
  int32_t, 1000
)

//---------------------------------------------------------------------------
// Prefs starting with "security."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "security.csp.enable",
  security_csp_enable,
  bool, true
)

VARCACHE_PREF(
  Live,
  "security.csp.enableStrictDynamic",
  security_csp_enableStrictDynamic,
  bool, true
)

VARCACHE_PREF(
  Live,
  "security.csp.reporting.script-sample.max-length",
  security_csp_reporting_script_sample_max_length,
  int32_t, 40
)

// Whether strict file origin policy is in effect.
VARCACHE_PREF(
  Live,
  "security.fileuri.strict_origin_policy",
  security_fileuri_strict_origin_policy,
  RelaxedAtomicBool, true
)

// Hardware Origin-bound Second Factor Support
VARCACHE_PREF(
  Live,
  "security.webauth.webauthn",
  security_webauth_webauthn,
  bool, true
)

// No way to enable on Android, Bug 1552602
#if defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "security.webauth.u2f",
  security_webauth_u2f,
  bool, PREF_VALUE
)
#undef PREF_VALUE

//---------------------------------------------------------------------------
// Prefs starting with "slider."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Once,
  "slider.snapMultiplier",
  SliderSnapMultiplier,
  int32_t, 0
)

//---------------------------------------------------------------------------
// Prefs starting with "telemetry."
//---------------------------------------------------------------------------

// Enable origin telemetry test mode or not
// NOTE: turning this on will override the
//       privacy.trackingprotection.origin_telemetry.enabled pref.
VARCACHE_PREF(
  Live,
  "telemetry.origin_telemetry_test_mode.enabled",
  telemetry_origin_telemetry_test_mode_enabled,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "test."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "test.events.async.enabled",
  TestEventsAsyncEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "test.mousescroll",
  MouseScrollTestingEnabled,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "thread."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "threads.medium_high_event_queue.enabled",
  threads_medium_high_event_queue_enabled,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "toolkit."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "toolkit.scrollbox.horizontalScrollDistance",
  ToolkitHorizontalScrollDistance,
  RelaxedAtomicInt32, 5
)

VARCACHE_PREF(
  Live,
  "toolkit.scrollbox.verticalScrollDistance",
  ToolkitVerticalScrollDistance,
  RelaxedAtomicInt32, 3
)

//---------------------------------------------------------------------------
// Prefs starting with "ui."
//---------------------------------------------------------------------------

// Prevent system colors from being exposed to CSS or canvas.
VARCACHE_PREF(
  Live,
  "ui.use_standins_for_native_colors",
  ui_use_standins_for_native_colors,
  RelaxedAtomicBool, false
)

// Disable page loading activity cursor by default.
VARCACHE_PREF(
  Live,
  "ui.use_activity_cursor",
  ui_use_activity_cursor,
  bool, false
)

VARCACHE_PREF(
  Live,
  "ui.click_hold_context_menus.delay",
  UiClickHoldContextMenusDelay,
  RelaxedAtomicInt32, 500
)

//---------------------------------------------------------------------------
// Prefs starting with "view_source."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "view_source.editor.external",
  view_source_editor_external,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "webgl." (for pref access from Worker threads)
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "webgl.1.allow-core-profiles",
  WebGL1AllowCoreProfile,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.all-angle-options",
  WebGLAllANGLEOptions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.force-d3d11",
  WebGLANGLEForceD3D11,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.try-d3d11",
  WebGLANGLETryD3D11,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.force-warp",
  WebGLANGLEForceWARP,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.can-lose-context-in-foreground",
  WebGLCanLoseContextInForeground,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "webgl.default-low-power",
  WebGLDefaultLowPower,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.default-no-alpha",
  WebGLDefaultNoAlpha,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-angle",
  WebGLDisableANGLE,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-wgl",
  WebGLDisableWGL,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-extensions",
  WebGLDisableExtensions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.dxgl.enabled",
  WebGLDXGLEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.dxgl.needs-finish",
  WebGLDXGLNeedsFinish,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.disable-fail-if-major-performance-caveat",
  WebGLDisableFailIfMajorPerformanceCaveat,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-DOM-blit-uploads",
  WebGLDisableDOMBlitUploads,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disabled",
  WebGLDisabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-draft-extensions",
  WebGLDraftExtensionsEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-privileged-extensions",
  WebGLPrivilegedExtensionsEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-surface-texture",
  WebGLSurfaceTextureEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-webgl2",
  webgl_enable_webgl2,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "webgl.force-enabled",
  WebGLForceEnabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "webgl.force-layers-readback",
  WebGLForceLayersReadback,
  bool, false
)

VARCACHE_PREF(
  Live,
  "webgl.force-index-validation",
  WebGLForceIndexValidation,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.lose-context-on-memory-pressure",
  WebGLLoseContextOnMemoryPressure,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.max-contexts",
  WebGLMaxContexts,
  RelaxedAtomicUint32, 32
)

VARCACHE_PREF(
  Live,
  "webgl.max-contexts-per-principal",
  WebGLMaxContextsPerPrincipal,
  RelaxedAtomicUint32, 16
)

VARCACHE_PREF(
  Live,
  "webgl.max-warnings-per-context",
  WebGLMaxWarningsPerContext,
  RelaxedAtomicUint32, 32
)

VARCACHE_PREF(
  Live,
  "webgl.min_capability_mode",
  WebGLMinCapabilityMode,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.msaa-force",
  WebGLForceMSAA,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.msaa-samples",
  WebGLMsaaSamples,
  RelaxedAtomicUint32, 4
)

VARCACHE_PREF(
  Live,
  "webgl.prefer-16bpp",
  WebGLPrefer16bpp,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.allow-immediate-queries",
  WebGLImmediateQueries,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.allow-fb-invalidation",
  WebGLFBInvalidation,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.perf.max-warnings",
  WebGLMaxPerfWarnings,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.perf.max-acceptable-fb-status-invals",
  WebGLMaxAcceptableFBStatusInvals,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.perf.spew-frame-allocs",
  WebGLSpewFrameAllocs,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "widget."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "widget.window-transforms.disabled",
  WindowTransformsDisabled,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "xul."
//---------------------------------------------------------------------------

// Pref to control whether arrow-panel animations are enabled or not.
// Transitions are currently disabled on Linux due to rendering issues on
// certain configurations.
#ifdef MOZ_WIDGET_GTK
#define PREF_VALUE false
#else
#define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "xul.panel-animations.enabled",
  xul_panel_animations_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

//---------------------------------------------------------------------------
// End of prefs
//---------------------------------------------------------------------------

// clang-format on
