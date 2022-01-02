/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const ID = "webextension1@tests.mozilla.org";

const ADDONS = {
  webextension_1: {
    "manifest.json": {
      name: "Web Extension Name",
      version: "1.0",
      manifest_version: 2,
      applications: {
        gecko: {
          id: ID,
        },
      },
      icons: {
        "48": "icon48.png",
        "64": "icon64.png",
      },
    },
    "chrome.manifest": "content webex ./\n",
  },
};

async function tearDownAddon(addon) {
  await addon.uninstall();
  await promiseShutdownManager();
}

add_task(async function test_reloading_a_temp_addon() {
  await promiseRestartManager();
  let xpi = AddonTestUtils.createTempXPIFile(ADDONS.webextension_1);
  const addon = await AddonManager.installTemporaryAddon(xpi);

  var receivedOnUninstalled = false;
  var receivedOnUninstalling = false;
  var receivedOnInstalled = false;
  var receivedOnInstalling = false;

  const onReload = new Promise(resolve => {
    const listener = {
      onUninstalling: addonObj => {
        if (addonObj.id === ID) {
          receivedOnUninstalling = true;
        }
      },
      onUninstalled: addonObj => {
        if (addonObj.id === ID) {
          receivedOnUninstalled = true;
        }
      },
      onInstalling: addonObj => {
        receivedOnInstalling = true;
        equal(addonObj.id, ID);
      },
      onInstalled: addonObj => {
        receivedOnInstalled = true;
        equal(addonObj.id, ID);
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
  equal(
    receivedOnUninstalled,
    false,
    "reload should not trigger onUninstalled"
  );
  equal(
    receivedOnUninstalling,
    false,
    "reload should not trigger onUninstalling"
  );

  // Make sure reload() triggers install events, like an upgrade.
  equal(receivedOnInstalling, true, "reload should trigger onInstalling");
  equal(receivedOnInstalled, true, "reload should trigger onInstalled");

  await tearDownAddon(addon);
});

add_task(async function test_can_reload_permanent_addon() {
  await promiseRestartManager();
  const { addon } = await AddonTestUtils.promiseInstallXPI(
    ADDONS.webextension_1
  );

  let disabledCalled = false;
  let enabledCalled = false;
  AddonManager.addAddonListener({
    onDisabled: aAddon => {
      Assert.ok(!enabledCalled);
      disabledCalled = true;
    },
    onEnabled: aAddon => {
      Assert.ok(disabledCalled);
      enabledCalled = true;
    },
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
      },
    },
  };

  let addonDir = await promiseWriteWebManifestForExtension(
    manifest,
    tempdir,
    "invalid_version"
  );
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

  addonDir = await promiseWriteWebManifestForExtension(
    manifest,
    tempdir,
    "invalid_version",
    false
  );
  let expectedMsg = new RegExp(
    "Add-on invalid_version_cannot_be_reloaded@tests.mozilla.org is not compatible with application version. " +
      "add-on minVersion: 1. add-on maxVersion: 1."
  );

  await Assert.rejects(
    addon.reload(),
    expectedMsg,
    "Reload rejects when application version does not fall between minVersion and maxVersion"
  );

  let reloadedAddon = await promiseAddonByID(addonId);
  notEqual(reloadedAddon, null);
  equal(reloadedAddon.id, addonId);
  equal(reloadedAddon.version, "1.0");
  equal(reloadedAddon.appDisabled, false);
  equal(reloadedAddon.userDisabled, false);

  await tearDownAddon(reloadedAddon);
  addonDir.remove(true);
});
