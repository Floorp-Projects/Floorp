/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "test_addon@tests.mozilla.org";

const ADDONS = {
  test_install1_1: {
    name: "Test 1 Addon",
    description: "Test 1 addon description",
    manifest_version: 2,
    version: "1.0",
    applications: {
      gecko: {
        id: ID,
      },
    },
  },
  test_install1_2: {
    name: "Test 1 Addon",
    description: "Test 1 addon description",
    manifest_version: 2,
    version: "2.0",
    applications: {
      gecko: {
        id: ID,
      },
    },
  },
};

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
});

// User intentionally reinstalls existing disabled addon of the same version.
// No onInstalling nor onInstalled are fired.
add_task(async function reinstallExistingDisabledAddonSameVersion() {
  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onInstalling" }, { event: "onInstalled" }],
      },
      installEvents: [
        { event: "onNewInstall" },
        { event: "onInstallStarted" },
        { event: "onInstallEnded" },
      ],
    },
    async () => {
      const xpi = AddonTestUtils.createTempWebExtensionFile({
        manifest: ADDONS.test_install1_1,
      });
      let install = await AddonManager.getInstallForFile(xpi);
      await install.install();
    }
  );

  let addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onDisabling" }, { event: "onDisabled" }],
      },
    },
    () => addon.disable()
  );

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onEnabling" }, { event: "onEnabled" }],
      },
      installEvents: [
        { event: "onNewInstall" },
        { event: "onInstallStarted" },
        { event: "onInstallEnded" },
      ],
    },
    async () => {
      const xpi2 = AddonTestUtils.createTempWebExtensionFile({
        manifest: ADDONS.test_install1_1,
      });
      let install = await AddonManager.getInstallForFile(xpi2);
      await install.install();
    }
  );

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onUninstalling" }, { event: "onUninstalled" }],
      },
    },
    () => addon.uninstall()
  );

  addon = await promiseAddonByID(ID);
  equal(addon, null);

  await promiseRestartManager();
});

// User intentionally reinstalls existing disabled addon of different version,
// but addon *still should be disabled*.
add_task(async function reinstallExistingDisabledAddonDifferentVersion() {
  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onInstalling" }, { event: "onInstalled" }],
      },
      installEvents: [
        { event: "onNewInstall" },
        { event: "onInstallStarted" },
        { event: "onInstallEnded" },
      ],
    },
    async () => {
      const xpi = AddonTestUtils.createTempWebExtensionFile({
        manifest: ADDONS.test_install1_1,
      });
      let install = await AddonManager.getInstallForFile(xpi);

      await install.install();
    }
  );

  let addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onDisabling" }, { event: "onDisabled" }],
      },
    },
    () => addon.disable()
  );

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onInstalling" }, { event: "onInstalled" }],
      },
      installEvents: [
        { event: "onNewInstall" },
        { event: "onInstallStarted" },
        { event: "onInstallEnded" },
      ],
    },
    async () => {
      let xpi2 = AddonTestUtils.createTempWebExtensionFile({
        manifest: ADDONS.test_install1_2,
      });
      let install = await AddonManager.getInstallForFile(xpi2);
      await install.install();
    }
  );

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);
  equal(addon.version, "2.0");

  await expectEvents(
    {
      ignorePlugins: true,
      addonEvents: {
        [ID]: [{ event: "onUninstalling" }, { event: "onUninstalled" }],
      },
    },
    () => addon.uninstall()
  );

  addon = await promiseAddonByID(ID);
  equal(addon, null);
});
