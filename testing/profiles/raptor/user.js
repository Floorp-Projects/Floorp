/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preferences file used by the raptor harness
/* globals user_pref */
// prevents normandy from running updates during the tests
user_pref("app.normandy.enabled", false);

user_pref("dom.performance.time_to_non_blank_paint.enabled", true);
user_pref("dom.performance.time_to_contentful_paint.enabled", true);
user_pref("dom.performance.time_to_dom_content_flushed.enabled", true);
user_pref("dom.performance.time_to_first_interactive.enabled", true);

// required for geckoview logging
user_pref("geckoview.console.enabled", true);

// get the console logging out of the webext into the stdout
user_pref("browser.dom.window.dump.enabled", true);
user_pref("devtools.console.stdout.chrome", true);
user_pref("devtools.console.stdout.content", true);

// prevent pages from opening after a crash
user_pref("browser.sessionstore.resume_from_crash", false);

// disable the background hang monitor
user_pref("toolkit.content-background-hang-monitor.disabled", true);

// disable async stacks to match release builds
// https://developer.mozilla.org/en-US/docs/Mozilla/Benchmarking#Async_Stacks
user_pref('javascript.options.asyncstack', false);

// disable Firefox Telemetry (and some other things too)
// https://bugzilla.mozilla.org/show_bug.cgi?id=1533879
user_pref('datareporting.healthreport.uploadEnabled', false);

// Telemetry initialization happens on a delay, that may elapse exactly in the
// middle of some raptor tests. While it doesn't do a lot of expensive work, it
// causes some I/O and thread creation, that can add noise to performance
// profiles we use to analyze performance regressions.
// https://bugzilla.mozilla.org/show_bug.cgi?id=1706180
user_pref('toolkit.telemetry.initDelay', 99999999);

// disable autoplay for raptor tests
user_pref('media.autoplay.default', 5);
user_pref('media.autoplay.ask-permission', true);
user_pref('media.autoplay.blocking_policy', 1);
user_pref('media.allowed-to-play.enabled', false);
user_pref('media.block-autoplay-until-in-foreground', true);
