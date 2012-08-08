/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that updating an add-on to a new ID works

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver;
const profileDir = gProfD.clone();
profileDir.append("extensions");

function resetPrefs() {
  Services.prefs.setIntPref("bootstraptest.active_version", -1);
  Services.prefs.setIntPref("bootstraptest.installed_version", -1);
  Services.prefs.setIntPref("bootstraptest.startup_reason", -1);
  Services.prefs.setIntPref("bootstraptest.shutdown_reason", -1);
  Services.prefs.setIntPref("bootstraptest.install_reason", -1);
  Services.prefs.setIntPref("bootstraptest.uninstall_reason", -1);
}

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

function getInstalledVersion() {
  return Services.prefs.getIntPref("bootstraptest.installed_version");
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

function installUpdate(aInstall, aCallback) {
  aInstall.addListener({
    onInstallEnded: function(aInstall) {
      aCallback(aInstall);
    }
  });

  aInstall.install();
}

// Verify that an update to an add-on with a new ID uninstalls the old add-on
function run_test_1() {
  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updateid.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "1.0");

    a1.findUpdates({
      onUpdateAvailable: function(addon, install) {
        do_check_eq(install.name, addon.name);
        do_check_eq(install.version, "2.0");
        do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
        do_check_eq(install.existingAddon, a1);

        installUpdate(install, check_test_1);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_1(install) {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    // Existing add-on should have a pending upgrade
    do_check_neq(a1.pendingUpgrade, null);
    do_check_eq(a1.pendingUpgrade.id, "addon2@tests.mozilla.org");
    do_check_eq(a1.pendingUpgrade.install.existingAddon, a1);
    do_check_neq(a1.syncGUID);

    let a1SyncGUID = a1.syncGUID;

    restartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org"], function([a1, a2]) {
      // Should have uninstalled the old and installed the new
      do_check_eq(a1, null);
      do_check_neq(a2, null);
      do_check_neq(a2.syncGUID, null);

      // The Sync GUID should change when the ID changes
      do_check_neq(a1SyncGUID, a2.syncGUID);

      a2.uninstall();

      restartManager();
      shutdownManager();

      run_test_2();
    });
  });
}

// Test that when the new add-on already exists we just upgrade that
function run_test_2() {
  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updateid.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);
  writeInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 2",
  }, profileDir);

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "1.0");

    a1.findUpdates({
      onUpdateAvailable: function(addon, install) {
        installUpdate(install, check_test_2);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_2(install) {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    do_check_eq(a1.pendingUpgrade, null);
    // Existing add-on should have a pending upgrade
    do_check_neq(a2.pendingUpgrade, null);
    do_check_eq(a2.pendingUpgrade.id, "addon2@tests.mozilla.org");
    do_check_eq(a2.pendingUpgrade.install.existingAddon, a2);

    restartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org"], function([a1, a2]) {
      // Should have uninstalled the old and installed the new
      do_check_neq(a1, null);
      do_check_neq(a2, null);

      a1.uninstall();
      a2.uninstall();

      restartManager();
      shutdownManager();

      run_test_3();
    });
  });
}

// Test that we rollback correctly when removing the old add-on fails
function run_test_3() {
  // This test only works on Windows
  if (!("nsIWindowsRegKey" in AM_Ci)) {
    run_test_4();
    return;
  }

  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updateid.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "1.0");

    a1.findUpdates({
      onUpdateAvailable: function(addon, install) {
        installUpdate(install, check_test_3);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_3(install) {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    // Existing add-on should have a pending upgrade
    do_check_neq(a1.pendingUpgrade, null);
    do_check_eq(a1.pendingUpgrade.id, "addon2@tests.mozilla.org");
    do_check_eq(a1.pendingUpgrade.install.existingAddon, a1);

    // Lock the old add-on open so it can't be uninstalled
    var file = profileDir.clone();
    file.append("addon1@tests.mozilla.org");
    if (!file.exists())
      file.leafName += ".xpi";
    else
      file.append("install.rdf");

    var fstream = AM_Cc["@mozilla.org/network/file-output-stream;1"].
                  createInstance(AM_Ci.nsIFileOutputStream);
    fstream.init(file, FileUtils.MODE_APPEND | FileUtils.MODE_WRONLY, FileUtils.PERMS_FILE, 0);

    restartManager();

    fstream.close();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org"], function([a1, a2]) {
      // Should not have installed the new add-on but it should still be
      // pending install
      do_check_neq(a1, null);
      do_check_eq(a2, null);

      restartManager();

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org"], function([a1, a2]) {
        // Should have installed the new add-on
        do_check_eq(a1, null);
        do_check_neq(a2, null);

        a2.uninstall();

        restartManager();
        shutdownManager();

        run_test_4();
      });
    });
  });
}

