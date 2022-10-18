/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Preferences file for valgrind.
/* globals user_pref */
// TODO: Bug 1795750 - Re-enable this pref when we have a new version of the
// Quitter XPI with a simpler version format.
user_pref("extensions.webextensions.warnings-as-errors", false);
