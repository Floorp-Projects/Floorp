/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that forcing undo for uninstall works

const APP_STARTUP                     = 1;
const APP_SHUTDOWN                    = 2;
const ADDON_DISABLE                   = 4;
const ADDON_INSTALL                   = 5;
const ADDON_UNINSTALL                 = 6;
const ADDON_DOWNGRADE                 = 8;

const ID = "undouninstall1@tests.mozilla.org";
const INCOMPAT_ID = "incompatible@tests.mozilla.org";

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};


const profileDir = gProfD.clone();
profileDir.append("extensions");

BootstrapMonitor.init();

function getStartupReason(id) {
  let info = BootstrapMonitor.started.get(id);
  return info ? info.reason : undefined;
}

function getShutdownReason(id) {
  let info = BootstrapMonitor.stopped.get(id);
  return info ? info.reason : undefined;
}

function getInstallReason(id) {
  let info = BootstrapMonitor.installed.get(id);
  return info ? info.reason : undefined;
}

function getUninstallReason(id) {
  let info = BootstrapMonitor.uninstalled.get(id);
  return info ? info.reason : undefined;
}

function getShutdownNewVersion(id) {
  let info = BootstrapMonitor.stopped.get(id);
  return info ? info.data.newVersion : undefined;
}

// Sets up the profile by installing an add-on.
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  registerCleanupFunction(promiseShutdownManager);

  run_next_test();
}

add_task(async function installAddon() {
  let olda1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.equal(olda1, null);

  writeInstallRDFForExtension(addon1, profileDir);
  await promiseRestartManager();

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
  Assert.equal(a1.pendingOperations, 0);
  do_check_in_crash_annotation(addon1.id, addon1.version);
});

// Uninstalling an add-on should work.
add_task(async function uninstallAddon() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.equal(a1.pendingOperations, 0);
  Assert.notEqual(a1.operationsRequiringRestart &
                  AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
  a1.uninstall(true);
  Assert.ok(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));
  do_check_in_crash_annotation(addon1.id, addon1.version);

  ensure_test_completed();

  let list = await promiseAddonsWithOperationsByTypes(null);

  Assert.equal(list.length, 1);
  Assert.equal(list[0].id, "addon1@tests.mozilla.org");

  await promiseRestartManager();

  a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.equal(a1, null);
  Assert.ok(!isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));
  do_check_not_in_crash_annotation(addon1.id, addon1.version);

  var dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  Assert.ok(!dest.exists());
  writeInstallRDFForExtension(addon1, profileDir);
  await promiseRestartManager();
});

// Cancelling the uninstall should send onOperationCancelled
add_task(async function cancelUninstall() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
  Assert.equal(a1.pendingOperations, 0);
  a1.uninstall(true);
  Assert.ok(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  ensure_test_completed();

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  Assert.equal(a1.pendingOperations, 0);

  ensure_test_completed();
  await promiseRestartManager();

  a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
});

// Uninstalling an item pending disable should still require a restart
add_task(async function pendingDisableRequestRestart() {
  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onDisabling"
    ]
  });
  a1.userDisabled = true;
  ensure_test_completed();

  Assert.ok(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));
  Assert.ok(a1.isActive);

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);

  ensure_test_completed();

  a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();
  Assert.ok(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));

  await promiseRestartManager();
});

// Test that uninstalling an inactive item should still allow cancelling
add_task(async function uninstallInactiveIsCancellable() {
  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);
  Assert.ok(!isExtensionInAddonsList(profileDir, a1.id));

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();

  await promiseRestartManager();
});

// Test that an inactive item can be uninstalled
add_task(async function uninstallInactive() {
  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);
  Assert.ok(!isExtensionInAddonsList(profileDir, a1.id));

  prepare_test({
    "addon1@tests.mozilla.org": [
      [ "onUninstalling", false ],
      "onUninstalled"
    ]
  });
  a1.uninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.equal(a1, null);
});

// Tests that an enabled restartless add-on can be uninstalled and goes away
// when the uninstall is committed
add_task(async function uninstallRestartless() {
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);
  ensure_test_completed();

  let a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  // complete the uinstall
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalled"
    ]
  });
  a1.uninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID(ID);

  Assert.equal(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
});

// Tests that an enabled restartless add-on can be uninstalled and then cancelled
add_task(async function cancelUninstallOfRestartless() {
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);
  ensure_test_completed();

  let a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  shutdownManager();

  Assert.equal(getShutdownReason(ID), APP_SHUTDOWN);
  Assert.equal(getShutdownNewVersion(ID), undefined);

  startupManager(false);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), APP_STARTUP);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  a1.uninstall();
});

// Tests that reinstalling an enabled restartless add-on waiting to be
// uninstalled aborts the uninstall and leaves the add-on enabled
add_task(async function reinstallAddonAwaitingUninstall() {
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);

  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  ensure_test_completed();

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getUninstallReason(ID), ADDON_DOWNGRADE);
  Assert.equal(getInstallReason(ID), ADDON_DOWNGRADE);
  Assert.equal(getStartupReason(ID), ADDON_DOWNGRADE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  shutdownManager();

  Assert.equal(getShutdownReason(ID), APP_SHUTDOWN);

  startupManager(false);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), APP_STARTUP);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  a1.uninstall();
});

// Tests that a disabled restartless add-on can be uninstalled and goes away
// when the uninstall is committed
add_task(async function uninstallDisabledRestartless() {
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  a1.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  // commit the uninstall
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalled"
    ]
  });
  a1.uninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.equal(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);
  Assert.equal(getUninstallReason(ID), ADDON_UNINSTALL);
});

// Tests that a disabled restartless add-on can be uninstalled and then cancelled
add_task(async function cancelUninstallDisabledRestartless() {
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);
  ensure_test_completed();

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled"
    ]
  });
  a1.userDisabled = true;
  ensure_test_completed();

  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonInstalled(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();

  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonInstalled(ID);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await promiseRestartManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonInstalled(ID);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  a1.uninstall();
});

// Tests that reinstalling a disabled restartless add-on waiting to be
// uninstalled aborts the uninstall and leaves the add-on disabled
add_task(async function reinstallDisabledAddonAwaitingUninstall() {
  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  a1.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);

  await promiseInstallAllFiles([do_get_addon("test_undouninstall1")]);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  ensure_test_completed();

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID, "1.0");
  Assert.equal(getUninstallReason(ID), ADDON_DOWNGRADE);
  Assert.equal(getInstallReason(ID), ADDON_DOWNGRADE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await promiseRestartManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(ID, "1.0");
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  a1.uninstall();
});


// Test that uninstalling a temporary addon can be canceled
add_task(async function cancelUninstallTemporary() {
  await AddonManager.installTemporaryAddon(do_get_addon("test_undouninstall1"));

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  BootstrapMonitor.checkAddonNotStarted(ID, "1.0");
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");
  Assert.equal(a1.pendingOperations, 0);

  await promiseRestartManager();
});

// Tests that cancelling the uninstall of an incompatible restartless addon
// does not start the addon
add_task(async function cancelUninstallIncompatibleRestartless() {
  await promiseInstallAllFiles([do_get_addon("test_undoincompatible")]);

  let a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(INCOMPAT_ID);
  Assert.ok(!a1.isActive);

  prepare_test({
    "incompatible@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);

  prepare_test({
    "incompatible@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  a1.cancelUninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  BootstrapMonitor.checkAddonNotStarted(INCOMPAT_ID);
  Assert.equal(a1.pendingOperations, 0);
  Assert.ok(!a1.isActive);
});
