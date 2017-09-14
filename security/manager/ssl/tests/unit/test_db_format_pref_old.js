// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests that if "security.use_sqldb" is set to false when PSM initializes,
// we create the system-default certificate and key databases, which currently
// use the old BerkeleyDB format. This will change in bug 1377940.

function run_test() {
  let profileDir = do_get_profile();
  Services.prefs.setBoolPref("security.use_sqldb", false);
  let certificateDBFile = profileDir.clone();
  certificateDBFile.append("cert8.db");
  ok(!certificateDBFile.exists(), "cert8.db should not exist beforehand");
  let keyDBFile = profileDir.clone();
  keyDBFile.append("key3.db");
  ok(!keyDBFile.exists(), "key3.db should not exist beforehand");
  // This should start PSM.
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  ok(certificateDBFile.exists(), "cert8.db should exist in the profile");
  ok(keyDBFile.exists(), "key3.db should exist in the profile");
}
