/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ID = "experiment1@tests.mozilla.org";

var gIsNightly = false;

function getXS() {
  let XPI = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
  return XPI.XPIStates;
}

function getBootstrappedAddons() {
  let obj = {};
  for (let addon of getXS().bootstrappedAddons()) {
    obj[addon.id] = addon;
  }
  return obj;
}

function run_test() {
  BootstrapMonitor.init();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  gIsNightly = isNightlyChannel();

  run_next_test();
}

add_task(async function test_experiment() {
  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  await promiseInstallAllFiles([do_get_addon("test_experiment1")]);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID);

  let addon = await promiseAddonByID(ID);
  Assert.ok(addon, "Addon is found.");

  Assert.ok(addon.userDisabled, "Experiments are userDisabled by default.");
  Assert.ok(!addon.appDisabled, "Experiments are not appDisabled by compatibility.");
  Assert.equal(addon.isActive, false, "Add-on is not active.");
  Assert.equal(addon.updateURL, null, "No updateURL for experiments.");
  Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE,
               "Background updates are disabled.");
  Assert.equal(addon.permissions, AddonManager.PERM_CAN_UNINSTALL + AddonManager.PERM_CAN_ENABLE,
               "Permissions are minimal.");
  Assert.ok(!(addon.pendingOperations & AddonManager.PENDING_ENABLE),
            "Should not be pending enable");
  Assert.ok(!(addon.pendingOperations & AddonManager.PENDING_DISABLE),
            "Should not be pending disable");

  // Setting applyBackgroundUpdates should not work.
  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
  Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE,
               "Setting applyBackgroundUpdates shouldn't do anything.");

  let noCompatibleCalled = false;
  let noUpdateCalled = false;
  let finishedCalled = false;

  let listener = {
    onNoCompatibilityUpdateAvailable: () => { noCompatibleCalled = true; },
    onNoUpdateAvailable: () => { noUpdateCalled = true; },
    onUpdateFinished: () => { finishedCalled = true; },
  };

  addon.findUpdates(listener, "testing", null, null);
  Assert.ok(noCompatibleCalled, "Listener called.");
  Assert.ok(noUpdateCalled, "Listener called.");
  Assert.ok(finishedCalled, "Listener called.");
});

// Changes to userDisabled should not be persisted to the database.
add_task(async function test_userDisabledNotPersisted() {
  let addon = await promiseAddonByID(ID);
  Assert.ok(addon, "Add-on is found.");
  Assert.ok(addon.userDisabled, "Add-on is user disabled.");

  let promise = promiseAddonEvent("onEnabled");
  addon.userDisabled = false;
  let [addon2] = await promise;

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  Assert.equal(addon2.id, addon.id, "Changed add-on matches expected.");
  Assert.equal(addon2.userDisabled, false, "Add-on is no longer user disabled.");
  Assert.ok(addon2.isActive, "Add-on is active.");

  Assert.ok(ID in getBootstrappedAddons(),
            "Experiment add-on listed in XPIProvider bootstrapped list.");

  addon = await promiseAddonByID(ID);
  Assert.ok(addon, "Add-on retrieved.");
  Assert.equal(addon.userDisabled, false, "Add-on is still enabled after API retrieve.");
  Assert.ok(addon.isActive, "Add-on is still active.");
  Assert.ok(!(addon.pendingOperations & AddonManager.PENDING_ENABLE),
            "Should not be pending enable");
  Assert.ok(!(addon.pendingOperations & AddonManager.PENDING_DISABLE),
            "Should not be pending disable");

  // Now when we restart the manager the add-on should revert state.
  await promiseRestartManager();

  Assert.ok(!(ID in getBootstrappedAddons()),
            "Experiment add-on not persisted to bootstrappedAddons.");

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID);

  addon = await promiseAddonByID(ID);
  Assert.ok(addon, "Add-on retrieved.");
  Assert.ok(addon.userDisabled, "Add-on is disabled after restart.");
  Assert.equal(addon.isActive, false, "Add-on is not active after restart.");
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
});

add_task(async function test_checkCompatibility() {
  if (gIsNightly)
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", false);
  else
    Services.prefs.setBoolPref("extensions.checkCompatibility.1", false);

  await promiseRestartManager();

  await promiseInstallAllFiles([do_get_addon("test_experiment1")]);
  let addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID);

  Assert.ok(addon, "Add-on is found.");
  Assert.ok(addon.userDisabled, "Add-on is user disabled.");
  Assert.ok(!addon.appDisabled, "Add-on is not app disabled.");
});