// Tests that upgrading to a bootstrapped add-on works but requires a restart
function run_test_4() {
  writeInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "2.0",
    updateURL: "http://localhost:4444/data/test_updateid.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 2",
  }, profileDir);

  startupManager();

  resetPrefs();

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
    do_check_neq(a2, null);
    do_check_neq(a2.syncGUID, null);
    do_check_eq(a2.version, "2.0");

    a2.findUpdates({
      onUpdateAvailable: function(addon, install) {
        installUpdate(install, check_test_4);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_4() {
  AddonManager.getAddonsByIDs(["addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org"], function([a2, a3]) {
    // Should still be pending install even though the new add-on is restartless
    do_check_neq(a2, null);
    do_check_eq(a3, null);

    do_check_neq(a2.pendingUpgrade, null);
    do_check_eq(a2.pendingUpgrade.id, "addon3@tests.mozilla.org");

    do_check_eq(getInstalledVersion(), -1);
    do_check_eq(getActiveVersion(), -1);

    restartManager();

    AddonManager.getAddonsByIDs(["addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org"], function([a2, a3]) {
      // Should have updated
      do_check_eq(a2, null);
      do_check_neq(a3, null);

      do_check_eq(getInstalledVersion(), 3);
      do_check_eq(getActiveVersion(), 3);

      run_test_5();
    });
  });
}

// Tests that upgrading to another bootstrapped add-on works without a restart
function run_test_5() {
  AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
    do_check_neq(a3, null);
    do_check_eq(a3.version, "3.0");

    a3.findUpdates({
      onUpdateAvailable: function(addon, install) {
        installUpdate(install, check_test_5);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_5() {
  AddonManager.getAddonsByIDs(["addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"], function([a3, a4]) {
    // Should have updated
    do_check_eq(a3, null);
    do_check_neq(a4, null);

    do_check_eq(getInstalledVersion(), 4);
    do_check_eq(getActiveVersion(), 4);

    restartManager();

    AddonManager.getAddonsByIDs(["addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org"], function([a3, a4]) {
      // Should still be gone
      do_check_eq(a3, null);
      do_check_neq(a4, null);

      do_check_eq(getInstalledVersion(), 4);
      do_check_eq(getActiveVersion(), 4);

      run_test_6();
    });
  });
}

// Tests that upgrading to a non-bootstrapped add-on works but requires a restart
function run_test_6() {
  AddonManager.getAddonByID("addon4@tests.mozilla.org", function(a4) {
    do_check_neq(a4, null);
    do_check_eq(a4.version, "4.0");

    a4.findUpdates({
      onUpdateAvailable: function(addon, install) {
        installUpdate(install, check_test_6);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_6() {
  AddonManager.getAddonsByIDs(["addon4@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a4, a2]) {
    // Should still be pending install even though the old add-on is restartless
    do_check_neq(a4, null);
    do_check_eq(a2, null);

    do_check_neq(a4.pendingUpgrade, null);
    do_check_eq(a4.pendingUpgrade.id, "addon2@tests.mozilla.org");

    do_check_eq(getInstalledVersion(), 4);
    do_check_eq(getActiveVersion(), 4);

    restartManager();

    AddonManager.getAddonsByIDs(["addon4@tests.mozilla.org",
                                 "addon2@tests.mozilla.org"], function([a4, a2]) {
      // Should have updated
      do_check_eq(a4, null);
      do_check_neq(a2, null);

      do_check_eq(getInstalledVersion(), 0);
      do_check_eq(getActiveVersion(), 0);

      end_test();
    });
  });
}
