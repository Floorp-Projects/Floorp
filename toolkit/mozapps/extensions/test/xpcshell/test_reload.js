/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const sampleAddon = {
  id: "webextension1@tests.mozilla.org",
  name: "webextension_1",
};

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

async function installAddon(fixtureName, addonID) {
  await promiseInstallAllFiles([do_get_addon(fixtureName)]);
  return promiseAddonByID(addonID);
}

async function tearDownAddon(addon) {
  await addon.uninstall();
  await promiseShutdownManager();
}

add_task(async function test_reloading_a_temp_addon() {
  if (AppConstants.MOZ_APP_NAME == "thunderbird")
    return;
  await promiseRestartManager();
  await AddonManager.installTemporaryAddon(do_get_addon(sampleAddon.name));
  const addon = await promiseAddonByID(sampleAddon.id);

  var receivedOnUninstalled = false;
  var receivedOnUninstalling = false;
  var receivedOnInstalled = false;
  var receivedOnInstalling = false;

  const onReload = new Promise(resolve => {
    const listener = {
      onUninstalling: (addonObj) => {
        if (addonObj.id === sampleAddon.id) {
          receivedOnUninstalling = true;
        }
      },
      onUninstalled: (addonObj) => {
        if (addonObj.id === sampleAddon.id) {
          receivedOnUninstalled = true;
        }
      },
      onInstalling: (addonObj) => {
        receivedOnInstalling = true;
        equal(addonObj.id, sampleAddon.id);
      },
      onInstalled: (addonObj) => {
        receivedOnInstalled = true;
        equal(addonObj.id, sampleAddon.id);
        // This should be the last event called.
        AddonManager.removeAddonListener(listener);
        resolve();
      },
    };
    AddonManager.addAddonListener(listener);
  });

  await addon.reload();
  await onReload;

  // Make sure reload() doesn't trigger uninstall events.
  equal(receivedOnUninstalled, false, "reload should not trigger onUninstalled");
  equal(receivedOnUninstalling, false, "reload should not trigger onUninstalling");

  // Make sure reload() triggers install events, like an upgrade.
  equal(receivedOnInstalling, true, "reload should trigger onInstalling");
  equal(receivedOnInstalled, true, "reload should trigger onInstalled");

  await tearDownAddon(addon);
});

add_task(async function test_can_reload_permanent_addon() {
  await promiseRestartManager();
  const addon = await installAddon(sampleAddon.name, sampleAddon.id);

  let disabledCalled = false;
  let enabledCalled = false;
  AddonManager.addAddonListener({
    onDisabled: (aAddon) => {
      Assert.ok(!enabledCalled);
      disabledCalled = true;
    },
    onEnabled: (aAddon) => {
      Assert.ok(disabledCalled);
      enabledCalled = true;
    }
  });

  await addon.reload();

  Assert.ok(disabledCalled);
  Assert.ok(enabledCalled);

  notEqual(addon, null);
  equal(addon.appDisabled, false);
  equal(addon.userDisabled, false);

  await tearDownAddon(addon);
});

add_task(async function test_reload_to_invalid_version_fails() {
  await promiseRestartManager();
  let tempdir = gTmpD.clone();

  // The initial version of the add-on will be compatible, and will therefore load
  const addonId = "invalid_version_cannot_be_reloaded@tests.mozilla.org";
  let manifest = {
    name: "invalid_version_cannot_be_reloaded",
    description: "test invalid_version_cannot_be_reloaded",
    manifest_version: 2,
    version: "1.0",
    applications: {
      gecko: {
        id: addonId,
      }
    },
  };

  let addonDir = await promiseWriteWebManifestForExtension(manifest, tempdir, "invalid_version");
  await AddonManager.installTemporaryAddon(addonDir);

  let addon = await promiseAddonByID(addonId);
  notEqual(addon, null);
  equal(addon.id, addonId);
  equal(addon.version, "1.0");
  equal(addon.appDisabled, false);
  equal(addon.userDisabled, false);
  addonDir.remove(true);

  // update the manifest to make the add-on version incompatible, so the reload will reject
  manifest.applications.gecko.strict_min_version = "1";
  manifest.applications.gecko.strict_max_version = "1";
  manifest.version = "2.0";

  addonDir = await promiseWriteWebManifestForExtension(manifest, tempdir, "invalid_version", false);
  let expectedMsg = new RegExp("Add-on invalid_version_cannot_be_reloaded@tests.mozilla.org is not compatible with application version. " +
                               "add-on minVersion: 1. add-on maxVersion: 1.");

  await Assert.rejects(addon.reload(),
                       expectedMsg,
                       "Reload rejects when application version does not fall between minVersion and maxVersion");

  let reloadedAddon = await promiseAddonByID(addonId);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.id, addonId);
  equal(reloadedAddon.version, "1.0");
  equal(reloadedAddon.appDisabled, false);
  equal(reloadedAddon.userDisabled, false);

  await tearDownAddon(reloadedAddon);
  addonDir.remove(true);
});

add_task(async function test_manifest_changes_are_refreshed() {
  if (!AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
    return;
  }
  await promiseRestartManager();
  let tempdir = gTmpD.clone();

  const unpackedAddon = await promiseWriteInstallRDFToDir(
    Object.assign({}, manifestSample, {
      name: "Test Bootstrap 1",
    }), tempdir, manifestSample.id, "bootstrap.js");

  await AddonManager.installTemporaryAddon(unpackedAddon);
  const addon = await promiseAddonByID(manifestSample.id);
  notEqual(addon, null);
  equal(addon.name, "Test Bootstrap 1");

  await promiseWriteInstallRDFToDir(Object.assign({}, manifestSample, {
    name: "Test Bootstrap 1 (reloaded)",
  }), tempdir, manifestSample.id);

  await addon.reload();

  const reloadedAddon = await promiseAddonByID(manifestSample.id);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.name, "Test Bootstrap 1 (reloaded)");

  await tearDownAddon(reloadedAddon);
  unpackedAddon.remove(true);
});

add_task(async function test_reload_fails_on_installation_errors() {
  if (!AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
    return;
  }
  await promiseRestartManager();
  let tempdir = gTmpD.clone();

  const unpackedAddon = await promiseWriteInstallRDFToDir(
    Object.assign({}, manifestSample, {
      name: "Test Bootstrap 1",
    }), tempdir, manifestSample.id, "bootstrap.js");

  await AddonManager.installTemporaryAddon(unpackedAddon);
  const addon = await promiseAddonByID(manifestSample.id);
  notEqual(addon, null);

  // Trigger an installation error with an empty manifest.
  await promiseWriteInstallRDFToDir({}, tempdir, manifestSample.id);

  await Assert.rejects(addon.reload(), /No ID in install manifest/);

  // The old add-on should be active. I.E. the broken reload will not
  // disturb it.
  const oldAddon = await promiseAddonByID(manifestSample.id);
  notEqual(oldAddon, null);
  equal(oldAddon.isActive, true);
  equal(oldAddon.name, "Test Bootstrap 1");

  await tearDownAddon(addon);
  unpackedAddon.remove(true);
});
