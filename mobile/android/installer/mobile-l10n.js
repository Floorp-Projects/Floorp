/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This pref is in its own file for complex reasons. See bug 1428099 for
// details. Do not add other prefs to this file.

// Inherit locale from the OS, used for multi-locale builds
pref("intl.locale.requested", "");
