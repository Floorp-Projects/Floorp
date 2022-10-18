/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preferences file for profileserver.
/* globals user_pref */
// Turn off budget throttling for the profile server
user_pref("dom.timeout.enable_budget_timer_throttling", false);
// TODO: Bug 1795750 - Re-enable this pref when we have a new version of the
// Quitter XPI with a simpler version format.
user_pref("extensions.webextensions.warnings-as-errors", false);
