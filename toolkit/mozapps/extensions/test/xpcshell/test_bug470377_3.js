/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.2.3", "2");

  // inject the add-ons into the profile
  var dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_1@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o755);
  var source = do_get_file("data/test_bug470377/install_1.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_2@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o755);
  source = do_get_file("data/test_bug470377/install_2.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_3@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o755);
  source = do_get_file("data/test_bug470377/install_3.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_4@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o755);
  source = do_get_file("data/test_bug470377/install_4.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_5@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o755);
  source = do_get_file("data/test_bug470377/install_5.rdf");
  source.copyTo(dest, "install.rdf");

  startupManager();

  run_test_1();
}

function run_test_1() {
  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    Assert.notEqual(a1, null);
    Assert.ok(!a1.isActive);
    Assert.notEqual(a2, null);
    Assert.ok(a2.isActive);
    Assert.notEqual(a3, null);
    Assert.ok(a3.isActive);
    Assert.notEqual(a4, null);
    Assert.ok(a4.isActive);
    Assert.notEqual(a5, null);
    Assert.ok(a5.isActive);

    executeSoon(run_test_2);
  });
}

function run_test_2() {
  AddonManager.checkCompatibility = false;

  restartManager();

  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    Assert.notEqual(a1, null);
    Assert.ok(!a1.isActive);
    Assert.notEqual(a2, null);
    Assert.ok(a2.isActive);
    Assert.notEqual(a3, null);
    Assert.ok(a3.isActive);
    Assert.notEqual(a4, null);
    Assert.ok(a4.isActive);
    Assert.notEqual(a5, null);
    Assert.ok(a5.isActive);

    executeSoon(do_test_finished);
  });
}
