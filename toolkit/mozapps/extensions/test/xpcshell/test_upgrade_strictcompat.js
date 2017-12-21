/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that app upgrades produce the expected behaviours,
// with strict compatibility checking enabled.

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

  // Will be enabled in the first version and disabled in subsequent versions
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

  // Will be enabled in both versions but will change version in between
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

  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

  run_test_1();
}

function end_test() {
  if (!gGlobalExisted) {
    globalDir.remove(true);
  } else {
    globalDir.append(do_get_expected_addon_name("addon4@tests.mozilla.org"));
    globalDir.remove(true);
  }

  Services.prefs.clearUserPref(PREF_EM_STRICT_COMPATIBILITY);

  do_execute_soon(do_test_finished);
}

// Test that the test extensions are all installed
async function run_test_1() {
  await promiseStartupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    Assert.notEqual(a1, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

    Assert.notEqual(a2, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

    Assert.notEqual(a3, null);
    Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));

    Assert.notEqual(a4, null);
    Assert.ok(isExtensionInAddonsList(globalDir, a4.id));
    Assert.equal(a4.version, "1.0");

    do_execute_soon(run_test_2);
  });
}

// Test that upgrading the application disables now incompatible add-ons
async function run_test_2() {
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

  await promiseRestartManager("2");

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    Assert.notEqual(a1, null);
    Assert.ok(!isExtensionInAddonsList(profileDir, a1.id));

    Assert.notEqual(a2, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

    Assert.notEqual(a3, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a3.id));

    Assert.notEqual(a4, null);
    Assert.ok(isExtensionInAddonsList(globalDir, a4.id));
    Assert.equal(a4.version, "2.0");

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
  gAddonStartup.remove(true);
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    Assert.notEqual(a1, null);
    Assert.ok(!isExtensionInAddonsList(profileDir, a1.id));

    Assert.notEqual(a2, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

    Assert.notEqual(a3, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a3.id));

    Assert.notEqual(a4, null);
    Assert.ok(isExtensionInAddonsList(globalDir, a4.id));
    Assert.equal(a4.version, "2.0");

    end_test();
  });
}
