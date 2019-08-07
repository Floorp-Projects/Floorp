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
//      <pref-name-id>,  // indented one space to align with <pref-name-string>
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
//   * Once: Set the value once at startup, and then leave it unchanged after
//     that. (The exact point at which all Once pref values is set is when the
//     first Once getter is called.) This is useful for graphics prefs where we
//     often don't want a new pref value to apply until restart. Otherwise, this
//     update policy is best avoided because its behaviour can cause confusion
//     and bugs.
//
// - <pref-name-string> is the same as for normal prefs.
//
// - <pref-name-id> is the name of the static getter function generated within
//   the StaticPrefs class. For consistency, the identifier for every pref
//   should be created by starting with <pref-name-string> and converting any
//   '.' or '-' chars to '_'. For example, "foo.bar_baz" becomes
//   `foo_bar_baz`. This is arguably ugly, but clear, and you can search for
//   both using the regexp /foo.bar.baz/. Some getter functions have
//   `_do_not_use_directly` appended to indicate that there is some other
//   wrapper getter that should be used in preference to the normal static pref
//   getter.
//
// - <cpp-type> is one of bool, int32_t, uint32_t, float, or an Atomic version
//   of one of those. The C++ preprocessor doesn't like template syntax in a
//   macro argument, so use the typedefs defines in StaticPrefs.h; for example,
//   use `ReleaseAcquireAtomicBool` instead of `Atomic<bool, ReleaseAcquire>`.
//   A pref with a Skip or Once policy can be non-atomic as it is only ever
//   written to once during the parent process startup. A pref with a Live
//   policy must be made Atomic if ever accessed outside the main thread;
//   assertions are in place to ensure this.
//
// - <default-value> is the same as for normal prefs.
//
// Note that Rust code must access the global variable directly, rather than via
// the getter.

// clang-format off

#ifdef RELEASE_OR_BETA
# define NOT_IN_RELEASE_OR_BETA_VALUE false
#else
# define NOT_IN_RELEASE_OR_BETA_VALUE true
#endif

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
   accessibility_browsewithcaret,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "apz."
// The apz prefs are explained in AsyncPanZoomController.cpp
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "apz.allow_double_tap_zooming",
   apz_allow_double_tap_zooming,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.allow_immediate_handoff",
   apz_allow_immediate_handoff,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.allow_zooming",
   apz_allow_zooming,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.enabled",
   apz_android_chrome_fling_physics_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.friction",
   apz_android_chrome_fling_physics_friction,
  AtomicFloat, 0.015f
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.inflexion",
   apz_android_chrome_fling_physics_inflexion,
  AtomicFloat, 0.35f
)

VARCACHE_PREF(
  Live,
  "apz.android.chrome_fling_physics.stop_threshold",
   apz_android_chrome_fling_physics_stop_threshold,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "apz.autoscroll.enabled",
   apz_autoscroll_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.breakout_angle",
   apz_axis_lock_breakout_angle,
  AtomicFloat, float(M_PI / 8.0) /* 22.5 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.breakout_threshold",
   apz_axis_lock_breakout_threshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.direct_pan_angle",
   apz_axis_lock_direct_pan_angle,
  AtomicFloat, float(M_PI / 3.0) /* 60 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.lock_angle",
   apz_axis_lock_lock_angle,
  AtomicFloat, float(M_PI / 6.0) /* 30 degrees */
)

VARCACHE_PREF(
  Live,
  "apz.axis_lock.mode",
   apz_axis_lock_mode,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.content_response_timeout",
   apz_content_response_timeout,
  RelaxedAtomicInt32, 400
)

VARCACHE_PREF(
  Live,
  "apz.danger_zone_x",
   apz_danger_zone_x,
  RelaxedAtomicInt32, 50
)

VARCACHE_PREF(
  Live,
  "apz.danger_zone_y",
   apz_danger_zone_y,
  RelaxedAtomicInt32, 100
)

VARCACHE_PREF(
  Live,
  "apz.disable_for_scroll_linked_effects",
   apz_disable_for_scroll_linked_effects,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.displayport_expiry_ms",
   apz_displayport_expiry_ms,
  RelaxedAtomicUint32, 15000
)

VARCACHE_PREF(
  Live,
  "apz.drag.enabled",
   apz_drag_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.drag.initial.enabled",
   apz_drag_initial_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.drag.touch.enabled",
   apz_touch_drag_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.enlarge_displayport_when_clipped",
   apz_enlarge_displayport_when_clipped,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.enabled",
   apz_fixed_margin_override_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.bottom",
   apz_fixed_margin_override_bottom,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.fixed-margin-override.top",
   apz_fixed_margin_override_top,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_base_mult",
   apz_fling_accel_base_mult,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_interval_ms",
   apz_fling_accel_interval_ms,
  RelaxedAtomicInt32, 500
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_supplemental_mult",
   apz_fling_accel_supplemental_mult,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_accel_min_velocity",
   apz_fling_accel_min_velocity,
  AtomicFloat, 1.5f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_x1",
   apz_fling_curve_function_x1,
  float, 0.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_x2",
   apz_fling_curve_function_x2,
  float, 1.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_y1",
   apz_fling_curve_function_y1,
  float, 0.0f
)

VARCACHE_PREF(
  Once,
  "apz.fling_curve_function_y2",
   apz_fling_curve_function_y2,
  float, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_curve_threshold_inches_per_ms",
   apz_fling_curve_threshold_inches_per_ms,
  AtomicFloat, -1.0f
)

VARCACHE_PREF(
  Live,
  "apz.fling_friction",
   apz_fling_friction,
  AtomicFloat, 0.002f
)

VARCACHE_PREF(
  Live,
  "apz.fling_min_velocity_threshold",
   apz_fling_min_velocity_threshold,
  AtomicFloat, 0.5f
)

VARCACHE_PREF(
  Live,
  "apz.fling_stop_on_tap_threshold",
   apz_fling_stop_on_tap_threshold,
  AtomicFloat, 0.05f
)

VARCACHE_PREF(
  Live,
  "apz.fling_stopped_threshold",
   apz_fling_stopped_threshold,
  AtomicFloat, 0.01f
)

VARCACHE_PREF(
  Live,
  "apz.frame_delay.enabled",
   apz_frame_delay_enabled,
  RelaxedAtomicBool, false
)

#ifdef MOZ_WIDGET_GTK
VARCACHE_PREF(
  Live,
  "apz.gtk.kinetic_scroll.enabled",
   apz_gtk_kinetic_scroll_enabled,
  RelaxedAtomicBool, false
)
#endif

#if !defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "apz.keyboard.enabled",
   apz_keyboard_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "apz.keyboard.passive-listeners",
   apz_keyboard_passive_listeners,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.max_tap_time",
   apz_max_tap_time,
  RelaxedAtomicInt32, 300
)

VARCACHE_PREF(
  Live,
  "apz.max_velocity_inches_per_ms",
   apz_max_velocity_inches_per_ms,
  AtomicFloat, -1.0f
)

VARCACHE_PREF(
  Once,
  "apz.max_velocity_queue_size",
   apz_max_velocity_queue_size,
  uint32_t, 5
)

VARCACHE_PREF(
  Live,
  "apz.min_skate_speed",
   apz_min_skate_speed,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.minimap.enabled",
   apz_minimap_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.one_touch_pinch.enabled",
   apz_one_touch_pinch_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.enabled",
   apz_overscroll_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.min_pan_distance_ratio",
   apz_overscroll_min_pan_distance_ratio,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.spring_stiffness",
   apz_overscroll_spring_stiffness,
  AtomicFloat, 0.001f
)

VARCACHE_PREF(
  Live,
  "apz.overscroll.stop_distance_threshold",
   apz_overscroll_stop_distance_threshold,
  AtomicFloat, 5.0f
)

VARCACHE_PREF(
  Live,
  "apz.paint_skipping.enabled",
   apz_paint_skipping_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.peek_messages.enabled",
   apz_peek_messages_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.mode",
   apz_pinch_lock_mode,
  RelaxedAtomicInt32, 1
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.scroll_lock_threshold",
   apz_pinch_lock_scroll_lock_threshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.span_breakout_threshold",
   apz_pinch_lock_span_breakout_threshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Live,
  "apz.pinch_lock.span_lock_threshold",
   apz_pinch_lock_span_lock_threshold,
  AtomicFloat, 1.0f / 32.0f
)

VARCACHE_PREF(
  Once,
  "apz.pinch_lock.buffer_max_age",
   apz_pinch_lock_buffer_max_age,
  int32_t, 50 // milliseconds
)

VARCACHE_PREF(
  Live,
  "apz.popups.enabled",
   apz_popups_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.printtree",
   apz_printtree,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.record_checkerboarding",
   apz_record_checkerboarding,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "apz.second_tap_tolerance",
   apz_second_tap_tolerance,
  AtomicFloat, 0.5f
)

VARCACHE_PREF(
  Live,
  "apz.test.fails_with_native_injection",
   apz_test_fails_with_native_injection,
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
   apz_touch_move_tolerance,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "apz.touch_start_tolerance",
   apz_touch_start_tolerance,
  AtomicFloat, 1.0f/4.5f
)

VARCACHE_PREF(
  Live,
  "apz.velocity_bias",
   apz_velocity_bias,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.velocity_relevance_time_ms",
   apz_velocity_relevance_time_ms,
  RelaxedAtomicUint32, 150
)

VARCACHE_PREF(
  Live,
  "apz.x_skate_highmem_adjust",
   apz_x_skate_highmem_adjust,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.x_skate_size_multiplier",
   apz_x_skate_size_multiplier,
  AtomicFloat, 1.5f
)

VARCACHE_PREF(
  Live,
  "apz.x_stationary_size_multiplier",
   apz_x_stationary_size_multiplier,
  AtomicFloat, 3.0f
)

VARCACHE_PREF(
  Live,
  "apz.y_skate_highmem_adjust",
   apz_y_skate_highmem_adjust,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "apz.y_skate_size_multiplier",
   apz_y_skate_size_multiplier,
  AtomicFloat, 2.5f
)

VARCACHE_PREF(
  Live,
  "apz.y_stationary_size_multiplier",
   apz_y_stationary_size_multiplier,
  AtomicFloat, 3.5f
)

VARCACHE_PREF(
  Live,
  "apz.zoom_animation_duration_ms",
   apz_zoom_animation_duration_ms,
  RelaxedAtomicInt32, 250
)

VARCACHE_PREF(
  Live,
  "apz.scale_repaint_delay_ms",
   apz_scale_repaint_delay_ms,
  RelaxedAtomicInt32, 500
)

