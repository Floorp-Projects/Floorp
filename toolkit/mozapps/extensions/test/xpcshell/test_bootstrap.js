/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const APP_STARTUP                     = 1;
const APP_SHUTDOWN                    = 2;
const ADDON_ENABLE                    = 3;
const ADDON_DISABLE                   = 4;
const ADDON_INSTALL                   = 5;
const ADDON_UNINSTALL                 = 6;
const ADDON_UPGRADE                   = 7;
const ADDON_DOWNGRADE                 = 8;

const ID1 = "bootstrap1@tests.mozilla.org";
const ID2 = "bootstrap2@tests.mozilla.org";

// This verifies that bootstrappable add-ons can be used without restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

BootstrapMonitor.init();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userExtDir.parent);

Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(undefined);
gPort = testserver.identity.primaryPort;

testserver.registerDirectory("/addons/", do_get_file("addons"));

function getStartupReason() {
  let info = BootstrapMonitor.started.get(ID1);
  return info ? info.reason : undefined;
}

function getShutdownReason() {
  let info = BootstrapMonitor.stopped.get(ID1);
  return info ? info.reason : undefined;
}

function getInstallReason() {
  let info = BootstrapMonitor.installed.get(ID1);
  return info ? info.reason : undefined;
}

function getUninstallReason() {
  let info = BootstrapMonitor.uninstalled.get(ID1);
  return info ? info.reason : undefined;
}

function getStartupOldVersion() {
  let info = BootstrapMonitor.started.get(ID1);
  return info ? info.data.oldVersion : undefined;
}

function getShutdownNewVersion() {
  let info = BootstrapMonitor.stopped.get(ID1);
  return info ? info.data.newVersion : undefined;
}

function getInstallOldVersion() {
  let info = BootstrapMonitor.installed.get(ID1);
  return info ? info.data.oldVersion : undefined;
}

function getUninstallNewVersion() {
  let info = BootstrapMonitor.uninstalled.get(ID1);
  return info ? info.data.newVersion : undefined;
}

function do_check_bootstrappedPref(aCallback) {
  let XPIScope = AM_Cu.import("resource://gre/modules/addons/XPIProvider.jsm", {});

  let data = {};
  for (let entry of XPIScope.XPIStates.bootstrappedAddons()) {
    data[entry.id] = entry;
  }

  AddonManager.getAddonsByTypes(["extension"], function(aAddons) {
    for (let addon of aAddons) {
      if (!addon.id.endsWith("@tests.mozilla.org"))
        continue;
      if (!addon.isActive)
        continue;
      if (addon.operationsRequiringRestart != AddonManager.OP_NEEDS_RESTART_NONE)
        continue;

      Assert.ok(addon.id in data);
      let addonData = data[addon.id];
      delete data[addon.id];

      Assert.equal(addonData.version, addon.version);
      Assert.equal(addonData.type, addon.type);
      let file = addon.getResourceURI().QueryInterface(Components.interfaces.nsIFileURL).file;
      Assert.equal(addonData.path, file.path);
    }
    Assert.equal(Object.keys(data).length, 0);

    executeSoon(aCallback);
  });
}


function run_test() {
  do_test_pending();

  startupManager();

  Assert.ok(!gExtensionsJSON.exists());

  Assert.ok(!gAddonStartup.exists());

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.type, "extension");
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Test Bootstrap 1");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.notEqual(install.addon.syncGUID, null);
    Assert.ok(install.addon.hasResource("install.rdf"));
    Assert.ok(install.addon.hasResource("bootstrap.js"));
    Assert.ok(!install.addon.hasResource("foo.bar"));
    Assert.equal(install.addon.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
    do_check_not_in_crash_annotation(ID1, "1.0");

    let addon = install.addon;

    BootstrapMonitor.promiseAddonStartup(ID1).then(function() {
      do_check_bootstrappedPref(function() {
        check_test_1(addon.syncGUID);
      });
    });

    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      Assert.ok(addon.hasResource("install.rdf"));

      // startup should not have been called yet.
      BootstrapMonitor.checkAddonNotStarted(ID1);
    });
    install.install();
  });
}

