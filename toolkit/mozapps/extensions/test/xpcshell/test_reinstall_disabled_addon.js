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
  prepare_test({
    [ID]: [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);

  const xpi = AddonTestUtils.createTempWebExtensionFile({manifest: ADDONS.test_install1_1});
  let install = await AddonManager.getInstallForFile(xpi);
  await install.install();
  ensure_test_completed();

  let addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  prepare_test({
    [ID]: [
      ["onDisabling", false],
      "onDisabled",
    ],
  });
  await addon.disable();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);

  prepare_test({
    [ID]: [
      ["onEnabling", false],
      "onEnabled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);

  const xpi2 = AddonTestUtils.createTempWebExtensionFile({manifest: ADDONS.test_install1_1});
  install = await AddonManager.getInstallForFile(xpi2);
  await install.install();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  prepare_test({
    [ID]: [
      ["onUninstalling", false],
      "onUninstalled",
    ],
  });
  await addon.uninstall();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  equal(addon, null);

  await promiseRestartManager();
});

// User intentionally reinstalls existing disabled addon of different version,
// but addon *still should be disabled*.
add_task(async function reinstallExistingDisabledAddonDifferentVersion() {
  prepare_test({
    [ID]: [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);

  const xpi = AddonTestUtils.createTempWebExtensionFile({manifest: ADDONS.test_install1_1});
  let install = await AddonManager.getInstallForFile(xpi);

  await install.install();
  ensure_test_completed();

  let addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(addon.isActive);
  ok(!addon.userDisabled);

  prepare_test({
    [ID]: [
      ["onDisabling", false],
      "onDisabled",
    ],
  });
  await addon.disable();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);

  prepare_test({
    [ID]: [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);
  let xpi2 = AddonTestUtils.createTempWebExtensionFile({manifest: ADDONS.test_install1_2});
  install = await AddonManager.getInstallForFile(xpi2);
  await install.install();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  notEqual(addon, null);
  equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  ok(!addon.isActive);
  ok(addon.userDisabled);
  equal(addon.version, "2.0");

  prepare_test({
    [ID]: [
      ["onUninstalling", false],
      "onUninstalled",
    ],
  });
  await addon.uninstall();
  ensure_test_completed();

  addon = await promiseAddonByID(ID);
  equal(addon, null);
});
