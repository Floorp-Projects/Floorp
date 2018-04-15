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

const globalDir = Services.dirsvc.get("XREAddonAppDir", Ci.nsIFile);
globalDir.append("extensions");

var gGlobalExisted = globalDir.exists();
var gInstallTime = Date.now();

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Will be compatible in the first version and incompatible in subsequent versions
  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
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
    bootstrap: true,
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
    bootstrap: true,
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
    bootstrap: true,
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
  } else {
    globalDir.append(do_get_expected_addon_name("addon4@tests.mozilla.org"));
    globalDir.remove(true);
  }
  executeSoon(do_test_finished);
}

// Test that the test extensions are all installed
async function run_test_1() {
  await promiseStartupManager();

  let [a1, a2, a3, a4] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                            "addon2@tests.mozilla.org",
                                                            "addon3@tests.mozilla.org",
                                                            "addon4@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(isExtensionInBootstrappedList(globalDir, a4.id));
  Assert.equal(a4.version, "1.0");

  executeSoon(run_test_2);
}

// Test that upgrading the application doesn't disable now incompatible add-ons
async function run_test_2() {
  // Upgrade the extension
  var dest = writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "2.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "2",
      maxVersion: "2"
    }],
    name: "Test Addon 4",
  }, globalDir);
  setExtensionModifiedTime(dest, gInstallTime);

  await promiseRestartManager("2");
  let [a1, a2, a3, a4] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                            "addon2@tests.mozilla.org",
                                                            "addon3@tests.mozilla.org",
                                                            "addon4@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(isExtensionInBootstrappedList(globalDir, a4.id));
  Assert.equal(a4.version, "2.0");

  executeSoon(run_test_3);
}

// Test that nothing changes when only the build ID changes.
async function run_test_3() {
  // Upgrade the extension
  var dest = writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "3.0",
    bootstrap: true,
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

  let [a1, a2, a3, a4] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                            "addon2@tests.mozilla.org",
                                                            "addon3@tests.mozilla.org",
                                                            "addon4@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(isExtensionInBootstrappedList(globalDir, a4.id));
  Assert.equal(a4.version, "2.0");

  shutdownManager();

  end_test();
}