function check_test_1(installSyncGUID) {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    Assert.equal(installs.length, 0);

    AddonManager.getAddonByID(ID1, function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.notEqual(b1.syncGUID, null);
      Assert.equal(b1.syncGUID, installSyncGUID);
      Assert.ok(!b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");
      Assert.equal(getStartupReason(), ADDON_INSTALL);
      Assert.equal(getStartupOldVersion(), undefined);
      Assert.ok(b1.hasResource("install.rdf"));
      Assert.ok(b1.hasResource("bootstrap.js"));
      Assert.ok(!b1.hasResource("foo.bar"));
      do_check_in_crash_annotation(ID1, "1.0");

      let dir = do_get_addon_root_uri(profileDir, ID1);
      Assert.equal(b1.getResourceURI("bootstrap.js").spec, dir + "bootstrap.js");

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        Assert.equal(list.length, 0);

        executeSoon(run_test_2);
      });
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddonByID(ID1, function(b1) {
    prepare_test({
      [ID1]: [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
    b1.userDisabled = true;
    ensure_test_completed();

    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(b1.userDisabled);
    Assert.ok(!b1.isActive);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonNotStarted(ID1);
    Assert.equal(getShutdownReason(), ADDON_DISABLE);
    Assert.equal(getShutdownNewVersion(), undefined);
    do_check_not_in_crash_annotation(ID1, "1.0");

    AddonManager.getAddonByID(ID1, function(newb1) {
      Assert.notEqual(newb1, null);
      Assert.equal(newb1.version, "1.0");
      Assert.ok(!newb1.appDisabled);
      Assert.ok(newb1.userDisabled);
      Assert.ok(!newb1.isActive);

      do_check_bootstrappedPref(run_test_3);
    });
  });
}

// Test that restarting doesn't accidentally re-enable
function run_test_3() {
  shutdownManager();
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  Assert.equal(getShutdownReason(), ADDON_DISABLE);
  Assert.equal(getShutdownNewVersion(), undefined);
  startupManager(false);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  Assert.equal(getShutdownReason(), ADDON_DISABLE);
  Assert.equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");

  Assert.ok(gAddonStartup.exists());

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(b1.userDisabled);
    Assert.ok(!b1.isActive);

    do_check_bootstrappedPref(run_test_4);
  });
}

// Tests that enabling doesn't require a restart
function run_test_4() {
  AddonManager.getAddonByID(ID1, function(b1) {
    prepare_test({
      [ID1]: [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
    b1.userDisabled = false;
    ensure_test_completed();

    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.equal(getStartupReason(), ADDON_ENABLE);
    Assert.equal(getStartupOldVersion(), undefined);
    do_check_in_crash_annotation(ID1, "1.0");

    AddonManager.getAddonByID(ID1, function(newb1) {
      Assert.notEqual(newb1, null);
      Assert.equal(newb1.version, "1.0");
      Assert.ok(!newb1.appDisabled);
      Assert.ok(!newb1.userDisabled);
      Assert.ok(newb1.isActive);

      do_check_bootstrappedPref(run_test_5);
    });
  });
}

// Tests that a restart shuts down and restarts the add-on
function run_test_5() {
  shutdownManager();
  // By the time we've shut down, the database must have been written
  Assert.ok(gExtensionsJSON.exists());

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  Assert.equal(getShutdownReason(), APP_SHUTDOWN);
  Assert.equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");
  startupManager(false);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  Assert.equal(getStartupReason(), APP_STARTUP);
  Assert.equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    Assert.ok(!isExtensionInAddonsList(profileDir, b1.id));

    do_check_bootstrappedPref(run_test_6);
  });
}

// Tests that installing an upgrade doesn't require a restart
function run_test_6() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.type, "extension");
    Assert.equal(install.version, "2.0");
    Assert.equal(install.name, "Test Bootstrap 1");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);

    BootstrapMonitor.promiseAddonStartup(ID1).then(check_test_6);
    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
    });
    install.install();
  });
}

