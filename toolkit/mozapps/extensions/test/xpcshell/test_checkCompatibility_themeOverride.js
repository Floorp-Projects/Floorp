/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the (temporary)
// extensions.checkCompatibility.temporaryThemeOverride_minAppVersion
// preference works.

var ADDONS = [{
  id: "addon1@tests.mozilla.org",
  type: 4,
  internalName: "theme1/1.0",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1.0",
    maxVersion: "1.0"
  }]
}, {
  id: "addon2@tests.mozilla.org",
  type: 4,
  internalName: "theme2/1.0",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2.0",
    maxVersion: "2.0"
  }]
}];

const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3.0", "1");

  for (let a of ADDONS) {
    writeInstallRDFForExtension(a, profileDir);
  }

  startupManager();

  run_test_1();
}

function run_test_1() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"],
                               function([a1, a2]) {

    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_false(a1.isCompatible);
    do_check_true(a1.appDisabled);

    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_false(a2.isCompatible);
    do_check_true(a1.appDisabled);

    do_execute_soon(run_test_2);
  });
}

function run_test_2() {
  Services.prefs.setCharPref("extensions.checkCompatibility.temporaryThemeOverride_minAppVersion", "2.0");
  if (isNightlyChannel())
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", false);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.3.0", false);
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"],
                               function([a1, a2]) {

    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_false(a1.isCompatible);
    do_check_true(a1.appDisabled);

    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_false(a2.isCompatible);
    do_check_false(a2.appDisabled);

    do_execute_soon(do_test_finished);
  });
}
