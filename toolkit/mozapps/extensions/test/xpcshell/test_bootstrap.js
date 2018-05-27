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
ChromeUtils.import("resource://gre/modules/Services.jsm");

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


const ADDONS = {
  test_bootstrap1_1: {
    "install.rdf": {
      id: "bootstrap1@tests.mozilla.org",

      name: "Test Bootstrap 1",

      iconURL: "chrome://foo/skin/icon.png",
      aboutURL: "chrome://foo/content/about.xul",
      optionsURL: "chrome://foo/content/options.xul",
    },
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
  },
  test_bootstrap1_2: {
    "install.rdf": {
      id: "bootstrap1@tests.mozilla.org",
      version: "2.0",

      name: "Test Bootstrap 1",
    },
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
  },
  test_bootstrap1_3: {
    "install.rdf": {
      id: "bootstrap1@tests.mozilla.org",
      version: "3.0",

      name: "Test Bootstrap 1",

      targetApplications: [{
        id: "undefined",
        minVersion: "1",
        maxVersion: "1"}],
    },
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
  },
  test_bootstrap2_1: {
    "install.rdf": {
      id: "bootstrap2@tests.mozilla.org",
    },
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
  },
};

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

const XPIS = {};
for (let [name, addon] of Object.entries(ADDONS)) {
  XPIS[name] = AddonTestUtils.createTempXPIFile(addon);
  testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
}


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

async function checkBootstrappedPref() {
  let XPIScope = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});

  let data = new Map();
  for (let entry of XPIScope.XPIStates.enabledAddons()) {
    data.set(entry.id, entry);
  }

  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  for (let addon of addons) {
    if (!addon.id.endsWith("@tests.mozilla.org"))
      continue;
    if (!addon.isActive)
      continue;
    if (addon.operationsRequiringRestart != AddonManager.OP_NEEDS_RESTART_NONE)
      continue;

    ok(data.has(addon.id));
    let addonData = data.get(addon.id);
    data.delete(addon.id);

    equal(addonData.version, addon.version);
    equal(addonData.type, addon.type);
    let file = addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
    equal(addonData.path, file.path);
  }
  equal(data.size, 0);
}


add_task(async function run_test() {
  promiseStartupManager();

  ok(!gExtensionsJSON.exists());
  ok(!gAddonStartup.exists());
});

// Tests that installing doesn't require a restart
add_task(async function test_1() {
  prepare_test({}, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_1);
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "extension");
  equal(install.version, "1.0");
  equal(install.name, "Test Bootstrap 1");
  equal(install.state, AddonManager.STATE_DOWNLOADED);
  notEqual(install.addon.syncGUID, null);
  equal(install.addon.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
  do_check_not_in_crash_annotation(ID1, "1.0");

  let addon = install.addon;

  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    new Promise(resolve => {
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() {
        // startup should not have been called yet.
        BootstrapMonitor.checkAddonNotStarted(ID1);
        resolve();
      });
      install.install();
    }),
  ]);

  await checkBootstrappedPref();
  let installSyncGUID = addon.syncGUID;

  let installs = await AddonManager.getAllInstalls();
  // There should be no active installs now since the install completed and
  // doesn't require a restart.
  equal(installs.length, 0);

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  notEqual(b1.syncGUID, null);
  equal(b1.syncGUID, installSyncGUID);
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_INSTALL);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  let dir = do_get_addon_root_uri(profileDir, ID1);
  equal(b1.getResourceURI("bootstrap.js").spec, dir + "bootstrap.js");
});

// Tests that disabling doesn't require a restart
add_task(async function test_2() {
  let b1 = await AddonManager.getAddonByID(ID1);
  prepare_test({
    [ID1]: [
      ["onDisabling", false],
      "onDisabled"
    ]
  });

  equal(b1.operationsRequiringRestart &
        AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
  await b1.disable();
  ensure_test_completed();

  await new Promise(executeSoon);

  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(b1.userDisabled);
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), ADDON_DISABLE);
  equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");

  let newb1 = await AddonManager.getAddonByID(ID1);
  notEqual(newb1, null);
  equal(newb1.version, "1.0");
  ok(!newb1.appDisabled);
  ok(newb1.userDisabled);
  ok(!newb1.isActive);

  await checkBootstrappedPref();
});