function check_test_6() {
  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "2.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
    BootstrapMonitor.checkAddonStarted(ID1, "2.0");
    Assert.equal(getStartupReason(), ADDON_UPGRADE);
    Assert.equal(getInstallOldVersion(), 1);
    Assert.equal(getStartupOldVersion(), 1);
    Assert.equal(getShutdownReason(), ADDON_UPGRADE);
    Assert.equal(getShutdownNewVersion(), 2);
    Assert.equal(getUninstallNewVersion(), 2);
    do_check_not_in_crash_annotation(ID1, "1.0");
    do_check_in_crash_annotation(ID1, "2.0");

    do_check_bootstrappedPref(run_test_7);
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddonByID(ID1, function(b1) {
    prepare_test({
      [ID1]: [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
    b1.uninstall();

    do_check_bootstrappedPref(check_test_7);
  });
}

function check_test_7() {
  ensure_test_completed();
  BootstrapMonitor.checkAddonNotInstalled(ID1);
  BootstrapMonitor.checkAddonNotStarted(ID1);
  Assert.equal(getShutdownReason(), ADDON_UNINSTALL);
  Assert.equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "2.0");

  AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
    Assert.equal(b1, null);

    restartManager();

    AddonManager.getAddonByID(ID1, function(newb1) {
      Assert.equal(newb1, null);

      do_check_bootstrappedPref(run_test_8);
    });
  }));
}

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
function run_test_8() {
  shutdownManager();

  manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir,
                  ID1);

  startupManager(false);

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.equal(getStartupReason(), ADDON_INSTALL);
    Assert.equal(getStartupOldVersion(), undefined);
    do_check_in_crash_annotation(ID1, "1.0");

    do_check_bootstrappedPref(run_test_9);
  });
}

// Test that items detected as removed during startup get removed properly
function run_test_9() {
  shutdownManager();

  manuallyUninstall(profileDir, ID1);
  BootstrapMonitor.clear(ID1);

  startupManager(false);

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.equal(b1, null);
    do_check_not_in_crash_annotation(ID1, "1.0");

    do_check_bootstrappedPref(run_test_10);
  });
}


// Tests that installing a downgrade sends the right reason
function run_test_10() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.type, "extension");
    Assert.equal(install.version, "2.0");
    Assert.equal(install.name, "Test Bootstrap 1");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.ok(install.addon.hasResource("install.rdf"));
    Assert.ok(install.addon.hasResource("bootstrap.js"));
    Assert.ok(!install.addon.hasResource("foo.bar"));
    do_check_not_in_crash_annotation(ID1, "2.0");

    BootstrapMonitor.promiseAddonStartup(ID1).then(check_test_10_pt1);
    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      info("Waiting for startup of bootstrap1_2");
    });
    install.install();
  });
}

function check_test_10_pt1() {
  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "2.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
    BootstrapMonitor.checkAddonStarted(ID1, "2.0");
    Assert.equal(getStartupReason(), ADDON_INSTALL);
    Assert.equal(getStartupOldVersion(), undefined);
    Assert.ok(b1.hasResource("install.rdf"));
    Assert.ok(b1.hasResource("bootstrap.js"));
    Assert.ok(!b1.hasResource("foo.bar"));
    do_check_in_crash_annotation(ID1, "2.0");

    prepare_test({ }, [
      "onNewInstall"
    ]);

    AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), function(install) {
      ensure_test_completed();

      Assert.notEqual(install, null);
      Assert.equal(install.type, "extension");
      Assert.equal(install.version, "1.0");
      Assert.equal(install.name, "Test Bootstrap 1");
      Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);

      BootstrapMonitor.promiseAddonStartup(ID1).then(check_test_10_pt2);
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() { });
      install.install();
    });
  });
}

function check_test_10_pt2() {
  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.equal(getStartupReason(), ADDON_DOWNGRADE);
    Assert.equal(getInstallOldVersion(), 2);
    Assert.equal(getStartupOldVersion(), 2);
    Assert.equal(getShutdownReason(), ADDON_DOWNGRADE);
    Assert.equal(getShutdownNewVersion(), 1);
    Assert.equal(getUninstallNewVersion(), 1);
    do_check_in_crash_annotation(ID1, "1.0");
    do_check_not_in_crash_annotation(ID1, "2.0");

    do_check_bootstrappedPref(run_test_11);
  });
}