VARCACHE_PREF(
  Live,
  "apz.relative-update.enabled",
   apz_relative_update_enabled,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "beacon."
//---------------------------------------------------------------------------

// Is support for Navigator.sendBeacon enabled?
VARCACHE_PREF(
  Live,
  "beacon.enabled",
  beacon_enabled,
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

// Whether Content Blocking allow list is respected for ETP interventions.
VARCACHE_PREF(
  Live,
  "browser.contentblocking.allowlist.storage.enabled",
  browser_contentblocking_allowlist_storage_enabled,
  bool, true
)

// Whether Content Blocking allow list is respected for tracking annotations.
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
  bool, false
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
  bool, true
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

// When this pref is enabled top level loads with a mismatched
// Cross-Origin-Opener-Policy header will be loaded in a separate process.
VARCACHE_PREF(
  Live,
  "browser.tabs.remote.useCrossOriginOpenerPolicy",
  browser_tabs_remote_useCrossOriginOpenerPolicy,
  bool, false
)

// When this pref is enabled, the browser will check no-cors responses that
// have the Cross-Origin-Resource-Policy header and will fail the request if
// the origin of the resource's loading document doesn't match the origin
// (or base domain) of the loaded resource.
VARCACHE_PREF(
  Live,
  "browser.tabs.remote.useCORP",
  browser_tabs_remote_useCORP,
  bool, false
)

VARCACHE_PREF(
  Live,
  "browser.ui.scroll-toolbar-threshold",
   browser_ui_scroll_toolbar_threshold,
  RelaxedAtomicInt32, 10
)

VARCACHE_PREF(
  Live,
  "browser.ui.zoom.force-user-scalable",
   browser_ui_zoom_force_user_scalable,
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
   browser_viewport_desktopWidth,
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

// Is support for CanvasRenderingContext2D.filter enabled?
VARCACHE_PREF(
  Live,
  "canvas.filters.enabled",
  canvas_filters_enabled,
  bool, true
)

// Provide ability to turn on support for canvas focus rings.
VARCACHE_PREF(
  Live,
  "canvas.focusring.enabled",
  canvas_focusring_enabled,
  bool, true
)

// Is support for CanvasRenderingContext2D's hitRegion APIs enabled?
VARCACHE_PREF(
  Live,
  "canvas.hitregions.enabled",
  canvas_hitregions_enabled,
  bool, false
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

// Is support for the device sensors API enabled?
VARCACHE_PREF(
  Live,
  "device.sensors.enabled",
  device_sensors_enabled,
  bool, true
)

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

// Is support for Navigator.getBattery enabled?
VARCACHE_PREF(
  Live,
  "dom.battery.enabled",
  dom_battery_enabled,
  bool, true
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

VARCACHE_PREF(
  Live,
  "dom.element.transform-getters.enabled",
  dom_element_transform_getters_enabled,
  bool, false
)

// Is support for Performance.mozMemory enabled?
VARCACHE_PREF(
  Live,
  "dom.enable_memory_stats",
  dom_enable_memory_stats,
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
   dom_ipc_plugins_asyncdrawing_enabled,
  RelaxedAtomicBool, false
)

// Is support for input type=date and type=time enabled?
VARCACHE_PREF(
  Live,
  "dom.forms.datetime",
  dom_forms_datetime,
  bool, true
)

// Is support for HTMLInputElement.inputMode enabled?
VARCACHE_PREF(
  Live,
  "dom.forms.inputmode",
  dom_forms_inputmode,
  bool, NOT_IN_RELEASE_OR_BETA_VALUE
)

// Enable Directory API. By default, disabled.
VARCACHE_PREF(
  Live,
  "dom.input.dirpicker",
  dom_input_dirpicker,
  bool, false
)

// Is support for InputEvent.data enabled?
VARCACHE_PREF(
  Live,
  "dom.inputevent.data.enabled",
  dom_inputevent_data_enabled,
  bool, true
)

// Is support for InputEvent.dataTransfer enabled?
VARCACHE_PREF(
  Live,
  "dom.inputevent.datatransfer.enabled",
  dom_inputevent_datatransfer_enabled,
  bool, true
)

// Is support for InputEvent.inputType enabled?
VARCACHE_PREF(
  Live,
  "dom.inputevent.inputtype.enabled",
  dom_inputevent_inputtype_enabled,
  bool, true
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

// Whether window.onappinstalled from "W3C Web Manifest" is enabled
VARCACHE_PREF(
  Live,
  "dom.manifest.onappinstalled",
  dom_manifest_onappinstalled,
  bool, false
)

// This pref is used to enable/disable the `document.autoplayPolicy` API which
// returns a enum string which presents current autoplay policy and can change
// overtime based on user session activity.
VARCACHE_PREF(
  Live,
  "dom.media.autoplay.autoplay-policy-api",
  dom_media_autoplay_autoplay_policy_api,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.meta-viewport.enabled",
   dom_meta_viewport_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.metaElement.setCookie.allowed",
  dom_metaElement_setCookie_allowed,
  bool, false
)

// Is support for module scripts (<script type="module">) enabled for content?
VARCACHE_PREF(
  Live,
  "dom.moduleScripts.enabled",
  dom_moduleScripts_enabled,
  bool, true
)

// Is support for mozBrowser frames enabled?
VARCACHE_PREF(
  Live,
  "dom.mozBrowserFramesEnabled",
  dom_mozBrowserFramesEnabled,
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

// Is support for Window.paintWorklet enabled?
VARCACHE_PREF(
  Live,
  "dom.paintWorklet.enabled",
  dom_paintWorklet_enabled,
  bool, false
)

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

// Is support for PerformanceTiming.timeToContentfulPaint enabled?
VARCACHE_PREF(
  Live,
  "dom.performance.time_to_contentful_paint.enabled",
  dom_performance_time_to_contentful_paint_enabled,
  bool, false
)

// Is support for PerformanceTiming.timeToDOMContentFlushed enabled?
VARCACHE_PREF(
  Live,
  "dom.performance.time_to_dom_content_flushed.enabled",
  dom_performance_time_to_dom_content_flushed_enabled,
  bool, false
)

// Is support for PerformanceTiming.timeToFirstInteractive enabled?
VARCACHE_PREF(
  Live,
  "dom.performance.time_to_first_interactive.enabled",
  dom_performance_time_to_first_interactive_enabled,
  bool, false
)

// Is support for PerformanceTiming.timeToNonBlankPaint enabled?
VARCACHE_PREF(
  Live,
  "dom.performance.time_to_non_blank_paint.enabled",
  dom_performance_time_to_non_blank_paint_enabled,
  bool, false
)

// Is support for Permissions.revoke enabled?
VARCACHE_PREF(
  Live,
  "dom.permissions.revoke.enable",
  dom_permissions_revoke_enable,
  bool, false
)

// Whether we should show the placeholder when the element is focused but empty.
VARCACHE_PREF(
  Live,
  "dom.placeholder.show_on_focus",
  dom_placeholder_show_on_focus,
  bool, true
)

// Is support for Element.requestPointerLock enabled?
// This is added for accessibility purpose. When user has no way to exit
// pointer lock (e.g. no keyboard available), they can use this pref to
// disable the Pointer Lock API altogether.
VARCACHE_PREF(
  Live,
  "dom.pointer-lock.enabled",
  dom_pointer_lock_enabled,
  bool, true
)

// Presentation API
#if defined(ANDROID)
# define PREF_VALUE NOT_IN_RELEASE_OR_BETA_VALUE
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.presentation.enabled",
  dom_presentation_enabled,
  bool, PREF_VALUE
)

VARCACHE_PREF(
  Live,
  "dom.presentation.controller.enabled",
  dom_presentation_controller_enabled,
  bool, PREF_VALUE
)

VARCACHE_PREF(
  Live,
  "dom.presentation.receiver.enabled",
  dom_presentation_receiver_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.presentation.testing.simulate-receiver",
  dom_presentation_testing_simulate_receiver,
  bool, false
)

// WHATWG promise rejection events. See
// https://html.spec.whatwg.org/multipage/webappapis.html#promiserejectionevent
VARCACHE_PREF(
  Live,
  "dom.promise_rejection_events.enabled",
  dom_promise_rejection_events_enabled,
  RelaxedAtomicBool, true
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

// Is support for Navigator.registerContentHandler enabled?
VARCACHE_PREF(
  Live,
  "dom.registerContentHandler.enabled",
  dom_registerContentHandler_enabled,
  bool, false
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

// Is support for decoding external (non-inline) classic or module DOM scripts
// (i.e. anything but workers) as UTF-8, then directly compiling without
// inflating to UTF-16, enabled?
VARCACHE_PREF(
  Live,
  "dom.script_loader.external_scripts.utf8_parsing.enabled",
  dom_script_loader_external_scripts_utf8_parsing_enabled,
  bool, true
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

// Is support for selection event APIs enabled?
VARCACHE_PREF(
  Live,
  "dom.select_events.enabled",
  dom_select_events_enabled,
  bool, true
)

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

// External.AddSearchProvider is deprecated and it will be removed in the next
// cycles.
VARCACHE_PREF(
  Live,
  "dom.sidebar.enabled",
  dom_sidebar_enabled,
  bool, true
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

// Is support for Storage test APIs enabled?
VARCACHE_PREF(
  Live,
  "dom.storage.testing",
  dom_storage_testing,
  bool, false
)

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

// Is support for Selection.GetRangesForInterval enabled?
VARCACHE_PREF(
  Live,
  "dom.testing.selection.GetRangesForInterval",
  dom_testing_selection_GetRangesForInterval,
  bool, false
)

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

// Is support for Window.visualViewport enabled?
VARCACHE_PREF(
  Live,
  "dom.visualviewport.enabled",
  dom_visualviewport_enabled,
  bool, false
)

// Is support for WebVR APIs enabled?
// Enabled by default in beta and release for Windows and OS X and for all
// platforms in nightly and aurora.
#if defined(XP_WIN) || defined(XP_DARWIN) || !defined(RELEASE_OR_BETA)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.vr.enabled",
  dom_vr_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.vr.autoactivate.enabled",
   dom_vr_autoactivate_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.controller.enumerate.interval",
   dom_vr_controller_enumerate_interval,
  RelaxedAtomicInt32, 1000
)

VARCACHE_PREF(
  Live,
  "dom.vr.controller_trigger_threshold",
   dom_vr_controller_trigger_threshold,
  AtomicFloat, 0.1f
)

VARCACHE_PREF(
  Live,
  "dom.vr.display.enumerate.interval",
   dom_vr_display_enumerate_interval,
  RelaxedAtomicInt32, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.display.rafMaxDuration",
   dom_vr_display_rafMaxDuration,
  RelaxedAtomicUint32, 50
)

VARCACHE_PREF(
  Live,
  "dom.vr.external.notdetected.timeout",
   dom_vr_external_notdetected_timeout,
  RelaxedAtomicInt32, 60000
)

VARCACHE_PREF(
  Live,
  "dom.vr.external.quit.timeout",
   dom_vr_external_quit_timeout,
  RelaxedAtomicInt32, 10000
)

VARCACHE_PREF(
  Live,
  "dom.vr.inactive.timeout",
   dom_vr_inactive_timeout,
  RelaxedAtomicInt32, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.navigation.timeout",
   dom_vr_navigation_timeout,
  RelaxedAtomicInt32, 1000
)

// Oculus device
#if defined(HAVE_64BIT_BUILD) && !defined(ANDROID)
// We are only enabling WebVR by default on 64-bit builds (Bug 1384459)
#define PREF_VALUE true
#else
// On Android, this pref is irrelevant.
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "dom.vr.oculus.enabled",
   dom_vr_oculus_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "dom.vr.oculus.invisible.enabled",
   dom_vr_oculus_invisible_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "dom.vr.oculus.present.timeout",
   dom_vr_oculus_present_timeout,
  RelaxedAtomicInt32, 500
)

// OpenVR device
#if !defined(HAVE_64BIT_BUILD) && !defined(ANDROID)
// We are only enabling WebVR by default on 64-bit builds (Bug 1384459).
#define PREF_VALUE false
#elif defined(XP_WIN) || defined(XP_MACOSX)
// We enable OpenVR by default for Windows and macOS.
#define PREF_VALUE true
#else
// See Bug 1310663 (Linux).  On Android, this pref is irrelevant.
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "dom.vr.openvr.enabled",
   dom_vr_openvr_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Once,
  "dom.vr.openvr.action_input",
   dom_vr_openvr_action_input,
  bool, true
)

// OSVR device
VARCACHE_PREF(
  Once,
  "dom.vr.osvr.enabled",
   dom_vr_osvr_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.poseprediction.enabled",
   dom_vr_poseprediction_enabled,
  RelaxedAtomicBool, true
)

// Enable a separate process for VR module.
#if defined(XP_WIN)
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "dom.vr.process.enabled",
   dom_vr_process_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Once,
  "dom.vr.process.startup_timeout_ms",
   dom_vr_process_startup_timeout_ms,
  int32_t, 5000
)

VARCACHE_PREF(
  Live,
  "dom.vr.puppet.enabled",
   dom_vr_puppet_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "dom.vr.require-gesture",
   dom_vr_require_gesture,
  RelaxedAtomicBool, true
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

// Is support for Navigator.webdriver enabled?
VARCACHE_PREF(
  Live,
  "dom.webdriver.enabled",
  dom_webdriver_enabled,
  bool, true
)

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

// Is support for HTMLInputElement.webkitEntries enabled?
#if !defined(MOZ_WIDGET_ANDROID)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "dom.webkitBlink.filesystem.enabled",
  dom_webkitBlink_filesystem_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

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

// Is support for Window.event enabled?
VARCACHE_PREF(
  Live,
  "dom.window.event.enabled",
  dom_window_event_enabled,
  bool, true
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
VARCACHE_PREF(
  Live,
  "dom.worker.script_loader.utf8_parsing.enabled",
  dom_worker_script_loader_utf8_parsing_enabled,
  RelaxedAtomicBool, true
)

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

// Is support for XMLDocument.async enabled?
VARCACHE_PREF(
  Live,
  "dom.xmldocument.async.enabled",
  dom_xmldocument_async_enabled,
  bool, false
)

// Is support for XMLDocument.load enabled?
VARCACHE_PREF(
  Live,
  "dom.xmldocument.load.enabled",
  dom_xmldocument_load_enabled,
  bool, false
)

// When this pref is set, parent documents may consider child iframes have
// loaded while they are still loading.
VARCACHE_PREF(
  Live,
  "dom.cross_origin_iframes_loaded_in_background",
   dom_cross_origin_iframes_loaded_in_background,
  bool, false
)

// Is support for Navigator.geolocation enabled?
VARCACHE_PREF(
  Live,
  "geo.enabled",
  geo_enabled,
  bool, true
)

// WebIDL test prefs

VARCACHE_PREF(
  Live,
  "abc.def",
  abc_def,
  bool, true
)
VARCACHE_PREF(
  Live,
  "ghi.jkl",
  ghi_jkl,
  bool, true
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

// This pref governs whether we run webextensions in a separate process (true)
// or the parent/main process (false)
VARCACHE_PREF(
  Live,
  "extensions.webextensions.remote",
  extensions_webextensions_remote,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "fission."
//---------------------------------------------------------------------------

// This pref has no effect within fission windows, it only controls the
// behaviour within non-fission windows.
VARCACHE_PREF(
  Live,
  "fission.preserve_browsing_contexts",
  fission_preserve_browsing_contexts,
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

VARCACHE_PREF(
  Live,
  "full-screen-api.mouse-event-allow-left-button-only",
   full_screen_api_mouse_event_allow_left_button_only,
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
   general_smoothScroll,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.currentVelocityWeighting",
   general_smoothScroll_currentVelocityWeighting,
  AtomicFloat, 0.25
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.durationToIntervalRatio",
   general_smoothScroll_durationToIntervalRatio,
  RelaxedAtomicInt32, 200
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.lines.durationMaxMS",
   general_smoothScroll_lines_durationMaxMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.lines.durationMinMS",
   general_smoothScroll_lines_durationMinMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel",
   general_smoothScroll_mouseWheel,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel.durationMaxMS",
   general_smoothScroll_mouseWheel_durationMaxMS,
  RelaxedAtomicInt32, 400
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.mouseWheel.durationMinMS",
   general_smoothScroll_mouseWheel_durationMinMS,
  RelaxedAtomicInt32, 200
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.other.durationMaxMS",
   general_smoothScroll_other_durationMaxMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.other.durationMinMS",
   general_smoothScroll_other_durationMinMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages",
   general_smoothScroll_pages,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages.durationMaxMS",
   general_smoothScroll_pages_durationMaxMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pages.durationMinMS",
   general_smoothScroll_pages_durationMinMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pixels.durationMaxMS",
   general_smoothScroll_pixels_durationMaxMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.pixels.durationMinMS",
   general_smoothScroll_pixels_durationMinMS,
  RelaxedAtomicInt32, 150
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.stopDecelerationWeighting",
  general_smoothScroll_stopDecelerationWeighting,
  AtomicFloat, 0.4f
)


VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.enabled",
   general_smoothScroll_msdPhysics_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.continuousMotionMaxDeltaMS",
   general_smoothScroll_msdPhysics_continuousMotionMaxDeltaMS,
  RelaxedAtomicInt32, 120
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.motionBeginSpringConstant",
   general_smoothScroll_msdPhysics_motionBeginSpringConstant,
  RelaxedAtomicInt32, 1250
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownMinDeltaMS",
   general_smoothScroll_msdPhysics_slowdownMinDeltaMS,
  RelaxedAtomicInt32, 12
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownMinDeltaRatio",
   general_smoothScroll_msdPhysics_slowdownMinDeltaRatio,
  AtomicFloat, 1.3f
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.slowdownSpringConstant",
   general_smoothScroll_msdPhysics_slowdownSpringConstant,
  RelaxedAtomicInt32, 2000
)

VARCACHE_PREF(
  Live,
  "general.smoothScroll.msdPhysics.regularSpringConstant",
   general_smoothScroll_msdPhysics_regularSpringConstant,
  RelaxedAtomicInt32, 1000
)

//---------------------------------------------------------------------------
// Prefs starting with "gfx."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Once,
  "gfx.allow-texture-direct-mapping",
   gfx_allow_texture_direct_mapping,
  bool, true
)

VARCACHE_PREF(
  Once,
  "gfx.android.rgb16.force",
   gfx_android_rgb16_force,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.apitrace.enabled",
   gfx_apitrace_enabled,
  bool, false
)

#if defined(RELEASE_OR_BETA)
// "Skip" means this is locked to the default value in beta and release.
VARCACHE_PREF(
  Skip,
  "gfx.blocklist.all",
   gfx_blocklist_all,
  int32_t, 0
)
#else
VARCACHE_PREF(
  Once,
  "gfx.blocklist.all",
   gfx_blocklist_all,
  int32_t, 0
)
#endif

// 0x7fff is the maximum supported xlib surface size and is more than enough for canvases.
VARCACHE_PREF(
  Live,
  "gfx.canvas.max-size",
   gfx_canvas_max_size,
  RelaxedAtomicInt32, 0x7fff
)

VARCACHE_PREF(
  Live,
  "gfx.canvas.remote",
   gfx_canvas_remote,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.color_management.enablev4",
   gfx_color_management_enablev4,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.color_management.mode",
   gfx_color_management_mode,
  RelaxedAtomicInt32, -1
)

// The zero default here should match QCMS_INTENT_DEFAULT from qcms.h
VARCACHE_PREF(
  Live,
  "gfx.color_management.rendering_intent",
   gfx_color_management_rendering_intent,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.clearstate",
   gfx_compositor_clearstate,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.glcontext.opaque",
   gfx_compositor_glcontext_opaque,
  RelaxedAtomicBool, false
)

#if defined(MOZ_WIDGET_ANDROID)
// Overrides the glClear color used when the surface origin is not (0, 0)
// Used for drawing a border around the content.
VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.r",
   gfx_compositor_override_clear_color_r,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.g",
   gfx_compositor_override_clear_color_g,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.b",
   gfx_compositor_override_clear_color_b,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "gfx.compositor.override.clear-color.a",
   gfx_compositor_override_clear_color_a,
  AtomicFloat, 0.0f
)
#endif // defined(MOZ_WIDGET_ANDROID)

VARCACHE_PREF(
  Live,
  "gfx.content.always-paint",
   gfx_content_always_paint,
  RelaxedAtomicBool, false
)

// Size in megabytes
VARCACHE_PREF(
  Once,
  "gfx.content.skia-font-cache-size",
   gfx_content_skia_font_cache_size,
  int32_t, 5
)

VARCACHE_PREF(
  Once,
  "gfx.device-reset.limit",
   gfx_device_reset_limit,
  int32_t, 10
)

VARCACHE_PREF(
  Once,
  "gfx.device-reset.threshold-ms",
   gfx_device_reset_threshold_ms,
  int32_t, -1
)


// Whether to disable the automatic detection and use of direct2d.
VARCACHE_PREF(
  Once,
  "gfx.direct2d.disabled",
   gfx_direct2d_disabled,
  bool, false
)

// Whether to attempt to enable Direct2D regardless of automatic detection or
// blacklisting.
VARCACHE_PREF(
  Once,
  "gfx.direct2d.force-enabled",
   gfx_direct2d_force_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.direct2d.destroy-dt-on-paintthread",
   gfx_direct2d_destroy_dt_on_paintthread,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.reuse-decoder-device",
   gfx_direct3d11_reuse_decoder_device,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.allow-keyed-mutex",
   gfx_direct3d11_allow_keyed_mutex,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.direct3d11.use-double-buffering",
   gfx_direct3d11_use_double_buffering,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.enable-debug-layer",
   gfx_direct3d11_enable_debug_layer,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.break-on-error",
   gfx_direct3d11_break_on_error,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.direct3d11.sleep-on-create-device",
   gfx_direct3d11_sleep_on_create_device,
  int32_t, 0
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.keep_color_bitmaps",
   gfx_downloadable_fonts_keep_color_bitmaps,
  RelaxedAtomicBool, false
)

// Whether font sanitization is performed on the main thread or not.
VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.sanitize_omt",
  gfx_downloadable_fonts_sanitize_omt,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.validate_variation_tables",
   gfx_downloadable_fonts_validate_variation_tables,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.downloadable_fonts.otl_validation",
   gfx_downloadable_fonts_otl_validation,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.draw-color-bars",
   gfx_draw_color_bars,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.e10s.hide-plugins-for-scroll",
   gfx_e10s_hide_plugins_for_scroll,
  bool, true
)

VARCACHE_PREF(
  Once,
  "gfx.e10s.font-list.shared",
   gfx_e10s_font_list_shared,
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
   gfx_font_rendering_coretext_enabled,
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
   gfx_layerscope_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.layerscope.port",
   gfx_layerscope_port,
  RelaxedAtomicInt32, 23456
)

// Note that        "gfx.logging.level" is defined in Logging.h.
VARCACHE_PREF(
  Live,
  "gfx.logging.level",
   gfx_logging_level,
  RelaxedAtomicInt32, mozilla::gfx::LOG_DEFAULT
)

VARCACHE_PREF(
  Once,
  "gfx.logging.crash.length",
   gfx_logging_crash_length,
  uint32_t, 16
)

VARCACHE_PREF(
  Live,
  "gfx.logging.painted-pixel-count.enabled",
   gfx_logging_painted_pixel_count_enabled,
  RelaxedAtomicBool, false
)

// The maximums here are quite conservative, we can tighten them if problems show up.
VARCACHE_PREF(
  Once,
  "gfx.logging.texture-usage.enabled",
   gfx_logging_texture_usage_enabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.logging.peak-texture-usage.enabled",
   gfx_logging_peak_texture_usage_enabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.logging.slow-frames.enabled",
   gfx_logging_slow_frames_enabled,
  bool, false
)

// Use gfxPlatform::MaxAllocSize instead of the pref directly.
VARCACHE_PREF(
  Once,
  "gfx.max-alloc-size",
   gfx_max_alloc_size_do_not_use_directly,
  int32_t, (int32_t)500000000
)

// Use gfxPlatform::MaxTextureSize instead of the pref directly.
VARCACHE_PREF(
  Once,
  "gfx.max-texture-size",
   gfx_max_texture_size_do_not_use_directly,
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
   gfx_partialpresent_force,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "gfx.perf-warnings.enabled",
   gfx_perf_warnings_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.testing.device-fail",
   gfx_testing_device_fail,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.testing.device-reset",
   gfx_testing_device_reset,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Once,
  "gfx.text.disable-aa",
   gfx_text_disable_aa,
  bool, false
)

// Disable surface sharing due to issues with compatible FBConfigs on
// NVIDIA drivers as described in bug 1193015.
VARCACHE_PREF(
  Live,
  "gfx.use-glx-texture-from-pixmap",
   gfx_use_glx_texture_from_pixmap,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-iosurface-textures",
   gfx_use_iosurface_textures,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-mutex-on-present",
   gfx_use_mutex_on_present,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.use-surfacetexture-textures",
   gfx_use_surfacetexture_textures,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.vsync.collect-scroll-transforms",
   gfx_vsync_collect_scroll_transforms,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.vsync.compositor.unobserve-count",
   gfx_vsync_compositor_unobserve_count,
  int32_t, 10
)

// We expose two prefs: gfx.webrender.all and gfx.webrender.enabled.
// The first enables WR+additional features, and the second just enables WR.
// For developer convenience, building with --enable-webrender=true or just
// --enable-webrender will set gfx.webrender.enabled to true by default.
//
// We also have a pref gfx.webrender.all.qualified which is not exposed via
// about:config. That pref enables WR but only on qualified hardware. This is
// the pref we'll eventually flip to deploy WebRender to the target population.
VARCACHE_PREF(
  Once,
  "gfx.webrender.all",
   gfx_webrender_all,
  bool, false
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.enabled",
   gfx_webrender_enabled_do_not_use_directly,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.blob-images",
   gfx_webrender_blob_images,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.blob.paint-flashing",
   gfx_webrender_blob_paint_flashing,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.dl.dump-parent",
   gfx_webrender_dl_dump_parent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.dl.dump-content",
   gfx_webrender_dl_dump_content,
  RelaxedAtomicBool, false
)

// Also expose a pref to allow users to force-disable WR. This is exposed
// on all channels because WR can be enabled on qualified hardware on all
// channels.
VARCACHE_PREF(
  Once,
  "gfx.webrender.force-disabled",
   gfx_webrender_force_disabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.highlight-painted-layers",
   gfx_webrender_highlight_painted_layers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.late-scenebuild-threshold",
   gfx_webrender_late_scenebuild_threshold,
  RelaxedAtomicInt32, 4
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.max-filter-ops-per-chain",
   gfx_webrender_max_filter_ops_per_chain,
  RelaxedAtomicUint32, 64
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.picture-caching",
   gfx_webrender_picture_caching,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "gfx.webrender.split-render-roots",
   gfx_webrender_split_render_roots,
  bool, false
)

VARCACHE_PREF(
  Live,
  "gfx.webrender.start-debug-server",
   gfx_webrender_start_debug_server,
  RelaxedAtomicBool, false
)

// Use vsync events generated by hardware
VARCACHE_PREF(
  Once,
  "gfx.work-around-driver-bugs",
   gfx_work_around_driver_bugs,
  bool, true
)

VARCACHE_PREF(
  Live,
  "gfx.ycbcr.accurate-conversion",
   gfx_ycbcr_accurate_conversion,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "gl." (OpenGL)
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "gl.allow-high-power",
   gl_allow_high_power,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "gl.ignore-dx-interop2-blacklist",
   gl_ignore_dx_interop2_blacklist,
  RelaxedAtomicBool, false
)

#if defined(XP_MACOSX)
VARCACHE_PREF(
  Live,
  "gl.multithreaded",
   gl_multithreaded,
  RelaxedAtomicBool, false
)
#endif

VARCACHE_PREF(
  Live,
  "gl.require-hardware",
   gl_require_hardware,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "gl.use-tls-is-current",
   gl_use_tls_is_current,
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
   image_animated_decode_on_demand_threshold_kb,
  RelaxedAtomicUint32, 20480
)

VARCACHE_PREF(
  Live,
  "image.animated.decode-on-demand.batch-size",
   image_animated_decode_on_demand_batch_size,
  RelaxedAtomicUint32, 6
)

// Whether we should recycle already displayed frames instead of discarding
// them. This saves on the allocation itself, and may be able to reuse the
// contents as well. Only applies if generating full frames.
VARCACHE_PREF(
  Once,
  "image.animated.decode-on-demand.recycle",
   image_animated_decode_on_demand_recycle,
  bool, true
)

VARCACHE_PREF(
  Live,
  "image.animated.resume-from-last-displayed",
   image_animated_resume_from_last_displayed,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.cache.factor2.threshold-surfaces",
   image_cache_factor2_threshold_surfaces,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "image.cache.max-rasterized-svg-threshold-kb",
   image_cache_max_rasterized_svg_threshold_kb,
  RelaxedAtomicInt32, 90*1024
)

// The maximum size, in bytes, of the decoded images we cache.
VARCACHE_PREF(
  Once,
  "image.cache.size",
   image_cache_size,
  int32_t, 5*1024*1024
)

// A weight, from 0-1000, to place on time when comparing to size.
// Size is given a weight of 1000 - timeweight.
VARCACHE_PREF(
  Once,
  "image.cache.timeweight",
   image_cache_timeweight,
  int32_t, 500
)

VARCACHE_PREF(
  Live,
  "image.decode-immediately.enabled",
   image_decode_immediately_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.downscale-during-decode.enabled",
   image_downscale_during_decode_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "image.infer-src-animation.threshold-ms",
   image_infer_src_animation_threshold_ms,
  RelaxedAtomicUint32, 2000
)

VARCACHE_PREF(
  Live,
  "image.layout_network_priority",
   image_layout_network_priority,
  RelaxedAtomicBool, true
)

// Chunk size for calls to the image decoders.
VARCACHE_PREF(
  Once,
  "image.mem.decode_bytes_at_a_time",
   image_mem_decode_bytes_at_a_time,
  uint32_t, 16384
)

VARCACHE_PREF(
  Live,
  "image.mem.discardable",
   image_mem_discardable,
  RelaxedAtomicBool, false
)

// Discards inactive image frames of _animated_ images and re-decodes them on
// demand from compressed data. Has no effect if image.mem.discardable is false.
VARCACHE_PREF(
  Once,
  "image.mem.animated.discardable",
   image_mem_animated_discardable,
  bool, true
)

VARCACHE_PREF(
  Live,
  "image.mem.animated.use_heap",
   image_mem_animated_use_heap,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.mem.debug-reporting",
   image_mem_debug_reporting,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "image.mem.shared",
   image_mem_shared,
  RelaxedAtomicBool, true
)

// How much of the data in the surface cache is discarded when we get a memory
// pressure notification, as a fraction. The discard factor is interpreted as a
// reciprocal, so a discard factor of 1 means to discard everything in the
// surface cache on memory pressure, a discard factor of 2 means to discard half
// of the data, and so forth. The default should be a good balance for desktop
// and laptop systems, where we never discard visible images.
VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.discard_factor",
   image_mem_surfacecache_discard_factor,
  uint32_t, 1
)

// Maximum size for the surface cache, in kilobytes.
VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.max_size_kb",
   image_mem_surfacecache_max_size_kb,
  uint32_t, 1024 * 1024
)

// Minimum timeout for expiring unused images from the surface cache, in
// milliseconds. This controls how long we store cached temporary surfaces.
VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.min_expiration_ms",
   image_mem_surfacecache_min_expiration_ms,
  uint32_t, 60*1000
)

// The surface cache's size, within the constraints of the maximum size set
// above, is determined as a fraction of main memory size. The size factor is
// interpreted as a reciprocal, so a size factor of 4 means to use no more than
// 1/4 of main memory.  The default should be a good balance for most systems.
VARCACHE_PREF(
  Once,
  "image.mem.surfacecache.size_factor",
   image_mem_surfacecache_size_factor,
  uint32_t, 4
)

VARCACHE_PREF(
  Live,
  "image.mem.volatile.min_threshold_kb",
   image_mem_volatile_min_threshold_kb,
  RelaxedAtomicInt32, -1
)

// How long in ms before we should start shutting down idle decoder threads.
VARCACHE_PREF(
  Once,
  "image.multithreaded_decoding.idle_timeout",
   image_multithreaded_decoding_idle_timeout,
  int32_t, 600000
)

// How many threads we'll use for multithreaded decoding. If < 0, will be
// automatically determined based on the system's number of cores.
VARCACHE_PREF(
  Once,
  "image.multithreaded_decoding.limit",
   image_multithreaded_decoding_limit,
  int32_t, -1
)

VARCACHE_PREF(
  Live,
  "image.webp.enabled",
   image_webp_enabled,
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
  RelaxedAtomicBool, true
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

// Whether to disable acceleration for all widgets.
VARCACHE_PREF(
  Once,
  "layers.acceleration.disabled",
   layers_acceleration_disabled_do_not_use_directly,
  bool, false
)
// Instead, use gfxConfig::IsEnabled(Feature::HW_COMPOSITING).

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps",
   layers_acceleration_draw_fps,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps.print-histogram",
   layers_acceleration_draw_fps_print_histogram,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.acceleration.draw-fps.write-to-file",
   layers_acceleration_draw_fps_write_to_file,
  RelaxedAtomicBool, false
)

// Whether to force acceleration on, ignoring blacklists.
#ifdef ANDROID
// bug 838603 -- on Android, accidentally blacklisting OpenGL layers
// means a startup crash for everyone.
// Temporarily force-enable GL compositing.  This is default-disabled
// deep within the bowels of the widgetry system.  Remove me when GL
// compositing isn't default disabled in widget/android.
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.acceleration.force-enabled",
   layers_acceleration_force_enabled_do_not_use_directly,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "layers.advanced.basic-layer.enabled",
   layers_advanced_basic_layer_enabled,
  RelaxedAtomicBool, false
)

// Whether we allow AMD switchable graphics.
VARCACHE_PREF(
  Once,
  "layers.amd-switchable-gfx.enabled",
   layers_amd_switchable_gfx_enabled,
  bool, true
)

// Whether to use async panning and zooming.
VARCACHE_PREF(
  Once,
  "layers.async-pan-zoom.enabled",
   layers_async_pan_zoom_enabled_do_not_use_directly,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layers.bench.enabled",
   layers_bench_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.bufferrotation.enabled",
   layers_bufferrotation_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layers.child-process-shutdown",
   layers_child_process_shutdown,
  RelaxedAtomicBool, true
)

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
// If MOZ_GFX_OPTIMIZE_MOBILE is defined, we force component alpha off
// and ignore the preference.
VARCACHE_PREF(
  Skip,
  "layers.componentalpha.enabled",
   layers_componentalpha_enabled,
  bool, false
)
#else
// If MOZ_GFX_OPTIMIZE_MOBILE is not defined, we actually take the
// preference value, defaulting to true.
VARCACHE_PREF(
  Once,
  "layers.componentalpha.enabled",
   layers_componentalpha_enabled,
  bool, true
)
#endif

VARCACHE_PREF(
  Once,
  "layers.d3d11.force-warp",
   layers_d3d11_force_warp,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.deaa.enabled",
   layers_deaa_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-bigimage-borders",
   layers_draw_bigimage_borders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-borders",
   layers_draw_borders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-tile-borders",
   layers_draw_tile_borders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-layer-info",
   layers_draw_layer_info,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump",
   layers_dump,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump-texture",
   layers_dump_texture,
  RelaxedAtomicBool, false
)

#ifdef MOZ_DUMP_PAINTING
VARCACHE_PREF(
  Live,
  "layers.dump-decision",
   layers_dump_decision,
  RelaxedAtomicBool, false
)
#endif

VARCACHE_PREF(
  Live,
  "layers.dump-client-layers",
   layers_dump_client_layers,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.dump-host-layers",
   layers_dump_host_layers,
  RelaxedAtomicBool, false
)

// 0 is "no change" for contrast, positive values increase it, negative values
// decrease it until we hit mid gray at -1 contrast, after that it gets weird.
VARCACHE_PREF(
  Live,
  "layers.effect.contrast",
   layers_effect_contrast,
  AtomicFloat, 0.0f
)

VARCACHE_PREF(
  Live,
  "layers.effect.grayscale",
   layers_effect_grayscale,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.effect.invert",
   layers_effect_invert,
  RelaxedAtomicBool, false
)

#if defined(XP_MACOSX) || defined (OS_OPENBSD)
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.enable-tiles",
   layers_enable_tiles,
  bool, PREF_VALUE
)
#undef PREF_VALUE

#if defined(XP_WIN)
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.enable-tiles-if-skia-pomtp",
   layers_enable_tiles_if_skia_pomtp,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "layers.flash-borders",
   layers_flash_borders,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.force-shmem-tiles",
   layers_force_shmem_tiles,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.draw-mask-debug",
   layers_draw_mask_debug,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.force-synchronous-resize",
   layers_force_synchronous_resize,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layers.geometry.opengl.enabled",
   layers_geometry_opengl_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.geometry.basic.enabled",
   layers_geometry_basic_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.geometry.d3d11.enabled",
   layers_geometry_d3d11_enabled,
  RelaxedAtomicBool, false
)

#if defined(XP_WIN)
#define PREF_VALUE true
#elif defined(MOZ_WIDGET_GTK)
#define PREF_VALUE false
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.gpu-process.allow-software",
   layers_gpu_process_allow_software,
  bool, PREF_VALUE
)
#undef PREF_VALUE

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.gpu-process.enabled",
   layers_gpu_process_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Once,
  "layers.gpu-process.force-enabled",
   layers_gpu_process_force_enabled,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.ipc_reply_timeout_ms",
   layers_gpu_process_ipc_reply_timeout_ms,
  int32_t, 10000
)

