/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "addon@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
// Force sync plugin loading to avoid spurious install events from plugins.
Services.prefs.setBoolPref("plugin.load_flash_only", false);
// plugin.load_flash_only is only respected if xpc::IsInAutomation is true.
// This is not the case by default in xpcshell tests, unless the following
// pref is also set. Fixing this generically is bug 1598804
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

function waitForBootstrapEvent(expectedEvent, addonId) {
  return new Promise(resolve => {
    function listener(msg, { method, params, reason }) {
      if (params.id === addonId && method === expectedEvent) {
        resolve({ params, method, reason });
        AddonTestUtils.off("bootstrap-method", listener);
      } else {
        info(`Ignoring bootstrap event: ${method} for ${params.id}`);
      }
    }
    AddonTestUtils.on("bootstrap-method", listener);
  });
}

async function checkEvent(promise, { reason, params }) {
  let event = await promise;
  info(`Checking bootstrap event ${event.method} for ${event.params.id}`);

  equal(
    event.reason,
    reason,
    `Expected bootstrap reason ${getReasonName(reason)} got ${getReasonName(
      event.reason
    )}`
  );

  for (let [param, value] of Object.entries(params)) {
    equal(event.params[param], value, `Expected value for params.${param}`);
  }
}

BootstrapMonitor.init();

const XPIS = {};

add_task(async function setup() {
  for (let n of [1, 2]) {
    XPIS[n] = await createTempWebExtensionFile({
      manifest: {
        name: "Test",
        version: `${n}.0`,
        applications: { gecko: { id: ID } },
      },
    });
  }
});

// Install a temporary add-on with no existing add-on present.
// Restart and make sure it has gone away.
add_task(async function test_new_temporary() {
  await promiseStartupManager();

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: aInstall => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: aInstall => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installingCalled = true;
    },
    onInstalled: aInstall => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installedCalled = true;
    },
    onInstallStarted: aInstall => {
      do_throw("onInstallStarted called unexpectedly");
    },
  });

  await AddonManager.installTemporaryAddon(XPIS[1]);

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  const install = BootstrapMonitor.checkInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  BootstrapMonitor.checkStarted(ID, "1.0");

  let info = BootstrapMonitor.started.get(ID);
  Assert.equal(info.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  let addon = await promiseAddonByID(ID);

  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: true,
  });

  let onShutdown = waitForBootstrapEvent("shutdown", ID);
  let onUninstall = waitForBootstrapEvent("uninstall", ID);

  await promiseRestartManager();

  let shutdown = await onShutdown;
  equal(shutdown.reason, BOOTSTRAP_REASONS.ADDON_UNINSTALL);

  let uninstall = await onUninstall;
  equal(uninstall.reason, BOOTSTRAP_REASONS.ADDON_UNINSTALL);

  BootstrapMonitor.checkNotInstalled(ID);
  BootstrapMonitor.checkNotStarted(ID);

  addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  await promiseRestartManager();
});

