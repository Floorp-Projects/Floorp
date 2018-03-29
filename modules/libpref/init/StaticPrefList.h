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
// End of prefs
//---------------------------------------------------------------------------

// clang-format on
