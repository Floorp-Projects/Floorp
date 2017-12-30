// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests that if the profile path contains a non-ASCII character,
// we create the old BerkeleyDB format certificate and key databases
// even if "security.use_sqldb" is set to true when PSM initializes.

function run_test() {
  // Append a single quote and non-ASCII characters to the profile path.
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsIFile);
  file.initWithPath(profd);
  file.append("'÷1");
  env.set("XPCSHELL_TEST_PROFILE_DIR", file.path);

  let profileDir = do_get_profile();

  // Restore the original value.
  env.set("XPCSHELL_TEST_PROFILE_DIR", profd);

  Services.prefs.setBoolPref("security.use_sqldb", true);

  let cert8DBFile = profileDir.clone();
  cert8DBFile.append("cert8.db");
  ok(!cert8DBFile.exists(), "cert8.db should not exist beforehand");

  let cert9DBFile = profileDir.clone();
  cert9DBFile.append("cert9.db");
  ok(!cert9DBFile.exists(), "cert9.db should not exist beforehand");

  let key3DBFile = profileDir.clone();
  key3DBFile.append("key3.db");
  ok(!key3DBFile.exists(), "key3.db should not exist beforehand");

  let key4DBFile = profileDir.clone();
  key4DBFile.append("key4.db");
  ok(!key4DBFile.exists(), "key4.db should not exist beforehand");

  // This should start PSM.
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

  const isWindows = mozinfo.os == "win";
  equal(cert8DBFile.exists(), isWindows, "cert8.db should exist in the profile on Windows");
  equal(cert9DBFile.exists(), !isWindows, "cert9.db should not exist in the profile on Windows");
  equal(key3DBFile.exists(), isWindows, "key3.db should exist in the profile on Windows");
  equal(key4DBFile.exists(), !isWindows, "key4.db should not exist in the profile on Windows");
}