// Install a temporary add-on over the top of an existing add-on.
// Restart and make sure the existing add-on comes back.
add_task(async function test_replace_temporary() {
  await promiseInstallFile(XPIS[2]);
  let addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkInstalled(ID, "2.0");
  BootstrapMonitor.checkStarted(ID, "2.0");

  checkAddon(ID, addon, {
    version: "2.0",
    name: "Test",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: false,
  });

  let tempdir = gTmpD.clone();

  for (let newversion of ["1.0", "3.0"]) {
    for (let packed of [false, true]) {
      // ugh, file locking issues with xpis on windows
      if (packed && AppConstants.platform == "win") {
        continue;
      }

      // Unpacked extensions don't support signing, which means that
      // our mock signing service is not able to give them a
      // privileged signed state, and we can't install them on release
      // builds.
      if (!packed && !AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
        continue;
      }

      let files = ExtensionTestCommon.generateFiles({
        manifest: {
          name: "Test",
          version: newversion,
          applications: { gecko: { id: ID } },
        },
      });

      let target = await AddonTestUtils.promiseWriteFilesToExtension(
        tempdir.path,
        ID,
        files,
        !packed
      );

      let onShutdown = waitForBootstrapEvent("shutdown", ID);
      let onUpdate = waitForBootstrapEvent("update", ID);
      let onStartup = waitForBootstrapEvent("startup", ID);

      await AddonManager.installTemporaryAddon(target);

      let reason =
        Services.vc.compare(newversion, "2.0") < 0
          ? BOOTSTRAP_REASONS.ADDON_DOWNGRADE
          : BOOTSTRAP_REASONS.ADDON_UPGRADE;

      await checkEvent(onShutdown, {
        reason,
        params: {
          version: "2.0",
        },
      });

      await checkEvent(onUpdate, {
        reason,
        params: {
          version: newversion,
          oldVersion: "2.0",
        },
      });

      await checkEvent(onStartup, {
        reason,
        params: {
          version: newversion,
          oldVersion: "2.0",
        },
      });

      addon = await promiseAddonByID(ID);

      let signedState = packed
        ? AddonManager.SIGNEDSTATE_PRIVILEGED
        : AddonManager.SIGNEDSTATE_UNKNOWN;

      // temporary add-on is installed and started
      checkAddon(ID, addon, {
        version: newversion,
        name: "Test",
        isCompatible: true,
        appDisabled: false,
        isActive: true,
        type: "extension",
        signedState,
        temporarilyInstalled: true,
      });

      // Now restart, the temporary addon will go away which should
      // be the opposite action (ie, if the temporary addon was an
      // upgrade, then removing it is a downgrade and vice versa)
      reason =
        reason == BOOTSTRAP_REASONS.ADDON_UPGRADE
          ? BOOTSTRAP_REASONS.ADDON_DOWNGRADE
          : BOOTSTRAP_REASONS.ADDON_UPGRADE;

      onShutdown = waitForBootstrapEvent("shutdown", ID);
      onUpdate = waitForBootstrapEvent("update", ID);
      onStartup = waitForBootstrapEvent("startup", ID);

      await promiseRestartManager();

      await checkEvent(onShutdown, {
        reason,
        params: {
          version: newversion,
        },
      });

      await checkEvent(onUpdate, {
        reason,
        params: {
          version: "2.0",
          oldVersion: newversion,
        },
      });

      await checkEvent(onStartup, {
        // We don't actually propagate the upgrade/downgrade reason across
        // the browser restart when a temporary addon is removed.  See
        // bug 1359558 for detailed reasoning.
        reason: BOOTSTRAP_REASONS.APP_STARTUP,
        params: {
          version: "2.0",
        },
      });

      BootstrapMonitor.checkInstalled(ID, "2.0");
      BootstrapMonitor.checkStarted(ID, "2.0");

      addon = await promiseAddonByID(ID);

      // existing add-on is back
      checkAddon(ID, addon, {
        version: "2.0",
        name: "Test",
        isCompatible: true,
        appDisabled: false,
        isActive: true,
        type: "extension",
        signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
        temporarilyInstalled: false,
      });

      Services.obs.notifyObservers(target, "flush-cache-entry");
      target.remove(true);
    }
  }

  // remove original add-on
  await addon.uninstall();

  BootstrapMonitor.checkNotInstalled(ID);
  BootstrapMonitor.checkNotStarted(ID);

  await promiseRestartManager();
});

