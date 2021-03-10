/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Base preferences file for enabling unittest features.
// Not enabling these features cause many unittests that
// expect these prefs to be set to fail.
// Ideally, tests depending on these prefs should be setting
// the prefs themselves in their manifests...
// NOTE: Setting prefs to enable unittests to run at all
// should occur in unittest-required/user.js, not here!
/* globals user_pref */
// Enable w3c touch events for testing
user_pref("dom.w3c_touch_events.enabled", 1);
// Enable CSS Grid 'subgrid' feature for testing
user_pref("layout.css.grid-template-subgrid-value.enabled", true);
// Enable CSS initial-letter for testing
user_pref("layout.css.initial-letter.enabled", true);
// Enable Media Source Extensions for testing
user_pref("media.mediasource.mp4.enabled", true);
user_pref("media.mediasource.webm.enabled", true);
user_pref("media.av1.enabled", true);
user_pref("media.eme.enabled", true);
user_pref("dom.forms.datetime.others", true);
// Enable Gamepad
user_pref("dom.gamepad.enabled", true);
user_pref("dom.gamepad.non_standard_events.enabled", true);
// Enable form autofill feature testing.
user_pref("extensions.formautofill.available", "on");
// Enable CSS clip-path `path()` for testing
user_pref("layout.css.clip-path-path.enabled", true);
// Enable visualviewport for testing
user_pref("dom.visualviewport.enabled", true);