// Test that restarting doesn't accidentally re-enable
add_task(async function test_3() {
  await promiseShutdownManager();

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), ADDON_DISABLE);
  equal(getShutdownNewVersion(), undefined);

  await promiseStartupManager();

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), ADDON_DISABLE);
  equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");

  ok(gAddonStartup.exists());

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(b1.userDisabled);
  ok(!b1.isActive);

  await checkBootstrappedPref();
});

// Tests that enabling doesn't require a restart
add_task(async function test_4() {
  let b1 = await AddonManager.getAddonByID(ID1);
  prepare_test({
    [ID1]: [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  equal(b1.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
  await b1.enable();
  ensure_test_completed();

  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_ENABLE);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  let newb1 = await AddonManager.getAddonByID(ID1);
  notEqual(newb1, null);
  equal(newb1.version, "1.0");
  ok(!newb1.appDisabled);
  ok(!newb1.userDisabled);
  ok(newb1.isActive);

  await checkBootstrappedPref();
});

// Tests that a restart shuts down and restarts the add-on
add_task(async function test_5() {
  await promiseShutdownManager();
  // By the time we've shut down, the database must have been written
  ok(gExtensionsJSON.exists());

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), APP_SHUTDOWN);
  equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");
  await promiseStartupManager();
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), APP_STARTUP);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);

  await checkBootstrappedPref();
});

// Tests that installing an upgrade doesn't require a restart
add_task(async function test_6() {
  prepare_test({}, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_2);
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "extension");
  equal(install.version, "2.0");
  equal(install.name, "Test Bootstrap 1");
  equal(install.state, AddonManager.STATE_DOWNLOADED);

  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    new Promise(resolve => {
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], resolve);
      install.install();
    }),
  ]);

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "2.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonStarted(ID1, "2.0");
  equal(getStartupReason(), ADDON_UPGRADE);
  equal(getInstallOldVersion(), 1);
  equal(getStartupOldVersion(), 1);
  equal(getShutdownReason(), ADDON_UPGRADE);
  equal(getShutdownNewVersion(), 2);
  equal(getUninstallNewVersion(), 2);
  do_check_not_in_crash_annotation(ID1, "1.0");
  do_check_in_crash_annotation(ID1, "2.0");

  await checkBootstrappedPref();
});

// Tests that uninstalling doesn't require a restart
add_task(async function test_7() {
  let b1 = await AddonManager.getAddonByID(ID1);
  prepare_test({
    [ID1]: [
      ["onUninstalling", false],
      "onUninstalled"
    ]
  });

  equal(b1.operationsRequiringRestart &
        AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
  await b1.uninstall();

  await checkBootstrappedPref();

  ensure_test_completed();
  BootstrapMonitor.checkAddonNotInstalled(ID1);
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), ADDON_UNINSTALL);
  equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "2.0");

  b1 = await AddonManager.getAddonByID(ID1);
  equal(b1, null);

  await promiseRestartManager();

  let newb1 = await AddonManager.getAddonByID(ID1);
  equal(newb1, null);

  await checkBootstrappedPref();
});

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
add_task(async function test_8() {
  await promiseShutdownManager();

  await manuallyInstall(XPIS.test_bootstrap1_1, profileDir, ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_INSTALL);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  await checkBootstrappedPref();
});

// Test that items detected as removed during startup get removed properly
add_task(async function test_9() {
  await promiseShutdownManager();

  manuallyUninstall(profileDir, ID1);
  BootstrapMonitor.clear(ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  equal(b1, null);
  do_check_not_in_crash_annotation(ID1, "1.0");

  await checkBootstrappedPref();
});


// Tests that installing a downgrade sends the right reason
add_task(async function test_10() {
  prepare_test({}, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_2);
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "extension");
  equal(install.version, "2.0");
  equal(install.name, "Test Bootstrap 1");
  equal(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_not_in_crash_annotation(ID1, "2.0");

  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    new Promise(resolve => {
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], resolve);
      install.install();
    }),
  ]);


  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "2.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonStarted(ID1, "2.0");
  equal(getStartupReason(), ADDON_INSTALL);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "2.0");

  prepare_test({}, [
    "onNewInstall"
  ]);

  install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_1);
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "extension");
  equal(install.version, "1.0");
  equal(install.name, "Test Bootstrap 1");
  equal(install.state, AddonManager.STATE_DOWNLOADED);

  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    new Promise(resolve => {
      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], resolve);
      install.install();
    }),
  ]);

  b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_DOWNGRADE);
  equal(getInstallOldVersion(), 2);
  equal(getStartupOldVersion(), 2);
  equal(getShutdownReason(), ADDON_DOWNGRADE);
  equal(getShutdownNewVersion(), 1);
  equal(getUninstallNewVersion(), 1);
  do_check_in_crash_annotation(ID1, "1.0");
  do_check_not_in_crash_annotation(ID1, "2.0");

  await checkBootstrappedPref();
});