// Test that loading from the same path multiple times work
add_task(async function test_samefile() {
  // File locking issues on Windows, ugh
  if (AppConstants.platform == "win") {
    return;
  }

  // test that a webextension works
  let webext = createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      name: "Test WebExtension 1 (temporary)",
      applications: {
        gecko: {
          id: ID,
        },
      },
    },
  });

  let addon = await AddonManager.installTemporaryAddon(webext);

  // temporary add-on is installed and started
  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test WebExtension 1 (temporary)",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: true,
  });

  Services.obs.notifyObservers(webext, "flush-cache-entry");
  webext.remove(false);
  webext = createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      name: "Test WebExtension 1 (temporary)",
      applications: {
        gecko: {
          id: ID,
        },
      },
    },
  });

  addon = await AddonManager.installTemporaryAddon(webext);

  // temporary add-on is installed and started
  checkAddon(ID, addon, {
    version: "2.0",
    name: "Test WebExtension 1 (temporary)",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    isWebExtension: true,
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: true,
  });

  await addon.uninstall();
});

// Install a temporary add-on over the top of an existing add-on.
// Uninstall it and make sure the existing add-on comes back.
add_task(async function test_replace_permanent() {
  await promiseInstallWebExtension({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "1.0",
      name: "Test Bootstrap 1",
    },
  });

  BootstrapMonitor.checkInstalled(ID, "1.0");
  BootstrapMonitor.checkStarted(ID, "1.0");

  let unpacked_addon = gTmpD.clone();
  unpacked_addon.append(ID);

  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "2.0",
      name: "Test Bootstrap 1 (temporary)",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpacked_addon.path, files);

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: aInstall => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: aInstall => {
      Assert.equal(aInstall.id, ID);
      if (!installingCalled) {
        Assert.equal(aInstall.version, "2.0");
      }
      installingCalled = true;
    },
    onInstalled: aInstall => {
      Assert.equal(aInstall.id, ID);
      if (!installedCalled) {
        Assert.equal(aInstall.version, "2.0");
      }
      installedCalled = true;
    },
    onInstallStarted: aInstall => {
      do_throw("onInstallStarted called unexpectedly");
    },
  });

  let addon = await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  BootstrapMonitor.checkInstalled(ID);
  BootstrapMonitor.checkStarted(ID);

  // temporary add-on is installed and started
  checkAddon(ID, addon, {
    version: "2.0",
    name: "Test Bootstrap 1 (temporary)",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_UNKNOWN,
    temporarilyInstalled: true,
  });

  await addon.uninstall();

  BootstrapMonitor.checkInstalled(ID);
  BootstrapMonitor.checkStarted(ID);

  addon = await promiseAddonByID(ID);

  // existing add-on is back
  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test Bootstrap 1",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: false,
  });

  unpacked_addon.remove(true);
  await addon.uninstall();

  BootstrapMonitor.checkNotInstalled(ID);
  BootstrapMonitor.checkNotStarted(ID);

  await promiseRestartManager();
});

// Install a temporary add-on as a version upgrade over the top of an
// existing temporary add-on.
add_task(async function test_replace_temporary() {
  const unpackedAddon = gTmpD.clone();
  unpackedAddon.append(ID);

  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "1.0",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpackedAddon.path, files);

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Increment the version number, re-install it, and make sure it
  // gets marked as an upgrade.
  files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "2.0",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpackedAddon.path, files);

  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUpdate = waitForBootstrapEvent("update", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  await checkEvent(onShutdown, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      version: "1.0",
    },
  });

  await checkEvent(onUpdate, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      version: "2.0",
      oldVersion: "1.0",
    },
  });

  await checkEvent(onStartup, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      version: "2.0",
      oldVersion: "1.0",
    },
  });

  const addon = await promiseAddonByID(ID);
  await addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Install a temporary add-on as a version downgrade over the top of an
