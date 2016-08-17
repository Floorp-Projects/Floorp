/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const sampleAddon = {
  id: "webextension1@tests.mozilla.org",
  name: "webextension_1",
}

const manifestSample = {
  id: "bootstrap1@tests.mozilla.org",
  version: "1.0",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
};

function* installAddon(fixtureName, addonID) {
  yield promiseInstallAllFiles([do_get_addon(fixtureName)]);
  return promiseAddonByID(addonID);
}

function* tearDownAddon(addon) {
  addon.uninstall();
  yield promiseShutdownManager();
}

add_task(function* test_reloading_a_temp_addon() {
  yield promiseRestartManager();
  yield AddonManager.installTemporaryAddon(do_get_addon(sampleAddon.name));
  const addon = yield promiseAddonByID(sampleAddon.id)

  var receivedOnUninstalled = false;
  var receivedOnUninstalling = false;
  var receivedOnInstalled = false;
  var receivedOnInstalling = false;

  const onReload = new Promise(resolve => {
    const listener = {
      onUninstalling: (addon) => {
        if (addon.id === sampleAddon.id) {
          receivedOnUninstalling = true;
        }
      },
      onUninstalled: (addon) => {
        if (addon.id === sampleAddon.id) {
          receivedOnUninstalled = true;
        }
      },
      onInstalling: (addon) => {
        receivedOnInstalling = true;
        equal(addon.id, sampleAddon.id);
      },
      onInstalled: (addon) => {
        receivedOnInstalled = true;
        equal(addon.id, sampleAddon.id);
        // This should be the last event called.
        AddonManager.removeAddonListener(listener);
        resolve();
      },
    }
    AddonManager.addAddonListener(listener);
  });

  yield addon.reload();
  yield onReload;

  // Make sure reload() doesn't trigger uninstall events.
  equal(receivedOnUninstalled, false, "reload should not trigger onUninstalled");
  equal(receivedOnUninstalling, false, "reload should not trigger onUninstalling");

  // Make sure reload() triggers install events, like an upgrade.
  equal(receivedOnInstalling, true, "reload should trigger onInstalling");
  equal(receivedOnInstalled, true, "reload should trigger onInstalled");

  yield tearDownAddon(addon);
});

add_task(function* test_can_reload_permanent_addon() {
  yield promiseRestartManager();
  const addon = yield installAddon(sampleAddon.name, sampleAddon.id);

  let disabledCalled = false;
  let enabledCalled = false;
  AddonManager.addAddonListener({
    onDisabled: (aAddon) => {
      do_check_false(enabledCalled);
      disabledCalled = true
    },
    onEnabled: (aAddon) => {
      do_check_true(disabledCalled);
      enabledCalled = true
    }
  })

  yield addon.reload();

  do_check_true(disabledCalled);
  do_check_true(enabledCalled);

  notEqual(addon, null);
  equal(addon.appDisabled, false);
  equal(addon.userDisabled, false);

  yield tearDownAddon(addon);
});

add_task(function* test_disabled_addon_can_be_enabled_after_reload() {
  yield promiseRestartManager();
  let tempdir = gTmpD.clone();

  // Create an add-on with strictCompatibility which should cause it
  // to be appDisabled.
  const unpackedAddon = writeInstallRDFToDir(
    Object.assign({}, manifestSample, {
      strictCompatibility: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0.1",
        maxVersion: "0.1"
      }],
    }), tempdir, manifestSample.id, "bootstrap.js");

  yield AddonManager.installTemporaryAddon(unpackedAddon);
  const addon = yield promiseAddonByID(manifestSample.id);
  notEqual(addon, null);
  equal(addon.appDisabled, true);

  // Remove strictCompatibility from the manifest.
  writeInstallRDFToDir(manifestSample, tempdir, manifestSample.id);

  yield addon.reload();

  const reloadedAddon = yield promiseAddonByID(manifestSample.id);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.appDisabled, false);

  yield tearDownAddon(reloadedAddon);
  unpackedAddon.remove(true);
});

add_task(function* test_manifest_changes_are_refreshed() {
  yield promiseRestartManager();
  let tempdir = gTmpD.clone();

  const unpackedAddon = writeInstallRDFToDir(
    Object.assign({}, manifestSample, {
      name: "Test Bootstrap 1",
    }), tempdir, manifestSample.id, "bootstrap.js");

  yield AddonManager.installTemporaryAddon(unpackedAddon);
  const addon = yield promiseAddonByID(manifestSample.id);
  notEqual(addon, null);
  equal(addon.name, "Test Bootstrap 1");

  writeInstallRDFToDir(Object.assign({}, manifestSample, {
    name: "Test Bootstrap 1 (reloaded)",
  }), tempdir, manifestSample.id);

  yield addon.reload();

  const reloadedAddon = yield promiseAddonByID(manifestSample.id);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.name, "Test Bootstrap 1 (reloaded)");

  yield tearDownAddon(reloadedAddon);
  unpackedAddon.remove(true);
});

add_task(function* test_reload_fails_on_installation_errors() {
  yield promiseRestartManager();
  let tempdir = gTmpD.clone();

  const unpackedAddon = writeInstallRDFToDir(
    Object.assign({}, manifestSample, {
      name: "Test Bootstrap 1",
    }), tempdir, manifestSample.id, "bootstrap.js");

  yield AddonManager.installTemporaryAddon(unpackedAddon);
  const addon = yield promiseAddonByID(manifestSample.id);
  notEqual(addon, null);

  // Trigger an installation error with an empty manifest.
  writeInstallRDFToDir({}, tempdir, manifestSample.id);

  yield Assert.rejects(addon.reload(), /No ID in install manifest/);

  // The old add-on should be active. I.E. the broken reload will not
  // disturb it.
  const oldAddon = yield promiseAddonByID(manifestSample.id);
  notEqual(oldAddon, null);
  equal(oldAddon.isActive, true);
  equal(oldAddon.name, "Test Bootstrap 1");

  yield tearDownAddon(addon);
  unpackedAddon.remove(true);
});
