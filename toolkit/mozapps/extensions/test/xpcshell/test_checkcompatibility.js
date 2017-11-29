/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the extensions.checkCompatibility.* preferences work.

var ADDONS = [{
  // Cannot be enabled as it has no target app info for the applciation
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "unknown@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
}, {
  // Always appears incompatible but can be enabled if compatibility checking is
  // disabled
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
}, {
  // Always appears incompatible but can be enabled if compatibility checking is
  // disabled
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
}, { // Always compatible and enabled
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
}, { // Always compatible and enabled
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
}];

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gIsNightly = false;

function run_test() {
  do_test_pending("checkcompatibility.js");
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.2.3", "2");

  ADDONS.forEach(function(a) {
    writeInstallRDFForExtension(a, profileDir);
  });

  gIsNightly = isNightlyChannel();

  startupManager();

  run_test_1();
}

/**
 * Checks that the add-ons are enabled as expected.
 * @param   overridden
 *          A boolean indicating that compatibility checking is overridden
 * @param   a1
 *          The Addon for addon1@tests.mozilla.org
 * @param   a2
 *          The Addon for addon2@tests.mozilla.org
 * @param   a3
 *          The Addon for addon3@tests.mozilla.org
 * @param   a4
 *          The Addon for addon4@tests.mozilla.org
 * @param   a5
 *          The Addon for addon5@tests.mozilla.org
 */
function check_state(overridden, a1, a2, a3, a4, a5) {
  do_check_neq(a1, null);
  do_check_false(a1.isActive);
  do_check_false(a1.isCompatible);

  do_check_neq(a2, null);
  if (overridden)
    do_check_true(a2.isActive);
  else
    do_check_false(a2.isActive);
  do_check_false(a2.isCompatible);

  do_check_neq(a3, null);
  if (overridden)
    do_check_true(a3.isActive);
  else
    do_check_false(a3.isActive);
  do_check_false(a3.isCompatible);

  do_check_neq(a4, null);
  do_check_true(a4.isActive);
  do_check_true(a4.isCompatible);

  do_check_neq(a5, null);
  do_check_true(a5.isActive);
  do_check_true(a5.isCompatible);
}

// Tests that with compatibility checking enabled we see the incompatible
// add-ons disabled
function run_test_1() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5]) {
                                check_state(false, a1, a2, a3, a4, a5);

                                do_execute_soon(run_test_2);
                              });
}

// Tests that with compatibility checking disabled we see the incompatible
// add-ons enabled
function run_test_2() {
  if (gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", false);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.2", false);
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5]) {
                                check_state(true, a1, a2, a3, a4, a5);

                                do_execute_soon(run_test_3);
                              });
}

// Tests that with compatibility checking disabled we see the incompatible
// add-ons enabled.
function run_test_3() {
  if (!gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.1a", false);
  restartManager("2.1a4");

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5]) {
                                check_state(true, a1, a2, a3, a4, a5);

                                do_execute_soon(run_test_4);
                              });
}

// Tests that with compatibility checking enabled we see the incompatible
// add-ons disabled.
function run_test_4() {
  if (gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", true);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.1a", true);
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5]) {
                                check_state(false, a1, a2, a3, a4, a5);

                                do_execute_soon(do_test_finished);
                              });
}
