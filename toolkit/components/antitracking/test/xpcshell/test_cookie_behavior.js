/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This test may cause intermittents if run at exactly midnight.

"use strict";

const PREF_FPI = "privacy.firstparty.isolate";
const PREF_COOKIE_BEHAVIOR = "network.cookie.cookieBehavior";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_FPI);
  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR);
});

add_task(function test_FPI_off() {
  Services.prefs.setBoolPref(PREF_FPI, false);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR), i);
    equal(Services.cookies.cookieBehavior, i);
  }
});

add_task(function test_FPI_on() {
  Services.prefs.setBoolPref(PREF_FPI, true);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR), i);
    equal(
      Services.cookies.cookieBehavior,
      i == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
        ? Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
        : i
    );
  }
});
