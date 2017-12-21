/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a corrupt database


Components.utils.import("resource://testing-common/httpd.js");
// Create and configure the HTTP server.
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// register files with server
testserver.registerDirectory("/addons/", do_get_file("addons"));
mapFile("/data/test_corrupt.rdf", testserver);

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

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

// Will get a compatibility update and stay enabled
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

// Will get a compatibility update and be enabled
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

// Would stay incompatible with strict compat
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

function run_test() {
  do_test_pending();
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
  AddonTestUtils.manuallyInstall(theme2XPI).then(() => {
    // Startup the profile and setup the initial state
    startupManager();

    AddonManager.getAddonsByIDs(["addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon7@tests.mozilla.org",
                                 "theme2@tests.mozilla.org"], function([a2, a3, a4,
                                                                        a7, t2]) {
      // Set up the initial state
      a2.userDisabled = true;
      a4.userDisabled = true;
      a7.userDisabled = true;
      t2.userDisabled = false;
      a3.findUpdates({
        onUpdateFinished() {
          a4.findUpdates({
            onUpdateFinished() {
              do_execute_soon(run_test_1);
            }
          }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
        }
      }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
    });
  });
}

function end_test() {
  testserver.stop(do_test_finished);
}

function run_test_1() {
  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"],
                               callback_soon(function([a1, a2, a3, a4, a5, a6, a7, t1, t2]) {
    Assert.notEqual(a1, null);
    Assert.ok(a1.isActive);
    Assert.ok(!a1.userDisabled);
    Assert.ok(!a1.appDisabled);
    Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);

    Assert.notEqual(a2, null);
    Assert.ok(!a2.isActive);
    Assert.ok(a2.userDisabled);
    Assert.ok(!a2.appDisabled);
    Assert.equal(a2.pendingOperations, AddonManager.PENDING_NONE);

    Assert.notEqual(a3, null);
    Assert.ok(a3.isActive);
    Assert.ok(!a3.userDisabled);
    Assert.ok(!a3.appDisabled);
    Assert.equal(a3.pendingOperations, AddonManager.PENDING_NONE);

    Assert.notEqual(a4, null);
    Assert.ok(!a4.isActive);
    Assert.ok(a4.userDisabled);
    Assert.ok(!a4.appDisabled);
    Assert.equal(a4.pendingOperations, AddonManager.PENDING_NONE);

    Assert.notEqual(a5, null);
    Assert.ok(a5.isActive);
    Assert.ok(!a5.userDisabled);
    Assert.ok(!a5.appDisabled);
    Assert.equal(a5.pendingOperations, AddonManager.PENDING_NONE);

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

    Assert.notEqual(t2, null);
    Assert.ok(t2.isActive);
    Assert.ok(!t2.userDisabled);
    Assert.ok(!t2.appDisabled);
    Assert.equal(t2.pendingOperations, AddonManager.PENDING_NONE);

    // Shutdown and replace the database with a corrupt file (a directory
    // serves this purpose). On startup the add-ons manager won't rebuild
    // because there is a file there still.
    shutdownManager();
    gExtensionsJSON.remove(true);
    gExtensionsJSON.create(AM_Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    startupManager(false);

    // Load the database.
    awaitPromise(new Promise(resolve => {
      Services.obs.addObserver(function listener() {
        Services.obs.removeObserver(listener, "xpi-database-loaded");
        resolve();
      }, "xpi-database-loaded");
      Services.obs.notifyObservers(null, "sessionstore-windows-restored");
    }));

    // Accessing the add-ons should open and recover the database
    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org",
                                 "addon7@tests.mozilla.org",
                                 "theme1@tests.mozilla.org",
                                 "theme2@tests.mozilla.org"],
                                 callback_soon(function([a1_2, a2_2, a3_2, a4_2, a5_2, a6_2, a7_2, t1_2, t2_2]) {
      // Should be correctly recovered
      Assert.notEqual(a1_2, null);
      Assert.ok(a1_2.isActive);
      Assert.ok(!a1_2.userDisabled);
      Assert.ok(!a1_2.appDisabled);
      Assert.equal(a1_2.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      Assert.notEqual(a2_2, null);
      Assert.ok(!a2_2.isActive);
      Assert.ok(a2_2.userDisabled);
      Assert.ok(!a2_2.appDisabled);
      Assert.equal(a2_2.pendingOperations, AddonManager.PENDING_NONE);

      // The compatibility update won't be recovered but it should still be
      // active for this session
      Assert.notEqual(a3_2, null);
      Assert.ok(a3_2.isActive);
      Assert.ok(!a3_2.userDisabled);
      Assert.ok(!a3_2.appDisabled);
      Assert.equal(a3_2.pendingOperations, AddonManager.PENDING_NONE);

      // The compatibility update won't be recovered and with strict
      // compatibility it would not have been able to tell that it was
      // previously userDisabled. However, without strict compat, it wasn't
      // appDisabled, so it knows it must have been userDisabled.
      Assert.notEqual(a4_2, null);
      Assert.ok(!a4_2.isActive);
      Assert.ok(a4_2.userDisabled);
      Assert.ok(!a4_2.appDisabled);
      Assert.equal(a4_2.pendingOperations, AddonManager.PENDING_NONE);

      Assert.notEqual(a5_2, null);
      Assert.ok(a5_2.isActive);
      Assert.ok(!a5_2.userDisabled);
      Assert.ok(!a5_2.appDisabled);
      Assert.equal(a5_2.pendingOperations, AddonManager.PENDING_NONE);

      Assert.notEqual(a6_2, null);
      Assert.ok(a6_2.isActive);
      Assert.ok(!a6_2.userDisabled);
      Assert.ok(!a6_2.appDisabled);
      Assert.equal(a6_2.pendingOperations, AddonManager.PENDING_NONE);

      Assert.notEqual(a7_2, null);
      Assert.ok(!a7_2.isActive);
      Assert.ok(a7_2.userDisabled);
      Assert.ok(!a7_2.appDisabled);
      Assert.equal(a7_2.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      Assert.notEqual(t1_2, null);

      // Disabled due to bug 1394117
      // do_check_false(t1_2.isActive);
      // do_check_true(t1_2.userDisabled);
      Assert.ok(!t1_2.appDisabled);
      Assert.equal(t1_2.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      Assert.notEqual(t2_2, null);
      Assert.ok(t2_2.isActive);
      // Disabled due to bug 1394117
      // do_check_false(t2_2.userDisabled);
      Assert.ok(!t2_2.appDisabled);
      // do_check_eq(t2_2.pendingOperations, AddonManager.PENDING_NONE);

      Assert.throws(shutdownManager);
      startupManager(false);

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org",
                                   "addon3@tests.mozilla.org",
                                   "addon4@tests.mozilla.org",
                                   "addon5@tests.mozilla.org",
                                   "addon6@tests.mozilla.org",
                                   "addon7@tests.mozilla.org",
                                   "theme1@tests.mozilla.org",
                                   "theme2@tests.mozilla.org"],
                                   callback_soon(function([a1_3, a2_3, a3_3, a4_3, a5_3, a6_3, a7_3, t1_3, t2_3]) {
        Assert.notEqual(a1_3, null);
        Assert.ok(a1_3.isActive);
        Assert.ok(!a1_3.userDisabled);
        Assert.ok(!a1_3.appDisabled);
        Assert.equal(a1_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a2_3, null);
        Assert.ok(!a2_3.isActive);
        Assert.ok(a2_3.userDisabled);
        Assert.ok(!a2_3.appDisabled);
        Assert.equal(a2_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a3_3, null);
        Assert.ok(a3_3.isActive);
        Assert.ok(!a3_3.userDisabled);
        Assert.ok(!a3_3.appDisabled);
        Assert.equal(a3_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a4_3, null);
        Assert.ok(!a4_3.isActive);
        Assert.ok(a4_3.userDisabled);
        Assert.ok(!a4_3.appDisabled);
        Assert.equal(a4_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a5_3, null);
        Assert.ok(a5_3.isActive);
        Assert.ok(!a5_3.userDisabled);
        Assert.ok(!a5_3.appDisabled);
        Assert.equal(a5_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a6_3, null);
        Assert.ok(a6_3.isActive);
        Assert.ok(!a6_3.userDisabled);
        Assert.ok(!a6_3.appDisabled);
        Assert.equal(a6_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(a7_3, null);
        Assert.ok(!a7_3.isActive);
        Assert.ok(a7_3.userDisabled);
        Assert.ok(!a7_3.appDisabled);
        Assert.equal(a7_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(t1_3, null);
        // Disabled due to bug 1394117
        // do_check_false(t1_3.isActive);
        // do_check_true(t1_3.userDisabled);
        Assert.ok(!t1_3.appDisabled);
        Assert.equal(t1_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.notEqual(t2_3, null);
        // Disabled due to bug 1394117
        // do_check_true(t2_3.isActive);
        // do_check_false(t2_3.userDisabled);
        Assert.ok(!t2_3.appDisabled);
        Assert.equal(t2_3.pendingOperations, AddonManager.PENDING_NONE);

        Assert.throws(shutdownManager);

        end_test();
      }));
    }));
  }));
}