VARCACHE_PREF(
  Live,
  "layers.gpu-process.max_restarts",
   layers_gpu_process_max_restarts,
  RelaxedAtomicInt32, 1
)

// Note: This pref will only be used if it is less than layers.gpu-process.max_restarts.
VARCACHE_PREF(
  Live,
  "layers.gpu-process.max_restarts_with_decoder",
   layers_gpu_process_max_restarts_with_decoder,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Once,
  "layers.gpu-process.startup_timeout_ms",
   layers_gpu_process_startup_timeout_ms,
  int32_t, 5000
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-buffer",
   layers_low_precision_buffer,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-opacity",
   layers_low_precision_opacity,
  AtomicFloat, 1.0f
)

VARCACHE_PREF(
  Live,
  "layers.low-precision-resolution",
   layers_low_precision_resolution,
  AtomicFloat, 0.25f
)

VARCACHE_PREF(
  Live,
  "layers.max-active",
   layers_max_active,
  RelaxedAtomicInt32, -1
)

#if defined(XP_WIN)
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.mlgpu.enabled",
   layers_mlgpu_enabled_do_not_use_directly,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-buffer-cache",
   layers_mlgpu_enable_buffer_cache,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-buffer-sharing",
   layers_mlgpu_enable_buffer_sharing,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-clear-view",
   layers_mlgpu_enable_clear_view,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-cpu-occlusion",
   layers_mlgpu_enable_cpu_occlusion,
  bool, true
)

VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-depth-buffer",
   layers_mlgpu_enable_depth_buffer,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.mlgpu.enable-invalidation",
   layers_mlgpu_enable_invalidation,
  RelaxedAtomicBool, true
)

#if defined(XP_WIN)
// Both this and the master "enabled" pref must be on to use Advanced Layers
// on Windows 7.
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.mlgpu.enable-on-windows7",
   layers_mlgpu_enable_on_windows7,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Once,
  "layers.offmainthreadcomposition.force-disabled",
   layers_offmainthreadcomposition_force_disabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "layers.offmainthreadcomposition.frame-rate",
   layers_offmainthreadcomposition_frame_rate,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Once,
  "layers.omtp.capture-limit",
   layers_omtp_capture_limit,
  uint32_t, 25 * 1024 * 1024
)

VARCACHE_PREF(
  Live,
  "layers.omtp.dump-capture",
   layers_omtp_dump_capture,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "layers.omtp.paint-workers",
   layers_omtp_paint_workers,
  int32_t, -1
)

VARCACHE_PREF(
  Live,
  "layers.omtp.release-capture-on-main-thread",
   layers_omtp_release_capture_on_main_thread,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.orientation.sync.timeout",
   layers_orientation_sync_timeout,
  RelaxedAtomicUint32, (uint32_t)0
)

