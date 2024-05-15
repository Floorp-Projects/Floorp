/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preferences file used by the raptor harness exclusively on android
/* globals user_pref */

// disk cache smart size is enabled in shipped apps
user_pref("browser.cache.disk.smart_size.enabled", true);

// Raptor's manifest.json contains the "geckoProfiler" permission, which is
// not supported on Android. Ignore the warning about the unknown permission.
user_pref("extensions.webextensions.warnings-as-errors", false);
// disable telemetry bug 1639148
user_pref("toolkit.telemetry.server", "");
user_pref("telemetry.fog.test.localhost_port", -1);
