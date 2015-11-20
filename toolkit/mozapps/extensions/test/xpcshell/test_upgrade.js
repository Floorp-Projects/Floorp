/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that app upgrades produce the expected behaviours,
// with strict compatibility checking disabled.

Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

// Enable loading extensions from the application scope
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE +
                          AddonManager.SCOPE_APPLICATION);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const globalDir = Services.dirsvc.get("XREAddonAppDir", AM_Ci.nsIFile);
globalDir.append("extensions");

var gGlobalExisted = globalDir.exists();
var gInstallTime = Date.now();

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Will be compatible in the first version and incompatible in subsequent versions
  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
    targetPlatforms: [
      "XPCShell",
      "WINNT_x86",
    ]
  }, profileDir);

  // Works in all tested versions
  writeInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }],
    name: "Test Addon 2",
    targetPlatforms: [
      "XPCShell_noarch-spidermonkey"
    ]
  }, profileDir);

  // Will be disabled in the first version and enabled in the second.
  writeInstallRDFForExtension({
    id: "addon3@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "2",
      maxVersion: "2"
    }],
    name: "Test Addon 3",
  }, profileDir);

  // Will be compatible in both versions but will change version in between
  var dest = writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 4",
  }, globalDir);
  setExtensionModifiedTime(dest, gInstallTime);

  do_test_pending();

  run_test_1();
}

function end_test() {
  if (!gGlobalExisted) {
    globalDir.remove(true);
  }
  else {
    globalDir.append(do_get_expected_addon_name("addon4@tests.mozilla.org"));
    globalDir.remove(true);
  }
  do_execute_soon(do_test_finished);
}

// Test that the test extensions are all installed
function run_test_1() {
  startupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    do_check_neq(a1, null);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, a3.id));

    do_check_neq(a4, null);
    do_check_true(isExtensionInAddonsList(globalDir, a4.id));
    do_check_eq(a4.version, "1.0");

    do_execute_soon(run_test_2);
  });
}

// Test that upgrading the application doesn't disable now incompatible add-ons
function run_test_2() {
  // Upgrade the extension
  var dest = writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "2.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "2",
      maxVersion: "2"
    }],
    name: "Test Addon 4",
  }, globalDir);
  setExtensionModifiedTime(dest, gInstallTime);

  restartManager("2");
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    do_check_neq(a1, null);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));

    do_check_neq(a4, null);
    do_check_true(isExtensionInAddonsList(globalDir, a4.id));
    do_check_eq(a4.version, "2.0");

    do_execute_soon(run_test_3);
  });
}

// Test that nothing changes when only the build ID changes.
function run_test_3() {
  // Upgrade the extension
  var dest = writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "3.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "3",
      maxVersion: "3"
    }],
    name: "Test Addon 4",
  }, globalDir);
  setExtensionModifiedTime(dest, gInstallTime);

  // Simulates a simple Build ID change, the platform deletes extensions.ini
  // whenever the application is changed.
  gExtensionsINI.remove(true);
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    do_check_neq(a1, null);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    do_check_neq(a2, null);
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));

    do_check_neq(a3, null);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));

    do_check_neq(a4, null);
    do_check_true(isExtensionInAddonsList(globalDir, a4.id));
    do_check_eq(a4.version, "2.0");

    end_test();
  });
}
