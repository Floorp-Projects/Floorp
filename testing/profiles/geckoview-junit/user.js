/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Base preferences file used by the mochitest
/* globals user_pref */
/* eslint quotes: 0 */

// Always run in e10s
user_pref("browser.tabs.remote.autostart", true);
