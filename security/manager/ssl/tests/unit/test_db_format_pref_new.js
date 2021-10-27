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
  let certificateDBName = "cert9.db";
  certificateDBFile.append(certificateDBName);
  ok(
    !certificateDBFile.exists(),
    `${certificateDBName} should not exist beforehand`
  );
  let keyDBFile = profileDir.clone();
  let keyDBName = "key4.db";
  keyDBFile.append(keyDBName);
  ok(!keyDBFile.exists(), `${keyDBName} should not exist beforehand`);
  // This should start PSM.
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  ok(
    certificateDBFile.exists(),
    `${certificateDBName} should exist in the profile`
  );
  ok(keyDBFile.exists(), `${keyDBName} should exist in the profile`);
}