// Tests that uninstalling a disabled add-on still calls the uninstall method
add_task(async function test_11() {
  let b1 = await AddonManager.getAddonByID(ID1);
  prepare_test({
    [ID1]: [
      ["onDisabling", false],
      "onDisabled",
      ["onUninstalling", false],
      "onUninstalled"
    ]
  });

  await b1.disable();

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  equal(getShutdownReason(), ADDON_DISABLE);
  equal(getShutdownNewVersion(), undefined);
  do_check_not_in_crash_annotation(ID1, "1.0");

  await b1.uninstall();

  ensure_test_completed();
  BootstrapMonitor.checkAddonNotInstalled(ID1);
  BootstrapMonitor.checkAddonNotStarted(ID1);
  do_check_not_in_crash_annotation(ID1, "1.0");

  await checkBootstrappedPref();
});

// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
add_task(async function test_12() {
  await promiseShutdownManager();

  await manuallyInstall(XPIS.test_bootstrap1_1, profileDir, ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_INSTALL);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  await b1.uninstall();

  await promiseRestartManager();
  await checkBootstrappedPref();
});


// Tests that installing a bootstrapped extension with an invalid application
// entry doesn't call it's startup method
add_task(async function test_13() {
  prepare_test({}, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_3);
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "extension");
  equal(install.version, "3.0");
  equal(install.name, "Test Bootstrap 1");
  equal(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_not_in_crash_annotation(ID1, "3.0");

  await new Promise(resolve => {
    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  let installs = await AddonManager.getAllInstalls();

  // There should be no active installs now since the install completed and
  // doesn't require a restart.
  equal(installs.length, 0);

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "3.0");
  ok(b1.appDisabled);
  ok(!b1.userDisabled);
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
  BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
  do_check_not_in_crash_annotation(ID1, "3.0");

  await promiseRestartManager();

  b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "3.0");
  ok(b1.appDisabled);
  ok(!b1.userDisabled);
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
  BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
  do_check_not_in_crash_annotation(ID1, "3.0");

  await checkBootstrappedPref();
  await b1.uninstall();
});

// Tests that a bootstrapped extension with an invalid target application entry
// does not get loaded when detected during startup
add_task(async function test_14() {
  await promiseRestartManager();

  await promiseShutdownManager();

  await manuallyInstall(XPIS.test_bootstrap1_3, profileDir, ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "3.0");
  ok(b1.appDisabled);
  ok(!b1.userDisabled);
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "3.0"); // We call install even for disabled add-ons
  BootstrapMonitor.checkAddonNotStarted(ID1); // Should not have called startup though
  do_check_not_in_crash_annotation(ID1, "3.0");

  await checkBootstrappedPref();
  await b1.uninstall();
});

// Tests that upgrading a disabled bootstrapped extension still calls uninstall
// and install but doesn't startup the new version
add_task(async function test_15() {
  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    promiseInstallFile(XPIS.test_bootstrap1_1),
  ]);

  let b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");

  await b1.disable();
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);

  prepare_test({}, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_2);
  ensure_test_completed();

  notEqual(install, null);
  ok(install.addon.userDisabled);

  await new Promise(resolve => {
    prepare_test({
      [ID1]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  b1 = await AddonManager.getAddonByID(ID1);
  notEqual(b1, null);
  equal(b1.version, "2.0");
  ok(!b1.appDisabled);
  ok(b1.userDisabled);
  ok(!b1.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);

  await checkBootstrappedPref();
  await promiseRestartManager();

  let b1_2 = await AddonManager.getAddonByID(ID1);
  notEqual(b1_2, null);
  equal(b1_2.version, "2.0");
  ok(!b1_2.appDisabled);
  ok(b1_2.userDisabled);
  ok(!b1_2.isActive);
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);

  await b1_2.uninstall();
});

// Tests that bootstrapped extensions don't get loaded when in safe mode
add_task(async function test_16() {
  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    promiseInstallFile(XPIS.test_bootstrap1_1),
  ]);

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  ok(b1.isActive);
  ok(!b1.isSystem);
  equal(b1.iconURL, "chrome://foo/skin/icon.png");
  equal(b1.aboutURL, "chrome://foo/content/about.xul");
  equal(b1.optionsURL, "chrome://foo/content/options.xul");

  await promiseShutdownManager();

  // Should have stopped
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);

  gAppInfo.inSafeMode = true;
  await promiseStartupManager();

  let b1_2 = await AddonManager.getAddonByID(ID1);
  // Should still be stopped
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID1);
  ok(!b1_2.isActive);
  equal(b1_2.iconURL, null);
  equal(b1_2.aboutURL, null);
  equal(b1_2.optionsURL, null);

  await promiseShutdownManager();
  gAppInfo.inSafeMode = false;
  await promiseStartupManager();

  // Should have started
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");

  let b1_3 = await AddonManager.getAddonByID(ID1);
  await b1_3.uninstall();
});

