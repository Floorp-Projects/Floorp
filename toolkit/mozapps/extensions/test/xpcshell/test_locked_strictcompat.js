/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a corrupt database

Components.utils.import("resource://testing-common/httpd.js");
Components.utils.import("resource://gre/modules/osfile.jsm");

var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;
mapFile("/data/test_corrupt.rdf", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

// Will be enabled
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

// Will be disabled
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

// Will get a compatibility update and be enabled
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  updateURL: "http://localhost:" + gPort + "/data/test_corrupt.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Will get a compatibility update and be disabled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  updateURL: "http://localhost:" + gPort + "/data/test_corrupt.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Stays incompatible
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Enabled bootstrapped
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test 6",
  bootstrap: "true",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Disabled bootstrapped
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test 7",
  bootstrap: "true",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// The default theme
var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "1.0",
  name: "Theme 1",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// The selected theme
var theme2 = {
  manifest: {
    manifest_version: 2,
    name: "Theme 2",
    version: "1.0",
    theme: { images: { headerURL: "example.png" } },
    applications: {
      gecko: {
        id: "theme2@tests.mozilla.org",
      },
    },
  },
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function init() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, profileDir);
  writeInstallRDFForExtension(theme1, profileDir);
  let theme2XPI = createTempWebExtensionFile(theme2);
  await AddonTestUtils.manuallyInstall(theme2XPI);

  // Startup the profile and setup the initial state
  await promiseStartupManager();

  // New profile so new add-ons are ignored
  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);

  let [a2, a3, a4, a7, t2] =
    await promiseAddonsByIDs(["addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon7@tests.mozilla.org",
                              "theme2@tests.mozilla.org"]);

  // Set up the initial state
  await new Promise(resolve => {

    a2.userDisabled = true;
    a4.userDisabled = true;
    a7.userDisabled = true;
    t2.userDisabled = false;
    a3.findUpdates({
      onUpdateFinished() {
        a4.findUpdates({
          onUpdateFinished() {
            resolve();
          }
        }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
      }
    }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
  });
});

