/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that app upgrades produce the expected behaviours.

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Will be enabled in the first version and disabled in subsequent versions
  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, dest);

  // Works in all tested versions
  dest = profileDir.clone();
  dest.append("addon2@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }],
    name: "Test Addon 2",
  }, dest);

  // Will be disabled in the first version and enabled in the second.
  dest = profileDir.clone();
  dest.append("addon3@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon3@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "2",
      maxVersion: "2"
    }],
    name: "Test Addon 3",
  }, dest);

  do_test_pending();

  run_test_1();
}

// Test that the test extensions are all installed
function run_test_1() {
  startupManager(1);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org"], function([a1, a2, a3]) {

    do_check_neq(a1, null);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, a3.id));

    run_test_2();
  });
}

// Test that upgrading the application disables now incompatible add-ons
function run_test_2() {
  restartManager(1, "2");
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org"], function([a1, a2, a3]) {

    do_check_neq(a1, null);
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));

    run_test_3();
  });
}

// Test that nothing changes when only the build ID changes.
function run_test_3() {
  // Simulates a simple Build ID change, the platform deletes extensions.ini
  // whenever the application is changed.
  var file = gProfD.clone();
  file.append("extensions.ini");
  file.remove(true);
  restartManager(1);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org"], function([a1, a2, a3]) {

    do_check_neq(a1, null);
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));

    do_test_finished();
  });
}