// Tests that uninstalling a disabled add-on still calls the uninstall method
function run_test_11() {
  AddonManager.getAddonByID(ID1, function(b1) {
    prepare_test({
      [ID1]: [
        ["onDisabling", false],
        "onDisabled",
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    b1.userDisabled = true;

    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonNotStarted(ID1);
    Assert.equal(getShutdownReason(), ADDON_DISABLE);
    Assert.equal(getShutdownNewVersion(), undefined);
    do_check_not_in_crash_annotation(ID1, "1.0");

    b1.uninstall();

    check_test_11();
  });
}

function check_test_11() {
  ensure_test_completed();
  BootstrapMonitor.checkAddonNotInstalled(ID1);
  BootstrapMonitor.checkAddonNotStarted(ID1);
  do_check_not_in_crash_annotation(ID1, "1.0");

  do_check_bootstrappedPref(run_test_12);
}

// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
function run_test_12() {
  shutdownManager();

  manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir,
                  ID1);

  startupManager(true);

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.equal(getStartupReason(), ADDON_INSTALL);
    Assert.equal(getStartupOldVersion(), undefined);
    do_check_in_crash_annotation(ID1, "1.0");

    b1.uninstall();
    executeSoon(test_12_restart);
  });
}

function test_12_restart() {
  restartManager();
  do_check_bootstrappedPref(run_test_13);
}


// Tests that installing a bootstrapped extension with an invalid application
// entry doesn't call it's startup method
function run_test_13() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_3"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.type, "extension");
    Assert.equal(install.version, "3.0");
    Assert.equal(install.name, "Test Bootstrap 1");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_not_in_crash_annotation(ID1, "3.0");

    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(check_test_13));
    install.install();
  });
}

function check_test_13() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    Assert.equal(installs.length, 0);

    AddonManager.getAddonByID(ID1, function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "3.0");
      Assert.ok(b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(!b1.isActive);
      BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
      BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
      do_check_not_in_crash_annotation(ID1, "3.0");

      executeSoon(test_13_restart);
    });
  });
}

function test_13_restart() {
  restartManager();

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "3.0");
    Assert.ok(b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(!b1.isActive);
    BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
    BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
    do_check_not_in_crash_annotation(ID1, "3.0");

    do_check_bootstrappedPref(function() {
      b1.uninstall();
      executeSoon(run_test_14);
    });
  });
}

// Tests that a bootstrapped extension with an invalid target application entry
// does not get loaded when detected during startup
function run_test_14() {
  restartManager();

  shutdownManager();

  manuallyInstall(do_get_addon("test_bootstrap1_3"), profileDir,
                  ID1);

  startupManager(false);

  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "3.0");
    Assert.ok(b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(!b1.isActive);
    BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
    BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
    do_check_not_in_crash_annotation(ID1, "3.0");

    do_check_bootstrappedPref(function() {
      b1.uninstall();

      run_test_15();
    });
  });
}

// Tests that upgrading a disabled bootstrapped extension still calls uninstall
// and install but doesn't startup the new version
function run_test_15() {
  BootstrapMonitor.promiseAddonStartup(ID1).then(function test_15_after_startup() {
    AddonManager.getAddonByID(ID1, function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.ok(!b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");

      b1.userDisabled = true;
      Assert.ok(!b1.isActive);
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonNotStarted(ID1);

      prepare_test({ }, [
        "onNewInstall"
      ]);

      AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
        ensure_test_completed();

        Assert.notEqual(install, null);
        Assert.ok(install.addon.userDisabled);

        prepare_test({
          [ID1]: [
            ["onInstalling", false],
            "onInstalled"
          ]
        }, [
          "onInstallStarted",
          "onInstallEnded",
        ], callback_soon(check_test_15));
        install.install();
      });
    });
  });
  installAllFiles([do_get_addon("test_bootstrap1_1")], function test_15_addon_installed() { });
}

function check_test_15() {
  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "2.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(b1.userDisabled);
    Assert.ok(!b1.isActive);
    BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
    BootstrapMonitor.checkAddonNotStarted(ID1);

    do_check_bootstrappedPref(function() {
      restartManager();

      AddonManager.getAddonByID(ID1, callback_soon(function(b1_2) {
        Assert.notEqual(b1_2, null);
        Assert.equal(b1_2.version, "2.0");
        Assert.ok(!b1_2.appDisabled);
        Assert.ok(b1_2.userDisabled);
        Assert.ok(!b1_2.isActive);
        BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
        BootstrapMonitor.checkAddonNotStarted(ID1);

        b1_2.uninstall();

        run_test_16();
      }));
    });
  });
}