#ifdef XP_WIN
VARCACHE_PREF(
  Once,
  "layers.prefer-opengl",
   layers_prefer_opengl,
  bool, false
)
#endif

VARCACHE_PREF(
  Live,
  "layers.progressive-paint",
   layers_progressive_paint,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.shared-buffer-provider.enabled",
   layers_shared_buffer_provider_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.single-tile.enabled",
   layers_single_tile_enabled,
  RelaxedAtomicBool, true
)

// We allow for configurable and rectangular tile size to avoid wasting memory
// on devices whose screen size does not align nicely to the default tile size.
// Although layers can be any size, they are often the same size as the screen,
// especially for width.
VARCACHE_PREF(
  Once,
  "layers.tile-width",
   layers_tile_width,
  int32_t, 512
)

VARCACHE_PREF(
  Once,
  "layers.tile-height",
   layers_tile_height,
  int32_t, 512
)

VARCACHE_PREF(
  Once,
  "layers.tile-initial-pool-size",
   layers_tile_initial_pool_size,
  uint32_t, (uint32_t)50
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-unused-size",
   layers_tile_pool_unused_size,
  uint32_t, (uint32_t)10
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-shrink-timeout",
   layers_tile_pool_shrink_timeout,
  uint32_t, (uint32_t)50
)

VARCACHE_PREF(
  Once,
  "layers.tile-pool-clear-timeout",
   layers_tile_pool_clear_timeout,
  uint32_t, (uint32_t)5000
)

// If this is set the tile size will only be treated as a suggestion.
// On B2G we will round this to the stride of the underlying allocation.
// On any platform we may later use the screen size and ignore
// tile-width/tile-height entirely. Its recommended to turn this off
// if you change the tile size.
VARCACHE_PREF(
  Once,
  "layers.tiles.adjust",
   layers_tiles_adjust,
  bool, true
)

#ifdef MOZ_WIDGET_ANDROID
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Once,
  "layers.tiles.edge-padding",
   layers_tiles_edge_padding,
  bool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "layers.tiles.fade-in.enabled",
   layers_tiles_fade_in_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layers.tiles.fade-in.duration-ms",
   layers_tiles_fade_in_duration_ms,
  RelaxedAtomicUint32, 250
)

VARCACHE_PREF(
  Live,
  "layers.tiles.retain-back-buffer",
   layers_tiles_retain_back_buffer,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layers.transaction.warning-ms",
   layers_transaction_warning_ms,
  RelaxedAtomicUint32, 200
)

VARCACHE_PREF(
  Once,
  "layers.uniformity-info",
   layers_uniformity_info,
  bool, false
)

VARCACHE_PREF(
  Once,
  "layers.use-image-offscreen-surfaces",
   layers_use_image_offscreen_surfaces,
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
   layout_animation_prerender_partial,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.viewport-ratio-limit-x",
   layout_animation_prerender_viewport_ratio_limit_x,
  AtomicFloat, 1.125f
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.viewport-ratio-limit-y",
   layout_animation_prerender_viewport_ratio_limit_y,
  AtomicFloat, 1.125f
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.absolute-limit-x",
   layout_animation_prerender_absolute_limit_x,
  RelaxedAtomicUint32, 4096
)