add_task(async function run_test_1() {
  let a1, a2, a3, a4, a5, a6, a7, t1, t2;

  restartManager();
  [a1, a2, a3, a4, a5, a6, a7, t1, t2] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                             "addon2@tests.mozilla.org",
                             "addon3@tests.mozilla.org",
                             "addon4@tests.mozilla.org",
                             "addon5@tests.mozilla.org",
                             "addon6@tests.mozilla.org",
                             "addon7@tests.mozilla.org",
                             "theme1@tests.mozilla.org",
                             "theme2@tests.mozilla.org"]);

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(!a2.isActive);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(a3.isActive);
  Assert.ok(!a3.userDisabled);
  Assert.ok(!a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(!a4.isActive);
  Assert.ok(a4.userDisabled);
  Assert.ok(!a4.appDisabled);
  Assert.equal(a4.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));

  Assert.notEqual(a5, null);
  Assert.ok(!a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a7, null);
  Assert.ok(!a7.isActive);
  Assert.ok(a7.userDisabled);
  Assert.ok(!a7.appDisabled);
  Assert.equal(a7.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(t1, null);
  Assert.ok(!t1.isActive);
  Assert.ok(t1.userDisabled);
  Assert.ok(!t1.appDisabled);
  Assert.equal(t1.pendingOperations, AddonManager.PENDING_NONE);
  // Disabled due to bug 1394117
  // do_check_false(isThemeInAddonsList(profileDir, t1.id));

  Assert.notEqual(t2, null);
  Assert.ok(t2.isActive);
  Assert.ok(!t2.userDisabled);
  Assert.ok(!t2.appDisabled);
  Assert.equal(t2.pendingOperations, AddonManager.PENDING_NONE);
  // Disabled due to bug 1394117
  // do_check_true(isThemeInAddonsList(profileDir, t2.id));

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

  // Shouldn't have seen any startup changes
  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);

  // Accessing the add-ons should open and recover the database
  [a1, a2, a3, a4, a5, a6, a7, t1, t2] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon5@tests.mozilla.org",
                              "addon6@tests.mozilla.org",
                              "addon7@tests.mozilla.org",
                              "theme1@tests.mozilla.org",
                              "theme2@tests.mozilla.org"]);

  // Should be correctly recovered
  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  // Should be correctly recovered
  Assert.notEqual(a2, null);
  Assert.ok(!a2.isActive);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a2.id));

  // The compatibility update won't be recovered but it should still be
  // active for this session
  Assert.notEqual(a3, null);
  Assert.ok(a3.isActive);
  Assert.ok(!a3.userDisabled);
  Assert.ok(a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_DISABLE);
  Assert.ok(isExtensionInAddonsList(profileDir, a3.id));

  // The compatibility update won't be recovered and it will not have been
  // able to tell that it was previously userDisabled
  Assert.notEqual(a4, null);
  Assert.ok(!a4.isActive);
  Assert.ok(!a4.userDisabled);
  Assert.ok(a4.appDisabled);
  Assert.equal(a4.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));

  Assert.notEqual(a5, null);
  Assert.ok(!a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a7, null);
  Assert.ok(!a7.isActive);
  Assert.ok(a7.userDisabled);
  Assert.ok(!a7.appDisabled);
  Assert.equal(a7.pendingOperations, AddonManager.PENDING_NONE);

  // Should be correctly recovered
  Assert.notEqual(t1, null);
  // Disabled due to bug 1394117
  // do_check_false(t1.isActive);
  // do_check_true(t1.userDisabled);
  Assert.ok(!t1.appDisabled);
  Assert.equal(t1.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_false(isThemeInAddonsList(profileDir, t1.id));

  // Should be correctly recovered
  Assert.notEqual(t2, null);
  Assert.ok(t2.isActive);
  // Disabled due to bug 1394117
  // do_check_false(t2.userDisabled);
  Assert.ok(!t2.appDisabled);
  // do_check_eq(t2.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_true(isThemeInAddonsList(profileDir, t2.id));

  // Restarting will actually apply changes to extensions.ini which will
  // then be put into the in-memory database when we next fail to load the
  // real thing
  try {
    shutdownManager();
  } catch (e) {
    // We're expecting an error here.
  }
  await promiseStartupManager(false);

  // Shouldn't have seen any startup changes
  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);

  [a1, a2, a3, a4, a5, a6, a7, t1, t2] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"]);

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(!a2.isActive);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(!a3.isActive);
  Assert.ok(!a3.userDisabled);
  Assert.ok(a3.appDisabled);
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));

  Assert.notEqual(a4, null);
  Assert.ok(!a4.isActive);
  Assert.ok(!a4.userDisabled);
  Assert.ok(a4.appDisabled);
  Assert.equal(a4.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));

  Assert.notEqual(a5, null);
  Assert.ok(!a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a7, null);
  Assert.ok(!a7.isActive);
  Assert.ok(a7.userDisabled);
  Assert.ok(!a7.appDisabled);
  Assert.equal(a7.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(t1, null);
  // Disabled due to bug 1394117
  // do_check_false(t1.isActive);
  //  do_check_true(t1.userDisabled);
  Assert.ok(!t1.appDisabled);
  Assert.equal(t1.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_false(isThemeInAddonsList(profileDir, t1.id));

  Assert.notEqual(t2, null);
  // Disabled due to bug 1394117
  // do_check_true(t2.isActive);
  // do_check_false(t2.userDisabled);
  Assert.ok(!t2.appDisabled);
  Assert.equal(t2.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_true(isThemeInAddonsList(profileDir, t2.id));

  // After allowing access to the original DB things should go back to as
  // back how they were before the lock
  let shutdownError;
  try {
    shutdownManager();
  } catch (e) {
    shutdownError = e;
  }
  info("Unlocking " + gExtensionsJSON.path);
  await file.close();
  gExtensionsJSON.permissions = filePermissions;
  await promiseStartupManager(false);

  // Shouldn't have seen any startup changes
  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);

  [a1, a2, a3, a4, a5, a6, a7, t1, t2] =
    await promiseAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org",
                              "addon3@tests.mozilla.org",
                              "addon4@tests.mozilla.org",
                              "addon5@tests.mozilla.org",
                              "addon6@tests.mozilla.org",
                              "addon7@tests.mozilla.org",
                              "theme1@tests.mozilla.org",
                              "theme2@tests.mozilla.org"]);

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(!a1.appDisabled);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(!a2.isActive);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.appDisabled);
  Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(!a3.userDisabled);
  // On Unix, we may be able to save our changes over the locked DB so we
  // remember that this extension was changed to disabled. On Windows we
  // couldn't replace the old DB so we read the older version of the DB
  // where the extension is enabled
  if (shutdownError) {
    info("XPI save failed");
    Assert.ok(a3.isActive);
    Assert.ok(!a3.appDisabled);
    Assert.ok(isExtensionInAddonsList(profileDir, a3.id));
  } else {
    info("XPI save succeeded");
    Assert.ok(!a3.isActive);
    Assert.ok(a3.appDisabled);
    Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));
  }
  Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a4, null);
  Assert.ok(!a4.isActive);
  // The reverse of the platform difference for a3 - Unix should
  // stay the same as the last iteration because the save succeeded,
  // Windows should still say userDisabled
  if (OS.Constants.Win) {
    Assert.ok(a4.userDisabled);
    Assert.ok(!a4.appDisabled);
  } else {
    Assert.ok(!a4.userDisabled);
    Assert.ok(a4.appDisabled);
  }
  Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));
  Assert.equal(a4.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a5, null);
  Assert.ok(!a5.isActive);
  Assert.ok(!a5.userDisabled);
  Assert.ok(a5.appDisabled);
  Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!isExtensionInAddonsList(profileDir, a5.id));

  Assert.notEqual(a6, null);
  Assert.ok(a6.isActive);
  Assert.ok(!a6.userDisabled);
  Assert.ok(!a6.appDisabled);
  Assert.equal(a6.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(a7, null);
  Assert.ok(!a7.isActive);
  Assert.ok(a7.userDisabled);
  Assert.ok(!a7.appDisabled);
  Assert.equal(a7.pendingOperations, AddonManager.PENDING_NONE);

  Assert.notEqual(t1, null);
  // Disabled due to bug 1394117
  // do_check_false(t1.isActive);
  // do_check_true(t1.userDisabled);
  Assert.ok(!t1.appDisabled);
  Assert.equal(t1.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_false(isThemeInAddonsList(profileDir, t1.id));

  Assert.notEqual(t2, null);
  // Disabled due to bug 1394117
  // do_check_true(t2.isActive);
  // do_check_false(t2.userDisabled);
  Assert.ok(!t2.appDisabled);
  Assert.equal(t2.pendingOperations, AddonManager.PENDING_NONE);
  // do_check_true(isThemeInAddonsList(profileDir, t2.id));

  try {
    shutdownManager();
  } catch (e) {
    // An error is expected here.
  }
});

