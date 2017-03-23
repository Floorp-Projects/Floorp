/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
Components.utils.import("resource://gre/modules/AppConstants.jsm");

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

const { Management } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

function promiseAddonStartup() {
  return new Promise(resolve => {
    let listener = (extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

function* installAddon(fixtureName, addonID) {
  yield promiseInstallAllFiles([do_get_addon(fixtureName)]);
  return promiseAddonByID(addonID);
}

function* tearDownAddon(addon) {
  addon.uninstall();
  yield promiseShutdownManager();
}

add_task(function* test_reloading_a_temp_addon() {
  if (AppConstants.MOZ_APP_NAME == "thunderbird")
    return;
  yield promiseRestartManager();
  yield AddonManager.installTemporaryAddon(do_get_addon(sampleAddon.name));
  const addon = yield promiseAddonByID(sampleAddon.id)

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
    }
    AddonManager.addAddonListener(listener);
  });

  yield Promise.all([
    addon.reload(),
    promiseAddonStartup(),
  ]);
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

  yield Promise.all([
    addon.reload(),
    promiseAddonStartup(),
  ]);

  do_check_true(disabledCalled);
  do_check_true(enabledCalled);

  notEqual(addon, null);
  equal(addon.appDisabled, false);
  equal(addon.userDisabled, false);

  yield tearDownAddon(addon);
});

add_task(function* test_reload_to_invalid_version_fails() {
  yield promiseRestartManager();
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

  let addonDir = yield promiseWriteWebManifestForExtension(manifest, tempdir, "invalid_version");
  yield AddonManager.installTemporaryAddon(addonDir);
  yield promiseAddonStartup();

  let addon = yield promiseAddonByID(addonId);
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

  addonDir = yield promiseWriteWebManifestForExtension(manifest, tempdir, "invalid_version", false);
  let expectedMsg = new RegExp("Add-on invalid_version_cannot_be_reloaded@tests.mozilla.org is not compatible with application version. " +
                               "add-on minVersion: 1. add-on maxVersion: 1.");

  yield Assert.rejects(addon.reload(),
                       expectedMsg,
                       "Reload rejects when application version does not fall between minVersion and maxVersion");

  let reloadedAddon = yield promiseAddonByID(addonId);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.id, addonId);
  equal(reloadedAddon.version, "1.0");
  equal(reloadedAddon.appDisabled, false);
  equal(reloadedAddon.userDisabled, false);

  yield tearDownAddon(reloadedAddon);
  addonDir.remove(true);
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