VARCACHE_PREF(
  Live,
  "layout.animation.prerender.absolute-limit-y",
   layout_animation_prerender_absolute_limit_y,
  RelaxedAtomicUint32, 4096
)

// Is the codepath for using cached scrollbar styles enabled?
VARCACHE_PREF(
  Live,
  "layout.css.cached-scrollbar-styles.enabled",
  layout_css_cached_scrollbar_styles_enabled,
  bool, true
)

// Is path() supported in clip-path?
VARCACHE_PREF(
  Live,
  "layout.css.clip-path-path.enabled",
  layout_css_clip_path_path_enabled,
  bool, false
)

// text underline offset
VARCACHE_PREF(
  Live,
  "layout.css.text-underline-offset.enabled",
  layout_css_text_underline_offset_enabled,
  bool, false
)

// text decoration width
VARCACHE_PREF(
  Live,
  "layout.css.text-decoration-width.enabled",
  layout_css_text_decoration_width_enabled,
  bool, false
)

// text decoration skip ink
VARCACHE_PREF(
  Live,
  "layout.css.text-decoration-skip-ink.enabled",
  layout_css_text_decoration_skip_ink_enabled,
  bool, false
)

// Is support for CSS column-span enabled?
VARCACHE_PREF(
  Live,
  "layout.css.column-span.enabled",
  layout_css_column_span_enabled,
  bool, false
)

// Is support for CSS backdrop-filter enabled?
VARCACHE_PREF(
  Live,
  "layout.css.backdrop-filter.enabled",
  layout_css_backdrop_filter_enabled,
  bool, false
)

// Is support for CSS contain enabled?
VARCACHE_PREF(
  Live,
  "layout.css.contain.enabled",
  layout_css_contain_enabled,
  bool, true
)

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

// Is support for GeometryUtils.convert*FromNode enabled?
VARCACHE_PREF(
  Live,
  "layout.css.convertFromNode.enabled",
  layout_css_convertFromNode_enabled,
  bool, NOT_IN_RELEASE_OR_BETA_VALUE
)

// Is support for DOMMatrix enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMMatrix.enabled",
  layout_css_DOMMatrix_enabled,
  RelaxedAtomicBool, true
)

// Is support for DOMQuad enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMQuad.enabled",
  layout_css_DOMQuad_enabled,
  RelaxedAtomicBool, true
)

// Is support for DOMPoint enabled?
VARCACHE_PREF(
  Live,
  "layout.css.DOMPoint.enabled",
  layout_css_DOMPoint_enabled,
  RelaxedAtomicBool, true
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

// Is support for CSS individual transform enabled?
VARCACHE_PREF(
  Live,
  "layout.css.individual-transform.enabled",
  layout_css_individual_transform_enabled,
  bool, false
)

// Is support for CSS initial-letter property enabled?
VARCACHE_PREF(
  Live,
  "layout.css.initial-letter.enabled",
  layout_css_initial_letter_enabled,
  bool, false
)

// Pref to control whether line-height: -moz-block-height is exposed to content.
VARCACHE_PREF(
  Live,
  "layout.css.line-height-moz-block-height.content.enabled",
  layout_css_line_height_moz_block_height_content_enabled,
  bool, false
)

// Is support for motion-path enabled?
VARCACHE_PREF(
  Live,
  "layout.css.motion-path.enabled",
  layout_css_motion_path_enabled,
  bool, NOT_IN_RELEASE_OR_BETA_VALUE
)

// Is support for -moz-binding enabled?
VARCACHE_PREF(
  Live,
  "layout.css.moz-binding.content.enabled",
  layout_css_moz_binding_content_enabled,
  bool, false
)

// Pref to control whether the ::marker property restrictions defined in [1]
// apply.
//
// [1]: https://drafts.csswg.org/css-pseudo-4/#selectordef-marker
VARCACHE_PREF(
  Live,
  "layout.css.marker.restricted",
   layout_css_marker_restricted,
  bool, true
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

// Is -moz-osx-font-smoothing enabled? (Only supported in OSX builds)
#if defined(XP_MACOSX)
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.osx-font-smoothing.enabled",
  layout_css_osx_font_smoothing_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for CSS overflow-clip-box enabled for non-UA sheets?
VARCACHE_PREF(
  Live,
  "layout.css.overflow-clip-box.enabled",
  layout_css_overflow_clip_box_enabled,
  bool, false
)

// Is support for overscroll-behavior enabled?
VARCACHE_PREF(
  Live,
  "layout.css.overscroll-behavior.enabled",
  layout_css_overscroll_behavior_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.overflow-logical.enabled",
  layout_css_overflow_logical_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.paint-order.enabled",
   layout_css_paint_order_enabled,
  RelaxedAtomicBool, false
)

// Is parallel CSS parsing enabled?
VARCACHE_PREF(
  Live,
  "layout.css.parsing.parallel",
  layout_css_parsing_parallel,
  bool, true
)

// Is support for -moz-prefixed animation properties enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.animations",
  layout_css_prefixes_animations,
  bool, true
)

// Is support for -moz-border-image enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.border-image",
  layout_css_prefixes_border_image,
  bool, true
)

// Is support for -moz-box-sizing enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.box-sizing",
  layout_css_prefixes_box_sizing,
  bool, true
)

// Are "-webkit-{min|max}-device-pixel-ratio" media queries supported?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.device-pixel-ratio-webkit",
  layout_css_prefixes_device_pixel_ratio_webkit,
  bool, true
)

// Is support for -moz-prefixed font feature properties enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.font-features",
  layout_css_prefixes_font_features,
  bool, true
)

// Is support for -moz-prefixed transform properties enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.transforms",
  layout_css_prefixes_transforms,
  bool, true
)

// Is support for -moz-prefixed transition properties enabled?
VARCACHE_PREF(
  Live,
  "layout.css.prefixes.transitions",
  layout_css_prefixes_transitions,
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
  "layout.css.resizeobserver.enabled",
   layout_css_resizeobserver_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.damping-ratio",
   layout_css_scroll_behavior_damping_ratio,
  AtomicFloat, 1.0f
)

// Is support for scrollbar-color property enabled?
VARCACHE_PREF(
  Live,
  "layout.css.scrollbar-color.enabled",
  layout_css_scrollbar_color_enabled,
  bool, true
)

// Is support for scrollbar-width property enabled?
VARCACHE_PREF(
  Live,
  "layout.css.scrollbar-width.enabled",
  layout_css_scrollbar_width_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.supports-selector.enabled",
  layout_css_supports_selector_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.enabled",
   layout_css_scroll_behavior_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-behavior.spring-constant",
   layout_css_scroll_behavior_spring_constant,
  AtomicFloat, 250.0f
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.prediction-max-velocity",
   layout_css_scroll_snap_prediction_max_velocity,
  RelaxedAtomicInt32, 2000
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.prediction-sensitivity",
   layout_css_scroll_snap_prediction_sensitivity,
  AtomicFloat, 0.750f
)

VARCACHE_PREF(
  Live,
  "layout.css.scroll-snap.proximity-threshold",
   layout_css_scroll_snap_proximity_threshold,
  RelaxedAtomicInt32, 200
)

// Is steps(jump-*) supported in easing functions?
VARCACHE_PREF(
  Live,
  "layout.css.step-position-jump.enabled",
  layout_css_step_position_jump_enabled,
  bool, true
)

// W3C touch-action css property (related to touch and pointer events)
// Note that we turn this on even on platforms/configurations where touch
// events are not supported (e.g. OS X, or Windows with e10s disabled). For
// those platforms we don't handle touch events anyway so it's conceptually
// a no-op.
VARCACHE_PREF(
  Live,
  "layout.css.touch_action.enabled",
  layout_css_touch_action_enabled,
  RelaxedAtomicBool, true
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
   layout_display_list_build_twice,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain",
   layout_display_list_retain,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.chrome",
   layout_display_list_retain_chrome,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.verify",
   layout_display_list_retain_verify,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.retain.verify.order",
   layout_display_list_retain_verify_order,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.rebuild-frame-limit",
   layout_display_list_rebuild_frame_limit,
  RelaxedAtomicUint32, 500
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump",
   layout_display_list_dump,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump-content",
   layout_display_list_dump_content,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.dump-parent",
   layout_display_list_dump_parent,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.show-rebuild-area",
   layout_display_list_show_rebuild_area,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.display-list.flatten-transform",
   layout_display_list_flatten_transform,
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
   layout_frame_rate,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "layout.min-active-layer-size",
   layout_min_active_layer_size,
  int, 64
)

VARCACHE_PREF(
  Once,
  "layout.paint_rects_separately",
   layout_paint_rects_separately,
  bool, true
)

// This and code dependent on it should be removed once containerless scrolling looks stable.
VARCACHE_PREF(
  Live,
  "layout.scroll.root-frame-containers",
   layout_scroll_root_frame_containers,
  RelaxedAtomicBool, false
)

