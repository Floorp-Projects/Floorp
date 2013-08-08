/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.1a4", "2");

  // inject the add-ons into the profile
  var profileDir = gProfD.clone();
  profileDir.append("extensions");
  var dest = profileDir.clone();
  dest.append("bug470377_1@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  var source = do_get_file("data/test_bug470377/install_1.rdf");
  source.copyTo(dest, "install.rdf");
  dest = profileDir.clone();
  dest.append("bug470377_2@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_2.rdf");
  source.copyTo(dest, "install.rdf");
  dest = profileDir.clone();
  dest.append("bug470377_3@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_3.rdf");
  source.copyTo(dest, "install.rdf");
  dest = profileDir.clone();
  dest.append("bug470377_4@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_4.rdf");
  source.copyTo(dest, "install.rdf");
  dest = profileDir.clone();
  dest.append("bug470377_5@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_5.rdf");
  source.copyTo(dest, "install.rdf");

  run_test_1();
}

function run_test_1() {
  startupManager();
  AddonManager.checkCompatibility = false;
  restartManager();

  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_neq(a2, null);
    do_check_true(a2.isActive);
    do_check_neq(a3, null);
    do_check_true(a3.isActive);
    do_check_neq(a4, null);
    do_check_true(a4.isActive);
    do_check_neq(a5, null);
    do_check_true(a5.isActive);

    do_execute_soon(run_test_2);
  });
}

function run_test_2() {
  AddonManager.checkCompatibility = true;

  restartManager();

  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_neq(a3, null);
    do_check_false(a3.isActive);
    do_check_neq(a4, null);
    do_check_true(a4.isActive);
    do_check_neq(a5, null);
    do_check_true(a5.isActive);

    do_execute_soon(do_test_finished);
  });
}
