/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests that cert_storage properly handles its preference values.

function run_test() {
  if (!AppConstants.MOZ_NEW_CERT_STORAGE) {
    return;
  }

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(Ci.nsICertStorage);
  // Since none of our prefs start with values, looking them up will fail. cert_storage should use
  // safe fallbacks.
  ok(!certStorage.isBlocklistFresh(), "checking blocklist freshness shouldn't crash");

  // If we set nonsensical values, cert_storage should still use safe fallbacks.
  Services.prefs.setIntPref("services.blocklist.onecrl.checked", -2);
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds", -7);
  ok(!certStorage.isBlocklistFresh(), "checking blocklist freshness still shouldn't crash");

  // Clearing prefs shouldn't cause failures.
  Services.prefs.clearUserPref("services.blocklist.onecrl.checked");
  ok(!certStorage.isBlocklistFresh(), "checking blocklist freshness again shouldn't crash");
}
