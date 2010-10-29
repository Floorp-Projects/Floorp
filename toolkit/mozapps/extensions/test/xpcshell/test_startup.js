/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies startup detection of added/removed/changed items and install
// location priorities

// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER +
                          AddonManager.SCOPE_SYSTEM);

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }, {                 // Repeated target application entries should be ignored
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "2.0",
  name: "Test 2",
  targetApplications: [{  // Bad target application entries should be ignored
    minVersion: "3",
    maxVersion: "4"
  }, {
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "3.0",
  name: "Test 3",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1.9.2",
    maxVersion: "1.9.2.*"
  }]
};

// Should be ignored because it has no ID
var addon4 = {
  version: "4.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Should be ignored because it has no version
var addon5 = {
  id: "addon5@tests.mozilla.org",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const globalDir = gProfD.clone();
globalDir.append("extensions2");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);
const userDir = gProfD.clone();
userDir.append("extensions3");
userDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userDir.parent);
const profileDir = gProfD.clone();
profileDir.append("extensions");

var gFastLoadService = AM_Cc["@mozilla.org/fast-load-service;1"].
                       getService(AM_Ci.nsIFastLoadService);
var gFastLoadFile = null;

// Set up the profile
function run_test() {
  do_test_pending();
  startupManager();

  gFastLoadFile = gFastLoadService.newFastLoadFile("XUL");
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_eq(a1, null);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);
    do_check_eq(a2, null);
    do_check_not_in_crash_annotation(addon2.id, addon2.version);
    do_check_eq(a3, null);
    do_check_not_in_crash_annotation(addon3.id, addon3.version);
    do_check_eq(a4, null);
    do_check_eq(a5, null);

    run_test_1();
  });
}

function end_test() {
  do_test_finished();
}

// Try to install all the items into the profile
function run_test_1() {
  writeInstallRDFForExtension(addon1, profileDir);
  var dest = writeInstallRDFForExtension(addon2, profileDir);
  // Attempt to make this look like it was added some time in the past so
  // the change in run_test_2 makes the last modified time change.
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);

  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.0");
    do_check_eq(a1.name, "Test 1");
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, addon1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_PROFILE);
    do_check_eq(a1.sourceURI, null);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.0");
    do_check_eq(a2.name, "Test 2");
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, addon2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_PROFILE);
    do_check_eq(a2.sourceURI, null);

    do_check_neq(a3, null);
    do_check_eq(a3.id, "addon3@tests.mozilla.org");
    do_check_eq(a3.version, "3.0");
    do_check_eq(a3.name, "Test 3");
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));
    do_check_true(hasFlag(a3.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a3.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon3.id, addon3.version);
    do_check_eq(a3.scope, AddonManager.SCOPE_PROFILE);
    do_check_eq(a3.sourceURI, null);

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));
    dest = profileDir.clone();
    dest.append(do_get_expected_addon_name("addon4@tests.mozilla.org"));
    do_check_false(dest.exists());

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));
    dest = profileDir.clone();
    dest.append(do_get_expected_addon_name("addon5@tests.mozilla.org"));
    do_check_false(dest.exists());

    AddonManager.getAddonsByTypes(["extension"], function(extensionAddons) {
      do_check_eq(extensionAddons.length, 3);

      run_test_2();
    });
  });
}

// Test that modified items are detected and items in other install locations
// are ignored
function run_test_2() {
  addon1.version = "1.1";
  writeInstallRDFForExtension(addon1, userDir);
  addon2.version="2.1";
  writeInstallRDFForExtension(addon2, profileDir);
  addon2.version="2.2";
  writeInstallRDFForExtension(addon2, globalDir);
  addon2.version="2.3";
  writeInstallRDFForExtension(addon2, userDir);
  var dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon3@tests.mozilla.org"));
  dest.remove(true);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.0");
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_false(isExtensionInAddonsList(userDir, a1.id));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_PROFILE);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.1");
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_PROFILE);

    do_check_eq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));
    do_check_not_in_crash_annotation(addon3.id, addon3.version);

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));

    run_test_3();
  });
}

// Check that removing items from the profile reveals their hidden versions.
function run_test_3() {
  var dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  dest.remove(true);
  dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon2@tests.mozilla.org"));
  dest.remove(true);
  writeInstallRDFForExtension(addon3, profileDir, "addon4@tests.mozilla.org");

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.1");
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));
    do_check_true(isExtensionInAddonsList(userDir, a1.id));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_USER);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.3");
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));
    do_check_true(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_USER);

    do_check_eq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));

    dest = profileDir.clone();
    dest.append(do_get_expected_addon_name("addon4@tests.mozilla.org"));
    do_check_false(dest.exists());

    run_test_4();
  });
}

