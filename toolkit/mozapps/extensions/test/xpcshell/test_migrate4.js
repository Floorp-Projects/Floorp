/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we migrate data from a previous version of the JSON database

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;
mapFile("/data/test_migrate4.rdf", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "2.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "2.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "2.0",
  name: "Test 4",
  strictCompatibility: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "2.0",
  name: "Test 5",
  updateURL: "http://localhost:" + gPort + "/data/test_migrate4.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0",
    maxVersion: "1"
  }]
};

var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test 6",
  updateURL: "http://localhost:" + gPort + "/data/test_migrate4.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0",
    maxVersion: "1"
  }]
};

var defaultTheme = {
  id: "default@tests.mozilla.org",
  version: "1.0",
  name: "Default",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

let oldSyncGUIDs = {};

function prepare_profile() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(defaultTheme, profileDir);

  startupManager();
  installAllFiles([do_get_addon("test_migrate8"), do_get_addon("test_migrate9")],
                  function() {
    restartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org",
                                 "addon9@tests.mozilla.org"],
                                 function([a1, a2, a3, a4, a5, a6, a9]) {
      a1.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
      a2.userDisabled = true;
      a2.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
      a3.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
      a4.userDisabled = true;
      a6.userDisabled = true;
      a9.userDisabled = false;

      for each (let addon in [a1, a2, a3, a4, a5, a6]) {
        oldSyncGUIDs[addon.id] = addon.syncGUID;
      }

      a6.findUpdates({
        onUpdateAvailable: function(aAddon, aInstall6) {
          AddonManager.getInstallForURL("http://localhost:" + gPort + "/addons/test_migrate4_7.xpi", function(aInstall7) {
            completeAllInstalls([aInstall6, aInstall7], function() {
              restartManager();
  
              AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                           "addon2@tests.mozilla.org",
                                           "addon3@tests.mozilla.org",
                                           "addon4@tests.mozilla.org",
                                           "addon5@tests.mozilla.org",
                                           "addon6@tests.mozilla.org"],
                                           function([a1, a2, a3, a4, a5, a6]) {
                a3.userDisabled = true;
                a4.userDisabled = false;
  
                a5.findUpdates({
                  onUpdateFinished: function() {
                    do_execute_soon(perform_migration);
                  }
                }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
              });
            });
          }, "application/x-xpinstall");
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });
}

function perform_migration() {
  shutdownManager();
  
  // Turn on disabling for all scopes
  Services.prefs.setIntPref("extensions.autoDisableScopes", 15);

  changeXPIDBVersion(1);
  Services.prefs.setIntPref("extensions.databaseSchema", 1);

  gAppInfo.version = "2"
  startupManager(true);
  test_results();
}

function test_results() {
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "addon8@tests.mozilla.org",
                               "addon9@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5, a6, a7, a8, a9]) {
    // addon1 was enabled
    do_check_neq(a1, null);
    do_check_eq(a1.syncGUID, oldSyncGUIDs[a1.id]);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_true(a1.isActive);
    do_check_eq(a1.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
    do_check_true(a1.foreignInstall);
    do_check_false(a1.hasBinaryComponents);
    do_check_false(a1.strictCompatibility);

    // addon2 was disabled
    do_check_neq(a2, null);
    do_check_eq(a2.syncGUID, oldSyncGUIDs[a2.id]);
    do_check_true(a2.userDisabled);
    do_check_false(a2.appDisabled);
    do_check_false(a2.isActive);
    do_check_eq(a2.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE);
    do_check_true(a2.foreignInstall);
    do_check_false(a2.hasBinaryComponents);
    do_check_false(a2.strictCompatibility);

    // addon3 was pending-disable in the database
    do_check_neq(a3, null);
    do_check_eq(a3.syncGUID, oldSyncGUIDs[a3.id]);
    do_check_true(a3.userDisabled);
    do_check_false(a3.appDisabled);
    do_check_false(a3.isActive);
    do_check_eq(a3.applyBackgroundUpdates, AddonManager.AUTOUPDATE_ENABLE);
    do_check_true(a3.foreignInstall);
    do_check_false(a3.hasBinaryComponents);
    do_check_false(a3.strictCompatibility);

    // addon4 was pending-enable in the database
    do_check_neq(a4, null);
    do_check_eq(a4.syncGUID, oldSyncGUIDs[a4.id]);
    do_check_false(a4.userDisabled);
    do_check_false(a4.appDisabled);
    do_check_true(a4.isActive);
    do_check_eq(a4.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
    do_check_true(a4.foreignInstall);
    do_check_false(a4.hasBinaryComponents);
    do_check_true(a4.strictCompatibility);

    // addon5 was enabled in the database but needed a compatibility update
    do_check_neq(a5, null);
    do_check_false(a5.userDisabled);
    do_check_false(a5.appDisabled);
    do_check_true(a5.isActive);
    do_check_eq(a4.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
    do_check_true(a5.foreignInstall);
    do_check_false(a5.hasBinaryComponents);
    do_check_false(a5.strictCompatibility);

    // addon6 was disabled and compatible but a new version has been installed
    do_check_neq(a6, null);
    do_check_eq(a6.syncGUID, oldSyncGUIDs[a6.id]);
    do_check_eq(a6.version, "2.0");
    do_check_true(a6.userDisabled);
    do_check_false(a6.appDisabled);
    do_check_false(a6.isActive);
    do_check_eq(a6.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
    do_check_true(a6.foreignInstall);
    do_check_eq(a6.sourceURI.spec, "http://localhost:" + gPort + "/addons/test_migrate4_6.xpi");
    do_check_eq(a6.releaseNotesURI.spec, "http://example.com/updateInfo.xhtml");
    do_check_false(a6.hasBinaryComponents);
    do_check_false(a6.strictCompatibility);

    // addon7 was installed manually
    do_check_neq(a7, null);
    do_check_eq(a7.version, "1.0");
    do_check_false(a7.userDisabled);
    do_check_false(a7.appDisabled);
    do_check_true(a7.isActive);
    do_check_eq(a7.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
    do_check_false(a7.foreignInstall);
    do_check_eq(a7.sourceURI.spec, "http://localhost:" + gPort + "/addons/test_migrate4_7.xpi");
    do_check_eq(a7.releaseNotesURI, null);
    do_check_false(a7.hasBinaryComponents);
    do_check_false(a7.strictCompatibility);

    // addon8 was enabled and has binary components
    do_check_neq(a8, null);
    do_check_false(a8.userDisabled);
    do_check_false(a8.appDisabled);
    do_check_true(a8.isActive);
    do_check_false(a8.foreignInstall);
    do_check_true(a8.hasBinaryComponents);
    do_check_false(a8.strictCompatibility);

    // addon9 is the active theme
    do_check_neq(a9, null);
    do_check_false(a9.userDisabled);
    do_check_false(a9.appDisabled);
    do_check_true(a9.isActive);
    do_check_false(a9.foreignInstall);
    do_check_false(a9.hasBinaryComponents);
    do_check_true(a9.strictCompatibility);

    testserver.stop(do_test_finished);
  });
}

function run_test() {
  do_test_pending();

  prepare_profile();
}