// Tests that bootstrapped extensions don't get loaded when in safe mode
function run_test_16() {
  BootstrapMonitor.promiseAddonStartup(ID1).then(function test_16_after_startup() {
    AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
      // Should have installed and started
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);
      Assert.equal(b1.iconURL, "chrome://foo/skin/icon.png");
      Assert.equal(b1.aboutURL, "chrome://foo/content/about.xul");
      Assert.equal(b1.optionsURL, "chrome://foo/content/options.xul");

      shutdownManager();

      // Should have stopped
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonNotStarted(ID1);

      gAppInfo.inSafeMode = true;
      startupManager(false);

      AddonManager.getAddonByID(ID1, callback_soon(function(b1_2) {
        // Should still be stopped
        BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
        BootstrapMonitor.checkAddonNotStarted(ID1);
        Assert.ok(!b1_2.isActive);
        Assert.equal(b1_2.iconURL, null);
        Assert.equal(b1_2.aboutURL, null);
        Assert.equal(b1_2.optionsURL, null);

        shutdownManager();
        gAppInfo.inSafeMode = false;
        startupManager(false);

        // Should have started
        BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
        BootstrapMonitor.checkAddonStarted(ID1, "1.0");

        AddonManager.getAddonByID(ID1, function(b1_3) {
          b1_3.uninstall();

          executeSoon(run_test_17);
        });
      }));
    }));
  });
  installAllFiles([do_get_addon("test_bootstrap1_1")], function() { });
}

// Check that a bootstrapped extension in a non-profile location is loaded
function run_test_17() {
  shutdownManager();

  manuallyInstall(do_get_addon("test_bootstrap1_1"), userExtDir,
                  ID1);

  startupManager();

  AddonManager.getAddonByID(ID1, function(b1) {
    // Should have installed and started
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);

    do_check_bootstrappedPref(run_test_18);
  });
}

// Check that installing a new bootstrapped extension in the profile replaces
// the existing one
function run_test_18() {
  BootstrapMonitor.promiseAddonStartup(ID1).then(function test_18_after_startup() {
    AddonManager.getAddonByID(ID1, function(b1) {
      // Should have installed and started
      BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
      BootstrapMonitor.checkAddonStarted(ID1, "2.0");
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "2.0");
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);

      Assert.equal(getShutdownReason(), ADDON_UPGRADE);
      Assert.equal(getUninstallReason(), ADDON_UPGRADE);
      Assert.equal(getInstallReason(), ADDON_UPGRADE);
      Assert.equal(getStartupReason(), ADDON_UPGRADE);

      Assert.equal(getShutdownNewVersion(), 2);
      Assert.equal(getUninstallNewVersion(), 2);
      Assert.equal(getInstallOldVersion(), 1);
      Assert.equal(getStartupOldVersion(), 1);

      do_check_bootstrappedPref(run_test_19);
    });
  });
  installAllFiles([do_get_addon("test_bootstrap1_2")], function() { });
}