// Test that disabling an install location works
function run_test_4() {
  Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_SYSTEM);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_eq(a1, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon1@tests.mozilla.org"));

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.2");
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_true(isExtensionInAddonsList(globalDir, a2.id));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_SYSTEM);

    run_test_5();
  });
}

// Switching disabled locations works
function run_test_5() {
  Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_USER);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.1");
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));
    do_check_true(isExtensionInAddonsList(userDir, a1.id));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_USER);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.3");
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));
    do_check_true(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_USER);

    run_test_6();
  });
}

// Resetting the pref makes everything visible again
function run_test_6() {
  Services.prefs.clearUserPref("extensions.enabledScopes");

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.1");
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));
    do_check_true(isExtensionInAddonsList(userDir, a1.id));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_USER);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.3");
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));
    do_check_true(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_USER);

    run_test_7();
  });
}

// Check that items in the profile hide the others again.
function run_test_7() {
  addon1.version = "1.2";
  writeInstallRDFForExtension(addon1, profileDir);
  var dest = userDir.clone();
  dest.append(do_get_expected_addon_name("addon2@tests.mozilla.org"));
  dest.remove(true);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.2");
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_false(isExtensionInAddonsList(userDir, a1.id));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_PROFILE);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.2");
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_true(isExtensionInAddonsList(globalDir, a2.id));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon2.id, a2.version);
    do_check_eq(a2.scope, AddonManager.SCOPE_SYSTEM);

    do_check_eq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));

    run_test_8();
  });
}

// Disabling all locations still leaves the profile working
function run_test_8() {
  Services.prefs.setIntPref("extensions.enabledScopes", 0);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.2");
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_false(isExtensionInAddonsList(userDir, a1.id));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(addon1.id, a1.version);
    do_check_eq(a1.scope, AddonManager.SCOPE_PROFILE);

    do_check_eq(a2, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon2@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon2@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon2@tests.mozilla.org"));

    run_test_9();
  });
}

// More hiding and revealing
function run_test_9() {
  Services.prefs.clearUserPref("extensions.enabledScopes", 0);

  var dest = userDir.clone();
  dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  dest.remove(true);
  dest = globalDir.clone();
  dest.append(do_get_expected_addon_name("addon2@tests.mozilla.org"));
  dest.remove(true);
  addon2.version = "2.4";
  writeInstallRDFForExtension(addon2, profileDir);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.2");
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_false(isExtensionInAddonsList(userDir, a1.id));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_eq(a1.scope, AddonManager.SCOPE_PROFILE);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.4");
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_eq(a2.scope, AddonManager.SCOPE_PROFILE);

    do_check_eq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));

    run_test_10();
  });
}

// Checks that a removal from one location and an addition in another location
// for the same item is handled
function run_test_10() {
  var dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  dest.remove(true);
  addon1.version = "1.3";
  writeInstallRDFForExtension(addon1, userDir);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_neq(a1, null);
    do_check_eq(a1.id, "addon1@tests.mozilla.org");
    do_check_eq(a1.version, "1.3");
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));
    do_check_true(isExtensionInAddonsList(userDir, a1.id));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_eq(a1.scope, AddonManager.SCOPE_USER);

    do_check_neq(a2, null);
    do_check_eq(a2.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.version, "2.4");
    do_check_true(isExtensionInAddonsList(profileDir, a2.id));
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_false(isExtensionInAddonsList(globalDir, a2.id));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_eq(a2.scope, AddonManager.SCOPE_PROFILE);

    do_check_eq(a3, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));

    do_check_eq(a4, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));

    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));

    run_test_11();
  });
}

// This should remove any remaining items
function run_test_11() {
  var dest = userDir.clone();
  dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  dest.remove(true);
  dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon2@tests.mozilla.org"));
  dest.remove(true);

  restartManager();
  do_check_false(gFastLoadFile.exists());
  gFastLoadFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    do_check_eq(a1, null);
    do_check_eq(a2, null);
    do_check_eq(a3, null);
    do_check_eq(a4, null);
    do_check_eq(a5, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(profileDir, "addon2@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(profileDir, "addon3@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(profileDir, "addon4@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(profileDir, "addon5@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon1@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon2@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon3@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon4@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(userDir, "addon5@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon1@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon2@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon3@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon4@tests.mozilla.org"));
    do_check_false(isExtensionInAddonsList(globalDir, "addon5@tests.mozilla.org"));
    do_check_not_in_crash_annotation(addon1.id, addon1.version);
    do_check_not_in_crash_annotation(addon2.id, addon2.version);

    end_test();
  });
}
