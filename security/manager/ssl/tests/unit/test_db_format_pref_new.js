// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests that when PSM initializes, we create the sqlite-backed certificate and
// key databases.

function run_test() {
  let profileDir = do_get_profile();
  let certificateDBFile = profileDir.clone();
  certificateDBFile.append("cert9.db");
  ok(!certificateDBFile.exists(), "cert9.db should not exist beforehand");
  let keyDBFile = profileDir.clone();
  keyDBFile.append("key4.db");
  ok(!keyDBFile.exists(), "key4.db should not exist beforehand");
  // This should start PSM.
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  ok(certificateDBFile.exists(), "cert9.db should exist in the profile");
  ok(keyDBFile.exists(), "key4.db should exist in the profile");
}