// Check that uninstalling the profile version reveals the non-profile one
function run_test_19() {
  AddonManager.getAddonByID(ID1, function(b1) {
    // The revealed add-on gets activated asynchronously
    prepare_test({
      [ID1]: [
        ["onUninstalling", false],
        "onUninstalled",
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [], check_test_19);

    b1.uninstall();
  });
}

function check_test_19() {
  AddonManager.getAddonByID(ID1, function(b1) {
    // Should have reverted to the older version
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);

    Assert.equal(getShutdownReason(), ADDON_DOWNGRADE);
    Assert.equal(getUninstallReason(), ADDON_DOWNGRADE);
    Assert.equal(getInstallReason(), ADDON_DOWNGRADE);
    Assert.equal(getStartupReason(), ADDON_DOWNGRADE);

    Assert.equal(getShutdownNewVersion(), undefined);
    Assert.equal(getUninstallNewVersion(), undefined);
    Assert.equal(getInstallOldVersion(), undefined);
    Assert.equal(getStartupOldVersion(), undefined);

    do_check_bootstrappedPref(run_test_20);
  });
}

// Check that a new profile extension detected at startup replaces the non-profile
// one
function run_test_20() {
  shutdownManager();

  manuallyInstall(do_get_addon("test_bootstrap1_2"), profileDir,
                  ID1);

  startupManager();

  AddonManager.getAddonByID(ID1, function(b1) {
    // Should have installed and started
    BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
    BootstrapMonitor.checkAddonStarted(ID1, "2.0");
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "2.0");
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);

    Assert.equal(getShutdownReason(), APP_SHUTDOWN);
    Assert.equal(getUninstallReason(), ADDON_UPGRADE);
    Assert.equal(getInstallReason(), ADDON_UPGRADE);
    Assert.equal(getStartupReason(), APP_STARTUP);

    Assert.equal(getShutdownNewVersion(), undefined);
    Assert.equal(getUninstallNewVersion(), 2);
    Assert.equal(getInstallOldVersion(), 1);
    Assert.equal(getStartupOldVersion(), undefined);

    executeSoon(run_test_21);
  });
}

// Check that a detected removal reveals the non-profile one
function run_test_21() {
  shutdownManager();

  Assert.equal(getShutdownReason(), APP_SHUTDOWN);
  Assert.equal(getShutdownNewVersion(), undefined);

  manuallyUninstall(profileDir, ID1);
  BootstrapMonitor.clear(ID1);

  startupManager();

  AddonManager.getAddonByID(ID1, function(b1) {
    // Should have installed and started
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);

    // This won't be set as the bootstrap script was gone so we couldn't
    // uninstall it properly
    Assert.equal(getUninstallReason(), undefined);
    Assert.equal(getUninstallNewVersion(), undefined);

    Assert.equal(getInstallReason(), ADDON_DOWNGRADE);
    Assert.equal(getInstallOldVersion(), 2);

    Assert.equal(getStartupReason(), APP_STARTUP);
    Assert.equal(getStartupOldVersion(), undefined);

    do_check_bootstrappedPref(function() {
      shutdownManager();

      manuallyUninstall(userExtDir, ID1);
      BootstrapMonitor.clear(ID1);

      startupManager(false);
      run_test_22();
    });
  });
}

// Check that an upgrade from the filesystem is detected and applied correctly
function run_test_22() {
  shutdownManager();

  let file = manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir,
                             ID1);
  if (file.isDirectory())
    file.append("install.rdf");

  // Make it look old so changes are detected
  setExtensionModifiedTime(file, file.lastModifiedTime - 5000);

  startupManager();

  AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
    // Should have installed and started
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(b1.isActive);
    Assert.ok(!b1.isSystem);

    shutdownManager();

    Assert.equal(getShutdownReason(), APP_SHUTDOWN);
    Assert.equal(getShutdownNewVersion(), undefined);

    manuallyUninstall(profileDir, ID1);
    BootstrapMonitor.clear(ID1);
    manuallyInstall(do_get_addon("test_bootstrap1_2"), profileDir,
                    ID1);

    startupManager();

    AddonManager.getAddonByID(ID1, function(b1_2) {
      // Should have installed and started
      BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
      BootstrapMonitor.checkAddonStarted(ID1, "2.0");
      Assert.notEqual(b1_2, null);
      Assert.equal(b1_2.version, "2.0");
      Assert.ok(b1_2.isActive);
      Assert.ok(!b1_2.isSystem);

      // This won't be set as the bootstrap script was gone so we couldn't
      // uninstall it properly
      Assert.equal(getUninstallReason(), undefined);
      Assert.equal(getUninstallNewVersion(), undefined);

      Assert.equal(getInstallReason(), ADDON_UPGRADE);
      Assert.equal(getInstallOldVersion(), 1);
      Assert.equal(getStartupReason(), APP_STARTUP);
      Assert.equal(getStartupOldVersion(), undefined);

      do_check_bootstrappedPref(function() {
        b1_2.uninstall();

        run_test_23();
      });
    });
  }));
}