// existing temporary add-on.
add_task(async function test_replace_temporary_downgrade() {
  const unpackedAddon = gTmpD.clone();
  unpackedAddon.append(ID);

  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "1.0",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpackedAddon.path, files);

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Decrement the version number, re-install, and make sure
  // it gets marked as a downgrade.
  files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "0.8",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpackedAddon.path, files);

  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUpdate = waitForBootstrapEvent("update", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  await checkEvent(onShutdown, {
    reason: BOOTSTRAP_REASONS.ADDON_DOWNGRADE,
    params: {
      version: "1.0",
    },
  });

  await checkEvent(onUpdate, {
    reason: BOOTSTRAP_REASONS.ADDON_DOWNGRADE,
    params: {
      oldVersion: "1.0",
      version: "0.8",
    },
  });

  await checkEvent(onStartup, {
    reason: BOOTSTRAP_REASONS.ADDON_DOWNGRADE,
    params: {
      version: "0.8",
    },
  });

  const addon = await promiseAddonByID(ID);
  await addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Installing a temporary add-on over an existing add-on with the same
// version number should be installed as an upgrade.
add_task(async function test_replace_same_version() {
  const unpackedAddon = gTmpD.clone();
  unpackedAddon.append(ID);

  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      version: "1.0",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpackedAddon.path, files);

  const onInitialInstall = waitForBootstrapEvent("install", ID);
  const onInitialStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  await checkEvent(onInitialInstall, {
    reason: BOOTSTRAP_REASONS.ADDON_INSTALL,
    params: {
      version: "1.0",
    },
  });

  await checkEvent(onInitialStartup, {
    reason: BOOTSTRAP_REASONS.ADDON_INSTALL,
    params: {
      version: "1.0",
    },
  });

  let info = BootstrapMonitor.started.get(ID);
  Assert.equal(info.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  // Install it again.
  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUpdate = waitForBootstrapEvent("update", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  await checkEvent(onShutdown, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      version: "1.0",
    },
  });

  await checkEvent(onUpdate, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      oldVersion: "1.0",
      version: "1.0",
    },
  });

  await checkEvent(onStartup, {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    params: {
      version: "1.0",
    },
  });

  const addon = await promiseAddonByID(ID);
  await addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Install a temporary add-on over the top of an existing disabled add-on.
// After restart, the existing add-on should continue to be installed and disabled.
add_task(async function test_replace_permanent_disabled() {
  await promiseInstallFile(XPIS[1]);
  let addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkInstalled(ID, "1.0");
  BootstrapMonitor.checkStarted(ID, "1.0");

  await addon.disable();

  BootstrapMonitor.checkInstalled(ID, "1.0");
  BootstrapMonitor.checkNotStarted(ID);

  let unpacked_addon = gTmpD.clone();
  unpacked_addon.append(ID);

  let files = ExtensionTestCommon.generateFiles({
    manifest: {
      applications: { gecko: { id: ID } },
      name: "Test",
      version: "2.0",
    },
  });
  await AddonTestUtils.promiseWriteFilesToDir(unpacked_addon.path, files);

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: aInstall => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  let tempAddon = await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);

  BootstrapMonitor.checkInstalled(ID, "2.0");
  BootstrapMonitor.checkStarted(ID);

  // temporary add-on is installed and started
  checkAddon(ID, tempAddon, {
    version: "2.0",
    name: "Test",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_UNKNOWN,
    temporarilyInstalled: true,
  });

  await tempAddon.uninstall();
  unpacked_addon.remove(true);

  await addon.enable();
  await new Promise(executeSoon);
  addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkInstalled(ID, "1.0");
  BootstrapMonitor.checkStarted(ID);

  // existing add-on is back
  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: false,
  });

  await addon.uninstall();

  BootstrapMonitor.checkNotInstalled(ID);
  BootstrapMonitor.checkNotStarted(ID);

  await promiseRestartManager();
});

// Tests that XPIs with a .zip extension work when loaded temporarily.
add_task(async function test_zip_extension() {
  let xpi = createTempWebExtensionFile({
    background() {
      /* globals browser */
      browser.test.sendMessage("started", "Hello.");
    },
  });
  xpi.moveTo(null, xpi.leafName.replace(/\.xpi$/, ".zip"));

  let extension = ExtensionTestUtils.loadExtensionXPI(xpi);
  await extension.startup();

  let msg = await extension.awaitMessage("started");
  equal(msg, "Hello.", "Got expected background script message");

  await extension.unload();
});