// This pref is to be set by test code only.
VARCACHE_PREF(
  Live,
  "layout.scrollbars.always-layerize-track",
   layout_scrollbars_always_layerize_track,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "layout.smaller-painted-layers",
   layout_smaller_painted_layers,
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
  "layout.css.shadow-parts.enabled",
   layout_css_shadow_parts_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for CSS text-justify property enabled?
VARCACHE_PREF(
  Live,
  "layout.css.text-justify.enabled",
  layout_css_text_justify_enabled,
  bool, true
)

// Is support for -webkit-line-clamp enabled?
VARCACHE_PREF(
  Live,
  "layout.css.webkit-line-clamp.enabled",
  layout_css_webkit_line_clamp_enabled,
  bool, true
)

// Whether the computed value of line-height: normal returns the `normal`
// keyword rather than a pixel value based on the first available font.
//
// Only enabled on Nightly and early beta, at least for now.
//
// NOTE(emilio): If / when removing this pref, the GETCS_NEEDS_LAYOUT_FLUSH flag
// should be removed from line-height (and we should let -moz-block-height
// compute to the keyword as well, which shouldn't be observable anyway since
// it's an internal value).
//
// It'd be nice to make numbers compute also to themselves, but it looks like
// everybody agrees on turning them into pixels, see the discussion starting
// from [1].
//
// [1]: https://github.com/w3c/csswg-drafts/issues/3749#issuecomment-477287453
#ifdef EARLY_BETA_OR_EARLIER
#define PREF_VALUE true
#else
#define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "layout.css.line-height.normal-as-resolved-value.enabled",
  layout_css_line_height_normal_as_resolved_value_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef EARLY_BETA_OR_EARLIER
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
// Are the width and height attributes on image-like elements mapped to the
// internal-for-now aspect-ratio property?
VARCACHE_PREF(
  Live,
  "layout.css.width-and-height-map-to-aspect-ratio.enabled",
  layout_css_width_and_height_map_to_aspect_ratio_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

//---------------------------------------------------------------------------
// Prefs starting with "media."
//---------------------------------------------------------------------------

// These prefs use camel case instead of snake case for the getter because one
// reviewer had an unshakeable preference for that. Who could that be?

VARCACHE_PREF(
  Live,
  "media.autoplay.enabled.user-gestures-needed",
   media_autoplay_enabled_user_gestures_needed,
  bool, false
)

// File-backed MediaCache size.
VARCACHE_PREF(
  Live,
  "media.cache_size",
   media_cache_size,
  RelaxedAtomicUint32, 512000 // Measured in KiB
)

// Size of file backed MediaCache while on a connection which is cellular (3G,
// etc), and thus assumed to be "expensive".
VARCACHE_PREF(
  Live,
  "media.cache_size.cellular",
   media_cache_size_cellular,
  RelaxedAtomicUint32, 32768 // Measured in KiB
)

// If a resource is known to be smaller than this size (in kilobytes), a
// memory-backed MediaCache may be used; otherwise the (single shared global)
// file-backed MediaCache is used.
VARCACHE_PREF(
  Live,
  "media.memory_cache_max_size",
   media_memory_cache_max_size,
  uint32_t, 8192      // Measured in KiB
)

// Don't create more memory-backed MediaCaches if their combined size would go
// above this absolute size limit.
VARCACHE_PREF(
  Live,
  "media.memory_caches_combined_limit_kb",
   media_memory_caches_combined_limit_kb,
  uint32_t, 524288
)

// Don't create more memory-backed MediaCaches if their combined size would go
// above this relative size limit (a percentage of physical memory).
VARCACHE_PREF(
  Live,
  "media.memory_caches_combined_limit_pc_sysmem",
   media_memory_caches_combined_limit_pc_sysmem,
  uint32_t, 5         // A percentage
)

// When a network connection is suspended, don't resume it until the amount of
// buffered data falls below this threshold (in seconds).
VARCACHE_PREF(
  Live,
  "media.cache_resume_threshold",
   media_cache_resume_threshold,
  RelaxedAtomicUint32, 30
)
VARCACHE_PREF(
  Live,
  "media.cache_resume_threshold.cellular",
   media_cache_resume_threshold_cellular,
  RelaxedAtomicUint32, 10
)

// Stop reading ahead when our buffered data is this many seconds ahead of the
// current playback position. This limit can stop us from using arbitrary
// amounts of network bandwidth prefetching huge videos.
VARCACHE_PREF(
  Live,
  "media.cache_readahead_limit",
   media_cache_readahead_limit,
  RelaxedAtomicUint32, 60
)
VARCACHE_PREF(
  Live,
  "media.cache_readahead_limit.cellular",
   media_cache_readahead_limit_cellular,
  RelaxedAtomicUint32, 30
)

// AudioSink
VARCACHE_PREF(
  Live,
  "media.resampling.enabled",
   media_resampling_enabled,
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
   media_forcestereo_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// VideoSink
VARCACHE_PREF(
  Live,
  "media.ruin-av-sync.enabled",
   media_ruin_av_sync_enabled,
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
   media_eme_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Whether we expose the functionality proposed in
// https://github.com/WICG/encrypted-media-encryption-scheme/blob/master/explainer.md
// I.e. if true, apps calling navigator.requestMediaKeySystemAccess() can pass
// an optional encryption scheme as part of MediaKeySystemMediaCapability
// objects. If a scheme is present when we check for support, we must ensure we
// support that scheme in order to provide key system access.
VARCACHE_PREF(
  Live,
  "media.eme.encrypted-media-encryption-scheme.enabled",
  media_eme_encrypted_media_encryption_scheme_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.clearkey.persistent-license.enabled",
   media_clearkey_persistent_license_enabled,
  bool, false
)

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
// Whether to allow, on a Linux system that doesn't support the necessary
// sandboxing features, loading Gecko Media Plugins unsandboxed.  However, EME
// CDMs will not be loaded without sandboxing even if this pref is changed.
VARCACHE_PREF(
  Live,
  "media.gmp.insecure.allow",
   media_gmp_insecure_allow,
  RelaxedAtomicBool, false
)
#endif

// Specifies whether the PDMFactory can create a test decoder that just outputs
// blank frames/audio instead of actually decoding. The blank decoder works on
// all platforms.
VARCACHE_PREF(
  Live,
  "media.use-blank-decoder",
   media_use_blank_decoder,
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
   media_gpu_process_decoder,
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
   media_rdd_process_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.rdd-process.startup_timeout_ms",
   media_rdd_process_startup_timeout_ms,
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
   media_rdd_vorbis_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#ifdef ANDROID

// Enable the MediaCodec PlatformDecoderModule by default.
VARCACHE_PREF(
  Live,
  "media.android-media-codec.enabled",
   media_android_media_codec_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.android-media-codec.preferred",
   media_android_media_codec_preferred,
  RelaxedAtomicBool, true
)

#endif // ANDROID

#ifdef MOZ_OMX
VARCACHE_PREF(
  Live,
  "media.omx.enabled",
   media_omx_enabled,
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
   media_ffmpeg_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.libavcodec.allow-obsolete",
   media_libavcodec_allow_obsolete,
  bool, false
)

#endif // MOZ_FFMPEG

#ifdef MOZ_FFVPX
VARCACHE_PREF(
  Live,
  "media.ffvpx.enabled",
   media_ffvpx_enabled,
  RelaxedAtomicBool, true
)
#endif

#if defined(MOZ_FFMPEG) || defined(MOZ_FFVPX)
VARCACHE_PREF(
  Live,
  "media.ffmpeg.low-latency.enabled",
   media_ffmpeg_low_latency_enabled,
  RelaxedAtomicBool, false
)
#endif

#ifdef MOZ_WMF

VARCACHE_PREF(
  Live,
  "media.wmf.enabled",
   media_wmf_enabled,
  RelaxedAtomicBool, true
)

// Whether DD should consider WMF-disabled a WMF failure, useful for testing.
VARCACHE_PREF(
  Live,
  "media.decoder-doctor.wmf-disabled-is-failure",
   media_decoder_doctor_wmf_disabled_is_failure,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.dxva.d3d11.enabled",
   media_wmf_dxva_d3d11_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.dxva.max-videos",
   media_wmf_dxva_max_videos,
  RelaxedAtomicUint32, 8
)

VARCACHE_PREF(
  Live,
  "media.wmf.use-nv12-format",
   media_wmf_use_nv12_format,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.force.allow-p010-format",
   media_wmf_force_allow_p010_format,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "media.wmf.use-sync-texture",
   media_wmf_use_sync_texture,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.low-latency.enabled",
   media_wmf_low_latency_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.low-latency.force-disabled",
   media_mwf_low_latency_force_disabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.skip-blacklist",
   media_wmf_skip_blacklist,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.deblacklisting-for-telemetry-in-gpu-process",
   media_wmf_deblacklisting_for_telemetry_in_gpu_process,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.wmf.amd.highres.enabled",
   media_wmf_amd_highres_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.wmf.allow-unsupported-resolutions",
   media_wmf_allow_unsupported_resolutions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Once,
  "media.wmf.vp9.enabled",
   media_wmf_vp9_enabled,
  bool, true
)
#endif // MOZ_WMF

VARCACHE_PREF(
  Once,
  "media.hardware-video-decoding.force-enabled",
   media_hardware_video_decoding_force_enabled,
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
   media_decoder_recycle_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Should MFR try to skip to the next key frame?
VARCACHE_PREF(
  Live,
  "media.decoder.skip-to-next-key-frame.enabled",
   media_decoder_skip_to_next_key_frame_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.gmp.decoder.enabled",
   media_gmp_decoder_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.eme.audio.blank",
   media_eme_audio_blank,
  RelaxedAtomicBool, false
)
VARCACHE_PREF(
  Live,
  "media.eme.video.blank",
   media_eme_video_blank,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.eme.chromium-api.video-shmems",
   media_eme_chromium_api_video_shmems,
  RelaxedAtomicUint32, 6
)

// Whether to suspend decoding of videos in background tabs.
VARCACHE_PREF(
  Live,
  "media.suspend-bkgnd-video.enabled",
   media_suspend_bkgnd_video_enabled,
  RelaxedAtomicBool, true
)

// Delay, in ms, from time window goes to background to suspending
// video decoders. Defaults to 10 seconds.
VARCACHE_PREF(
  Live,
  "media.suspend-bkgnd-video.delay-ms",
   media_suspend_bkgnd_video_delay_ms,
  RelaxedAtomicUint32, 10000
)

VARCACHE_PREF(
  Live,
  "media.dormant-on-pause-timeout-ms",
   media_dormant_on_pause_timeout_ms,
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

// This pref controls whether dispatch testing-only events.
VARCACHE_PREF(
  Live,
  "media.webvtt.testing.events",
  media_webvtt_testing_events,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.webspeech.synth.force_global_queue",
   media_webspeech_synth_force_global_queue,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.enable",
   media_webspeech_test_enable,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.fake_fsm_events",
   media_webspeech_test_fake_fsm_events,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.webspeech.test.fake_recognition_service",
   media_webspeech_test_fake_recognition_service,
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
   media_webspeech_recognition_force_enable,
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
   media_encoder_webm_enabled,
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
   media_audio_max_decode_error,
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
   media_video_max_decode_error,
  uint32_t, PREF_VALUE
)
#undef PREF_VALUE

// Opus
VARCACHE_PREF(
  Live,
  "media.opus.enabled",
   media_opus_enabled,
  RelaxedAtomicBool, true
)

// Wave
VARCACHE_PREF(
  Live,
  "media.wave.enabled",
   media_wave_enabled,
  RelaxedAtomicBool, true
)

// Ogg
VARCACHE_PREF(
  Live,
  "media.ogg.enabled",
   media_ogg_enabled,
  RelaxedAtomicBool, true
)

// WebM
VARCACHE_PREF(
  Live,
  "media.webm.enabled",
   media_webm_enabled,
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
   media_av1_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "media.av1.use-dav1d",
   media_av1_use_dav1d,
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
   media_flac_enabled,
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
   media_hls_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Max number of HLS players that can be created concurrently. Used only on
// Android and when "media.hls.enabled" is true.
#ifdef ANDROID
VARCACHE_PREF(
  Live,
  "media.hls.max-allocations",
   media_hls_max_allocations,
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
   media_mp4_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

// Error/warning handling, Decoder Doctor.
//
// Set to true to force demux/decode warnings to be treated as errors.
VARCACHE_PREF(
  Live,
  "media.playback.warnings-as-errors",
   media_playback_warnings_as_errors,
  RelaxedAtomicBool, false
)

// Resume video decoding when the cursor is hovering on a background tab to
// reduce the resume latency and improve the user experience.
VARCACHE_PREF(
  Live,
  "media.resume-bkgnd-video-on-tabhover",
   media_resume_bkgnd_video_on_tabhover,
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
  media_videocontrols_lock_video_orientation,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Media Seamless Looping
VARCACHE_PREF(
  Live,
  "media.seamless-looping",
   media_seamless_looping,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.autoplay.block-event.enabled",
   media_autoplay_block_event_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.media-capabilities.enabled",
   media_media_capabilities_enabled,
  RelaxedAtomicBool, true
)

VARCACHE_PREF(
  Live,
  "media.media-capabilities.screen.enabled",
   media_media_capabilities_screen_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.fps",
   media_benchmark_vp9_fps,
  RelaxedAtomicUint32, 0
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.threshold",
   media_benchmark_vp9_threshold,
  RelaxedAtomicUint32, 150
)

VARCACHE_PREF(
  Live,
  "media.benchmark.vp9.versioncheck",
   media_benchmark_vp9_versioncheck,
  RelaxedAtomicUint32, 0
)

VARCACHE_PREF(
  Live,
  "media.benchmark.frames",
   media_benchmark_frames,
  RelaxedAtomicUint32, 300
)

VARCACHE_PREF(
  Live,
  "media.benchmark.timeout",
   media_benchmark_timeout,
  RelaxedAtomicUint32, 1000
)

VARCACHE_PREF(
  Live,
  "media.test.video-suspend",
  media_test_video_suspend,
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
  bool, false
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

VARCACHE_PREF(
  Live,
  "media.peerconnection.dtmf.enabled",
  media_peerconnection_dtmf_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.peerconnection.identity.enabled",
  media_peerconnection_identity_enabled,
  bool, true
)

VARCACHE_PREF(
  Live,
  "media.peerconnection.rtpsourcesapi.enabled",
  media_peerconnection_rtpsourcesapi_enabled,
  bool, true
)

#ifdef MOZ_WEBRTC
#ifdef ANDROID

VARCACHE_PREF(
  Live,
  "media.navigator.hardware.vp8_encode.acceleration_remote_enabled",
   media_navigator_hardware_vp8_encode_acceleration_remote_enabled,
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
   media_navigator_mediadatadecoder_vpx_enabled,
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
   media_navigator_mediadatadecoder_h264_enabled,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

#endif // MOZ_WEBRTC

// HTMLMediaElement.allowedToPlay should be exposed to web content when
// block autoplay rides the trains to release. Until then, Nightly only.
#ifdef NIGHTLY_BUILD
# define PREF_VALUE true
#else
# define PREF_VALUE false
#endif
VARCACHE_PREF(
  Live,
  "media.allowed-to-play.enabled",
  media_allowed_to_play_enabled,
  bool, PREF_VALUE
)
#undef PREF_VALUE

// Is support for MediaKeys.getStatusForPolicy enabled?
VARCACHE_PREF(
  Live,
  "media.eme.hdcp-policy-check.enabled",
  media_eme_hdcp_policy_check_enabled,
  bool, false
)

// Is support for MediaDevices.ondevicechange enabled?
VARCACHE_PREF(
  Live,
  "media.ondevicechange.enabled",
  media_ondevicechange_enabled,
  bool, true
)

// Is support for HTMLMediaElement.seekToNextFrame enabled?
VARCACHE_PREF(
  Live,
  "media.seekToNextFrame.enabled",
  media_seekToNextFrame_enabled,
  bool, true
)

// setSinkId will be enabled in bug 1498512. Till then the
// implementation will remain hidden behind this pref (Bug 1152401, Bug 934425).
VARCACHE_PREF(
  Live,
  "media.setsinkid.enabled",
  media_setsinkid_enabled,
  bool, false
)

VARCACHE_PREF(
  Live,
  "media.useAudioChannelService.testing",
  media_useAudioChannelService_testing,
  bool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "mousewheel."
//---------------------------------------------------------------------------

// These affect how line scrolls from wheel events will be accelerated.
VARCACHE_PREF(
  Live,
  "mousewheel.acceleration.factor",
   mousewheel_acceleration_factor,
  RelaxedAtomicInt32, -1
)

VARCACHE_PREF(
  Live,
  "mousewheel.acceleration.start",
   mousewheel_acceleration_start,
  RelaxedAtomicInt32, -1
)

// This affects whether events will be routed through APZ or not.
VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.enabled",
   mousewheel_system_scroll_override_on_root_content_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.horizontal.factor",
   mousewheel_system_scroll_override_on_root_content_horizontal_factor,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "mousewheel.system_scroll_override_on_root_content.vertical.factor",
   mousewheel_system_scroll_override_on_root_content_vertical_factor,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "mousewheel.transaction.ignoremovedelay",
   mousewheel_transaction_ignoremovedelay,
  RelaxedAtomicInt32, (int32_t)100
)

VARCACHE_PREF(
  Live,
  "mousewheel.transaction.timeout",
   mousewheel_transaction_timeout,
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
  "network.cookie.sameSite.laxByDefault",
   network_cookie_sameSite_laxByDefault,
  bool, false
)

VARCACHE_PREF(
  Live,
  "network.cookie.sameSite.noneRequiresSecure",
   network_cookie_sameSite_noneRequiresSecure,
  bool, false
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
   nglayout_debug_widget_update_flashing,
  RelaxedAtomicBool, false
)

//---------------------------------------------------------------------------
// Prefs starting with "plain_text."
//---------------------------------------------------------------------------

// When false, text in plaintext documents does not wrap long lines.
VARCACHE_PREF(
  Live,
  "plain_text.wrap_long_lines",
  plain_text_wrap_long_lines,
  bool, true
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

#ifdef DEBUG
// If set to true, setting a Preference matched to a `Once` StaticPref will
// assert that the value matches. Such assertion being broken is a clear flag
// that the Once policy shouldn't be used.
VARCACHE_PREF(
  Live,
  "preferences.check.once.policy",
  preferences_check_once_policy,
  bool, false
)

// If set to true, StaticPrefs Once policy check will be skipped during
// automation regression test. Use with care. This pref must be set back to
// false as soon as specific test has completed.
VARCACHE_PREF(
  Live,
  "preferences.force-disable.check.once.policy",
  preferences_force_disable_check_once_policy,
  bool, false
)
#endif

//---------------------------------------------------------------------------
// Prefs starting with "print."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "print.font-variations-as-paths",
   print_font_variations_as_paths,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "privacy."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "privacy.file_unique_origin",
   privacy_file_unique_origin,
  bool, true
)

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

// Block 3rd party socialtracking resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.socialtracking.enabled",
  privacy_trackingprotection_socialtracking_enabled,
  bool, false
)

// Annotate socialtracking resources.
VARCACHE_PREF(
  Live,
  "privacy.trackingprotection.socialtracking.annotate.enabled",
  privacy_trackingprotection_socialtracking_annotate_enabled,
  bool, false
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

PREF("privacy.resistFingerprinting", bool, false)

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

// Scrollbar snapping region.
// - 0: off
// - 1 and higher: slider thickness multiple
VARCACHE_PREF(
  Once,
  "slider.snapMultiplier",
   slider_snapMultiplier,
  int32_t,
#ifdef XP_WIN
  6
#else
  0
#endif
)

//---------------------------------------------------------------------------
// Prefs starting with "svg."
//---------------------------------------------------------------------------

// Is support for transform-box enabled?
VARCACHE_PREF(
  Live,
  "svg.transform-box.enabled",
  svg_transform_box_enabled,
  bool, true
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
   test_events_async_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "test.mousescroll",
   test_mousescroll,
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
   toolkit_scrollbox_horizontalScrollDistance,
  RelaxedAtomicInt32, 5
)

VARCACHE_PREF(
  Live,
  "toolkit.scrollbox.verticalScrollDistance",
   toolkit_scrollbox_verticalScrollDistance,
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
   ui_click_hold_context_menus_delay,
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
   webgl_1_allow_core_profiles,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.all-angle-options",
   webgl_all_angle_options,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.force-d3d11",
   webgl_angle_force_d3d11,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.try-d3d11",
   webgl_angle_try_d3d11,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.angle.force-warp",
   webgl_angle_force_warp,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.can-lose-context-in-foreground",
   webgl_can_lose_context_in_foreground,
  RelaxedAtomicBool, true
)

#ifdef MOZ_WIDGET_ANDROID
# define PREF_VALUE false
#else
# define PREF_VALUE true
#endif
VARCACHE_PREF(
  Live,
  "webgl.default-antialias",
   webgl_default_antialias,
  RelaxedAtomicBool, PREF_VALUE
)
#undef PREF_VALUE

VARCACHE_PREF(
  Live,
  "webgl.default-low-power",
   webgl_default_low_power,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.default-no-alpha",
   webgl_default_no_alpha,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-angle",
   webgl_disable_angle,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-wgl",
   webgl_disable_wgl,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-extensions",
   webgl_disable_extensions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.dxgl.enabled",
   webgl_dxgl_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.dxgl.needs-finish",
   webgl_dxgl_needs_finish,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.disable-fail-if-major-performance-caveat",
   webgl_disable_fail_if_major_performance_caveat,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disable-DOM-blit-uploads",
   webgl_disable_DOM_blit_uploads,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.disabled",
   webgl_disabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-draft-extensions",
   webgl_enable_draft_extensions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-privileged-extensions",
   webgl_enable_privileged_extensions,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.enable-surface-texture",
   webgl_enable_surface_texture,
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
   webgl_force_enabled,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.force-layers-readback",
   webgl_force_layers_readback,
  bool, false
)

VARCACHE_PREF(
  Live,
  "webgl.force-index-validation",
   webgl_force_index_validation,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.lose-context-on-memory-pressure",
   webgl_lose_context_on_memory_pressure,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.max-contexts",
   webgl_max_contexts,
  RelaxedAtomicUint32, 32
)

VARCACHE_PREF(
  Live,
  "webgl.max-contexts-per-principal",
   webgl_max_contexts_per_principal,
  RelaxedAtomicUint32, 16
)

VARCACHE_PREF(
  Live,
  "webgl.max-warnings-per-context",
   webgl_max_warnings_per_context,
  RelaxedAtomicUint32, 32
)

VARCACHE_PREF(
  Live,
  "webgl.min_capability_mode",
   webgl_min_capability_mode,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.msaa-force",
   webgl_msaa_force,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.msaa-samples",
   webgl_msaa_samples,
  RelaxedAtomicUint32, 4
)

PREF("webgl.prefer-16bpp", bool, false)

VARCACHE_PREF(
  Live,
  "webgl.allow-immediate-queries",
   webgl_allow_immediate_queries,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "webgl.allow-fb-invalidation",
   webgl_allow_fb_invalidation,
  RelaxedAtomicBool, false
)


VARCACHE_PREF(
  Live,
  "webgl.perf.max-warnings",
   webgl_perf_max_warnings,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.perf.max-acceptable-fb-status-invals",
   webgl_perf_max_acceptable_fb_status_invals,
  RelaxedAtomicInt32, 0
)

VARCACHE_PREF(
  Live,
  "webgl.perf.spew-frame-allocs",
   webgl_perf_spew_frame_allocs,
  RelaxedAtomicBool, true
)

//---------------------------------------------------------------------------
// Prefs starting with "widget."
//---------------------------------------------------------------------------

VARCACHE_PREF(
  Live,
  "widget.window-transforms.disabled",
   widget_window_transforms_disabled,
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
