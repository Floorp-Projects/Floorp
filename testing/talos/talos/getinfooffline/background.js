/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We've run once, and hopefully bypassed any network connections that add-ons
// might have tried to make after first install. Now let's uninstall ourselves
// so that subsequent starts run in online mode.

/* globals browser */
browser.management.uninstallSelf({});