// Tests that installing from a URL doesn't require a restart
function run_test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_bootstrap1_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);

    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], function() {
      Assert.equal(install.type, "extension");
      Assert.equal(install.version, "1.0");
      Assert.equal(install.name, "Test Bootstrap 1");
      Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
      Assert.ok(install.addon.hasResource("install.rdf"));
      Assert.ok(install.addon.hasResource("bootstrap.js"));
      Assert.ok(!install.addon.hasResource("foo.bar"));
      Assert.equal(install.addon.operationsRequiringRestart &
                   AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
      do_check_not_in_crash_annotation(ID1, "1.0");

      let addon = install.addon;
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() {
        Assert.ok(addon.hasResource("install.rdf"));
        do_check_bootstrappedPref(check_test_23);
      });
    });
    install.install();
  }, "application/x-xpinstall");
}

function check_test_23() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    Assert.equal(installs.length, 0);

    AddonManager.getAddonByID(ID1, function(b1) {
     executeSoon(function test_23_after_startup() {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.ok(!b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");
      Assert.equal(getStartupReason(), ADDON_INSTALL);
      Assert.equal(getStartupOldVersion(), undefined);
      Assert.ok(b1.hasResource("install.rdf"));
      Assert.ok(b1.hasResource("bootstrap.js"));
      Assert.ok(!b1.hasResource("foo.bar"));
      do_check_in_crash_annotation(ID1, "1.0");

      let dir = do_get_addon_root_uri(profileDir, ID1);
      Assert.equal(b1.getResourceURI("bootstrap.js").spec, dir + "bootstrap.js");

      AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(function(list) {
        Assert.equal(list.length, 0);

        restartManager();
        AddonManager.getAddonByID(ID1, callback_soon(function(b1_2) {
          b1_2.uninstall();
          restartManager();

          testserver.stop(run_test_24);
        }));
      }));
     });
    });
  });
}

// Tests that we recover from a broken preference
function run_test_24() {
  info("starting 24");

  Promise.all([BootstrapMonitor.promiseAddonStartup(ID2),
              promiseInstallAllFiles([do_get_addon("test_bootstrap1_1"), do_get_addon("test_bootstrap2_1")])])
         .then(async function test_24_pref() {
    info("test 24 got prefs");
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
    BootstrapMonitor.checkAddonStarted(ID2, "1.0");

    restartManager();

    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
    BootstrapMonitor.checkAddonStarted(ID2, "1.0");

    shutdownManager();

    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonNotStarted(ID1);
    BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
    BootstrapMonitor.checkAddonNotStarted(ID2);

    // Break the JSON.
    let data = aomStartup.readStartupData();
    data["app-profile"].addons[ID1].path += "foo";

    await OS.File.writeAtomic(gAddonStartup.path,
                              new TextEncoder().encode(JSON.stringify(data)),
                              {compression: "lz4"});

    startupManager(false);

    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonStarted(ID1, "1.0");
    BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
    BootstrapMonitor.checkAddonStarted(ID2, "1.0");

    run_test_25();
  });
}

// Tests that updating from a bootstrappable add-on to a normal add-on calls
// the uninstall method
function run_test_25() {
  BootstrapMonitor.promiseAddonStartup(ID1).then(function test_25_after_pref() {
      info("test 25 pref change detected");
      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");

      installAllFiles([do_get_addon("test_bootstrap1_4")], function() {
        // Needs a restart to complete this so the old version stays running
        BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
        BootstrapMonitor.checkAddonStarted(ID1, "1.0");

        AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
          Assert.notEqual(b1, null);
          Assert.equal(b1.version, "1.0");
          Assert.ok(b1.isActive);
          Assert.ok(!b1.isSystem);
          Assert.ok(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));

          restartManager();

          BootstrapMonitor.checkAddonNotInstalled(ID1);
          Assert.equal(getUninstallReason(), ADDON_UPGRADE);
          Assert.equal(getUninstallNewVersion(), 4);
          BootstrapMonitor.checkAddonNotStarted(ID1);

          AddonManager.getAddonByID(ID1, function(b1_2) {
            Assert.notEqual(b1_2, null);
            Assert.equal(b1_2.version, "4.0");
            Assert.ok(b1_2.isActive);
            Assert.ok(!b1_2.isSystem);
            Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);

            do_check_bootstrappedPref(run_test_26);
          });
        }));
      });
  });
  installAllFiles([do_get_addon("test_bootstrap1_1")], function test_25_installed() {
    info("test 25 install done");
  });
}