// Check that a bootstrapped extension in a non-profile location is loaded
add_task(async function test_17() {
  await promiseShutdownManager();

  await manuallyInstall(XPIS.test_bootstrap1_1, userExtDir, ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  await checkBootstrappedPref();
});

// Check that installing a new bootstrapped extension in the profile replaces
// the existing one
add_task(async function test_18() {
  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID1),
    promiseInstallFile(XPIS.test_bootstrap1_2),
  ]);

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonStarted(ID1, "2.0");
  notEqual(b1, null);
  equal(b1.version, "2.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  equal(getShutdownReason(), ADDON_UPGRADE);
  equal(getUninstallReason(), ADDON_UPGRADE);
  equal(getInstallReason(), ADDON_UPGRADE);
  equal(getStartupReason(), ADDON_UPGRADE);

  equal(getShutdownNewVersion(), 2);
  equal(getUninstallNewVersion(), 2);
  equal(getInstallOldVersion(), 1);
  equal(getStartupOldVersion(), 1);

  await checkBootstrappedPref();
});

// Check that uninstalling the profile version reveals the non-profile one
add_task(async function test_19() {
  let b1 = await AddonManager.getAddonByID(ID1);
  // The revealed add-on gets activated asynchronously
  await new Promise(resolve => {
    prepare_test({
      [ID1]: [
        ["onUninstalling", false],
        "onUninstalled",
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [], resolve);

    b1.uninstall();
  });

  b1 = await AddonManager.getAddonByID(ID1);
  // Should have reverted to the older version
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  equal(getShutdownReason(), ADDON_DOWNGRADE);
  equal(getUninstallReason(), ADDON_DOWNGRADE);
  equal(getInstallReason(), ADDON_DOWNGRADE);
  equal(getStartupReason(), ADDON_DOWNGRADE);

  equal(getShutdownNewVersion(), "1.0");
  equal(getUninstallNewVersion(), "1.0");
  equal(getInstallOldVersion(), "2.0");
  equal(getStartupOldVersion(), "2.0");

  await checkBootstrappedPref();
});

// Check that a new profile extension detected at startup replaces the non-profile
// one
add_task(async function test_20() {
  await promiseShutdownManager();

  await manuallyInstall(XPIS.test_bootstrap1_2, profileDir, ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonStarted(ID1, "2.0");
  notEqual(b1, null);
  equal(b1.version, "2.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  equal(getShutdownReason(), APP_SHUTDOWN);
  equal(getUninstallReason(), ADDON_UPGRADE);
  equal(getInstallReason(), ADDON_UPGRADE);
  equal(getStartupReason(), APP_STARTUP);

  equal(getShutdownNewVersion(), undefined);
  equal(getUninstallNewVersion(), 2);
  equal(getInstallOldVersion(), 1);
  equal(getStartupOldVersion(), undefined);
});

// Check that a detected removal reveals the non-profile one
add_task(async function test_21() {
  await promiseShutdownManager();

  equal(getShutdownReason(), APP_SHUTDOWN);
  equal(getShutdownNewVersion(), undefined);

  manuallyUninstall(profileDir, ID1);
  BootstrapMonitor.clear(ID1);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  // This won't be set as the bootstrap script was gone so we couldn't
  // uninstall it properly
  equal(getUninstallReason(), undefined);
  equal(getUninstallNewVersion(), undefined);

  equal(getInstallReason(), ADDON_DOWNGRADE);
  equal(getInstallOldVersion(), 2);

  equal(getStartupReason(), APP_STARTUP);
  equal(getStartupOldVersion(), undefined);

  await checkBootstrappedPref();
  await promiseShutdownManager();

  manuallyUninstall(userExtDir, ID1);
  BootstrapMonitor.clear(ID1);

  await promiseStartupManager();
});

// Check that an upgrade from the filesystem is detected and applied correctly
add_task(async function test_22() {
  await promiseShutdownManager();

  let file = await manuallyInstall(XPIS.test_bootstrap1_1, profileDir, ID1);
  if (file.isDirectory())
    file.append("install.rdf");

  // Make it look old so changes are detected
  setExtensionModifiedTime(file, file.lastModifiedTime - 5000);

  await promiseStartupManager();

  let b1 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(b1.isActive);
  ok(!b1.isSystem);

  await promiseShutdownManager();

  equal(getShutdownReason(), APP_SHUTDOWN);
  equal(getShutdownNewVersion(), undefined);

  manuallyUninstall(profileDir, ID1);
  BootstrapMonitor.clear(ID1);
  await manuallyInstall(XPIS.test_bootstrap1_2, profileDir, ID1);

  await promiseStartupManager();

  let b1_2 = await AddonManager.getAddonByID(ID1);
  // Should have installed and started
  BootstrapMonitor.checkAddonInstalled(ID1, "2.0");
  BootstrapMonitor.checkAddonStarted(ID1, "2.0");
  notEqual(b1_2, null);
  equal(b1_2.version, "2.0");
  ok(b1_2.isActive);
  ok(!b1_2.isSystem);

  // This won't be set as the bootstrap script was gone so we couldn't
  // uninstall it properly
  equal(getUninstallReason(), undefined);
  equal(getUninstallNewVersion(), undefined);

  equal(getInstallReason(), ADDON_UPGRADE);
  equal(getInstallOldVersion(), 1);
  equal(getStartupReason(), APP_STARTUP);
  equal(getStartupOldVersion(), undefined);

  await checkBootstrappedPref();
  await b1_2.uninstall();
});


// Tests that installing from a URL doesn't require a restart
add_task(async function test_23() {
  prepare_test({}, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_bootstrap1_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  ensure_test_completed();

  notEqual(install, null);

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], function() {
      equal(install.type, "extension");
      equal(install.version, "1.0");
      equal(install.name, "Test Bootstrap 1");
      equal(install.state, AddonManager.STATE_DOWNLOADED);
      equal(install.addon.operationsRequiringRestart &
                   AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
      do_check_not_in_crash_annotation(ID1, "1.0");

      prepare_test({
        [ID1]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], resolve);
    });
    install.install();
  });

  await checkBootstrappedPref();

  let installs = await AddonManager.getAllInstalls();

  // There should be no active installs now since the install completed and
  // doesn't require a restart.
  equal(installs.length, 0);

  let b1 = await AddonManager.getAddonByID(ID1);

  notEqual(b1, null);
  equal(b1.version, "1.0");
  ok(!b1.appDisabled);
  ok(!b1.userDisabled);
  ok(b1.isActive);
  ok(!b1.isSystem);
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  equal(getStartupReason(), ADDON_INSTALL);
  equal(getStartupOldVersion(), undefined);
  do_check_in_crash_annotation(ID1, "1.0");

  let dir = do_get_addon_root_uri(profileDir, ID1);
  equal(b1.getResourceURI("bootstrap.js").spec, dir + "bootstrap.js");

  await promiseRestartManager();

  let b1_2 = await AddonManager.getAddonByID(ID1);
  await b1_2.uninstall();
});

// Tests that we recover from a broken preference
add_task(async function test_24() {
  info("starting 24");

  await Promise.all([
    BootstrapMonitor.promiseAddonStartup(ID2),
    promiseInstallAllFiles([XPIS.test_bootstrap1_1, XPIS.test_bootstrap2_1]),
  ]);

  info("test 24 got prefs");
  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
  BootstrapMonitor.checkAddonStarted(ID2, "1.0");

  await promiseRestartManager();

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
  BootstrapMonitor.checkAddonStarted(ID2, "1.0");

  await promiseShutdownManager();

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

  await promiseStartupManager();

  BootstrapMonitor.checkAddonInstalled(ID1, "1.0");
  BootstrapMonitor.checkAddonStarted(ID1, "1.0");
  BootstrapMonitor.checkAddonInstalled(ID2, "1.0");
  BootstrapMonitor.checkAddonStarted(ID2, "1.0");
});
