/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we handle a locked database when there are extension changes
// in progress

Components.utils.import("resource://gre/modules/osfile.jsm");

// Will be left alone
var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be enabled
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be disabled
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be uninstalled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};


// Will be updated
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function() {

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);

  // Make it look like add-on 5 was installed some time in the past so the update is
  // detected
  let path = getFileForAddon(profileDir, addon5.id).path;
  await promiseSetExtensionModifiedTime(path, Date.now() - (60000));

  // Startup the profile and setup the initial state
  startupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  let a1, a2, a3, a4, a5, a6;

  [a2] = await promiseAddonsByIDs(["addon2@tests.mozilla.org"]);
  a2.userDisabled = true;

  restartManager();

  [a1, a2, a3, a4, a5] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon5@tests.mozilla.org"]);

  a2.userDisabled = false;
  a3.userDisabled = true;
  a4.uninstall();

  await promiseInstallAllFiles([do_get_addon("test_locked2_5"),
                                do_get_addon("test_locked2_6")]);
  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(!a2.isActive);
  Assert.ok(!a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_ENABLE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(a3.isActive);
  Assert.ok(a3.userDisabled);
  Assert.ok(!a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_DISABLE);
  Assert.ok(isExtensionInAddonsList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(a4.isActive);
  Assert.ok(!a4.userDisabled);
  Assert.ok(!a4.appDisabled);
  Assert.equal(a4.pendingOperations, AddonManager.PENDING_UNINSTALL);
  Assert.ok(isExtensionInAddonsList(profileDir, a4.id));

  Assert.notEqual(a5, null);
  Assert.equal(a5.version, "1.0");
  Assert.ok(a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(!a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_UPGRADE);
  Assert.ok(isExtensionInAddonsList(profileDir, a5.id));

  // Open another handle on the JSON DB with as much Unix and Windows locking
  // as we can to simulate some other process interfering with it
  shutdownManager();
  info("Locking " + gExtensionsJSON.path);
  let options = {
    winShare: 0
  };
  if (OS.Constants.libc.O_EXLOCK)
    options.unixFlags = OS.Constants.libc.O_EXLOCK;

  let file = await OS.File.open(gExtensionsJSON.path, {read: true, write: true, existing: true}, options);

  let filePermissions = gExtensionsJSON.permissions;
  if (!OS.Constants.Win) {
    gExtensionsJSON.permissions = 0;
  }
  await promiseStartupManager(false);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  [a1, a2, a3, a4, a5, a6] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon5@tests.mozilla.org",
                              "addon6@tests.mozilla.org"]);

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(a2.isActive);
  Assert.ok(!a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(!a3.isActive);
  Assert.ok(a3.userDisabled);
  Assert.ok(!a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));

  Assert.equal(a4, null);

  Assert.notEqual(a5, null);
  Assert.equal(a5.version, "2.0");
  Assert.ok(a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(!a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a6.id));

  // After allowing access to the original DB things should still be
  // back how they were before the lock
  let shutdownError;
  try {
    shutdownManager();
  } catch (e) {
    shutdownError = e;
  }
  await file.close();
  gExtensionsJSON.permissions = filePermissions;
  await promiseStartupManager();

  // On Unix, we can save the DB even when the original file wasn't
  // readable, so our changes were saved. On Windows,
  // these things happened when we had no access to the database so
  // they are seen as external changes when we get the database back
  if (shutdownError) {
    info("Previous XPI save failed");
    check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED,
        ["addon6@tests.mozilla.org"]);
    check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED,
        ["addon4@tests.mozilla.org"]);
  } else {
    info("Previous XPI save succeeded");
    check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
    check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  }

  [a1, a2, a3, a4, a5, a6] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon5@tests.mozilla.org",
                              "addon6@tests.mozilla.org"]);

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(a2.isActive);
  Assert.ok(!a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(!a3.isActive);
  Assert.ok(a3.userDisabled);
  Assert.ok(!a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));

  Assert.equal(a4, null);

  Assert.notEqual(a5, null);
  Assert.equal(a5.version, "2.0");
  Assert.ok(a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(!a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a6.id));
});