// Tests that updating from a normal add-on to a bootstrappable add-on calls
// the install method
function run_test_26() {
  installAllFiles([do_get_addon("test_bootstrap1_1")], function() {
    // Needs a restart to complete this
    BootstrapMonitor.checkAddonNotInstalled(ID1);
    BootstrapMonitor.checkAddonNotStarted(ID1);

    AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "4.0");
      Assert.ok(b1.isActive);
      Assert.ok(!b1.isSystem);
      Assert.ok(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));

      restartManager();

      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      Assert.equal(getInstallReason(), ADDON_DOWNGRADE);
      Assert.equal(getInstallOldVersion(), 4);
      BootstrapMonitor.checkAddonStarted(ID1, "1.0");

      AddonManager.getAddonByID(ID1, function(b1_2) {
        Assert.notEqual(b1_2, null);
        Assert.equal(b1_2.version, "1.0");
        Assert.ok(b1_2.isActive);
        Assert.ok(!b1_2.isSystem);
        Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);

        do_check_bootstrappedPref(run_test_27);
      });
    }));
  });
}

// Tests that updating from a bootstrappable add-on to a normal add-on while
// disabled calls the uninstall method
function run_test_27() {
  AddonManager.getAddonByID(ID1, function(b1) {
    Assert.notEqual(b1, null);
    b1.userDisabled = true;
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.isActive);
    Assert.equal(b1.pendingOperations, AddonManager.PENDING_NONE);
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    BootstrapMonitor.checkAddonNotStarted(ID1);

    BootstrapMonitor.restartfulIds.add(ID1);

    installAllFiles([do_get_addon("test_bootstrap1_4")], function() {
      // Updating disabled things happens immediately
      BootstrapMonitor.checkAddonNotInstalled(ID1);
      Assert.equal(getUninstallReason(), ADDON_UPGRADE);
      Assert.equal(getUninstallNewVersion(), 4);
      BootstrapMonitor.checkAddonNotStarted(ID1);

      AddonManager.getAddonByID(ID1, callback_soon(function(b1_2) {
        Assert.notEqual(b1_2, null);
        Assert.equal(b1_2.version, "4.0");
        Assert.ok(!b1_2.isActive);
        Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);

        restartManager();

        BootstrapMonitor.checkAddonNotInstalled(ID1);
        BootstrapMonitor.checkAddonNotStarted(ID1);

        AddonManager.getAddonByID(ID1, function(b1_3) {
          Assert.notEqual(b1_3, null);
          Assert.equal(b1_3.version, "4.0");
          Assert.ok(!b1_3.isActive);
          Assert.equal(b1_3.pendingOperations, AddonManager.PENDING_NONE);

          do_check_bootstrappedPref(run_test_28);
        });
      }));
    });
  });
}

// Tests that updating from a normal add-on to a bootstrappable add-on when
// disabled calls the install method but not the startup method
function run_test_28() {
  installAllFiles([do_get_addon("test_bootstrap1_1")], function() {
   executeSoon(function bootstrap_disabled_downgrade_check() {
    // Doesn't need a restart to complete this
    BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
    Assert.equal(getInstallReason(), ADDON_DOWNGRADE);
    Assert.equal(getInstallOldVersion(), 4);
    BootstrapMonitor.checkAddonNotStarted(ID1);

    AddonManager.getAddonByID(ID1, callback_soon(function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.ok(!b1.isActive);
      Assert.ok(b1.userDisabled);
      Assert.equal(b1.pendingOperations, AddonManager.PENDING_NONE);

      restartManager();

      BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
      BootstrapMonitor.checkAddonNotStarted(ID1);

      AddonManager.getAddonByID(ID1, function(b1_2) {
        Assert.notEqual(b1_2, null);
        Assert.ok(b1_2.userDisabled);
        b1_2.userDisabled = false;
        Assert.equal(b1_2.version, "1.0");
        Assert.ok(b1_2.isActive);
        Assert.ok(!b1_2.isSystem);
        Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);
        BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
        BootstrapMonitor.checkAddonStarted(ID1, "1.0");

        do_check_bootstrappedPref(do_test_finished);
      });
    }));
   });
  });
}
