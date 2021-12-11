/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This test may cause intermittents if run at exactly midnight.

"use strict";

const PREF_FPI = "privacy.firstparty.isolate";
const PREF_COOKIE_BEHAVIOR = "network.cookie.cookieBehavior";
const PREF_COOKIE_BEHAVIOR_PBMODE = "network.cookie.cookieBehavior.pbmode";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_FPI);
  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR);
  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR_PBMODE);
});

add_task(function test_FPI_off() {
  Services.prefs.setBoolPref(PREF_FPI, false);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR), i);
    equal(Services.cookies.getCookieBehavior(false), i);
  }

  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR_PBMODE, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR_PBMODE), i);
    equal(Services.cookies.getCookieBehavior(true), i);
  }
});

add_task(function test_FPI_on() {
  Services.prefs.setBoolPref(PREF_FPI, true);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR), i);
    equal(
      Services.cookies.getCookieBehavior(false),
      i == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
        ? Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
        : i
    );
  }

  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR);

  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR_PBMODE, i);
    equal(Services.prefs.getIntPref(PREF_COOKIE_BEHAVIOR_PBMODE), i);
    equal(
      Services.cookies.getCookieBehavior(true),
      i == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
        ? Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
        : i
    );
  }

  Services.prefs.clearUserPref(PREF_FPI);
});

add_task(function test_private_cookieBehavior_mirroring() {
  // Test that the private cookieBehavior getter will return the regular pref if
  // the regular pref has a user value and the private pref has a default value.
  Services.prefs.clearUserPref(PREF_COOKIE_BEHAVIOR_PBMODE);
  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, i);
    if (!Services.prefs.prefHasUserValue(PREF_COOKIE_BEHAVIOR)) {
      continue;
    }

    equal(Services.cookies.getCookieBehavior(true), i);
  }

  // Test that the private cookieBehavior getter will always return the private
  // pref if the private cookieBehavior has a user value.
  for (let i = 0; i <= Ci.nsICookieService.BEHAVIOR_LAST; ++i) {
    Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR_PBMODE, i);
    if (!Services.prefs.prefHasUserValue(PREF_COOKIE_BEHAVIOR_PBMODE)) {
      continue;
    }

    for (let j = 0; j <= Ci.nsICookieService.BEHAVIOR_LAST; ++j) {
      Services.prefs.setIntPref(PREF_COOKIE_BEHAVIOR, j);

      equal(Services.cookies.getCookieBehavior(true), i);
    }
  }
});
