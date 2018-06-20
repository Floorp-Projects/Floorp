/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const ID = "bootstrap1@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const ADDONS = {
  test_bootstrap1_1: {
    "install.rdf": {
      "id": ID,
      "name": "Test Bootstrap 1",
    },
    "bootstrap.js": EMPTY_BOOTSTRAP_JS
  },
  test_bootstrap1_2: {
    "install.rdf": {
      "id": "bootstrap1@tests.mozilla.org",
      "version": "2.0",
      "name": "Test Bootstrap 1",
    },
    "bootstrap.js": EMPTY_BOOTSTRAP_JS
  },
};

const XPIS = {};
for (let [name, files] of Object.entries(ADDONS)) {
  XPIS[name] = AddonTestUtils.createTempXPIFile(files);
}

function waitForBootstrapEvent(expectedEvent, addonId) {
  return new Promise(resolve => {
    function listener(msg, {method, params, reason}) {
      if (params.id === addonId && method === expectedEvent) {
        resolve({params, method, reason});
        AddonTestUtils.off("bootstrap-method", listener);
      } else {
        info(`Ignoring bootstrap event: ${method} for ${params.id}`);
      }
    }
    AddonTestUtils.on("bootstrap-method", listener);
  });
}

async function checkEvent(promise, {reason, params}) {
  let event = await promise;
  info(`Checking bootstrap event ${event.method} for ${event.params.id}`);

  equal(event.reason, reason,
        `Expected bootstrap reason ${getReasonName(reason)} got ${getReasonName(event.reason)}`);

  for (let [param, value] of Object.entries(params)) {
    equal(event.params[param], value, `Expected value for params.${param}`);
  }
}

let Monitor = SlightlyLessDodgyBootstrapMonitor;
Monitor.init();

// Install a temporary add-on with no existing add-on present.
// Restart and make sure it has gone away.
add_task(async function test_new_temporary() {
  await promiseStartupManager();

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installingCalled = true;
    },
    onInstalled: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installedCalled = true;
    },
    onInstallStarted: (aInstall) => {
      do_throw("onInstallStarted called unexpectedly");
    }
  });

  await AddonManager.installTemporaryAddon(XPIS.test_bootstrap1_1);

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  const install = Monitor.checkInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  Monitor.checkStarted(ID, "1.0");

  let info = Monitor.started.get(ID);
  Assert.equal(info.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  let addon = await promiseAddonByID(ID);

  checkAddon(ID, addon, {
    version: "1.0",
    name: "Test Bootstrap 1",
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

  Monitor.checkNotInstalled(ID);
  Monitor.checkNotStarted(ID);

  addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  await promiseRestartManager();
});

// Install a temporary add-on over the top of an existing add-on.
// Restart and make sure the existing add-on comes back.
add_task(async function test_replace_temporary() {
  let {addon} = await AddonTestUtils.promiseInstallXPI(ADDONS.test_bootstrap1_2);

  Monitor.checkInstalled(ID, "2.0");
  Monitor.checkStarted(ID, "2.0");

  checkAddon(ID, addon, {
    version: "2.0",
    name: "Test Bootstrap 1",
    isCompatible: true,
    appDisabled: false,
    isActive: true,
    type: "extension",
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    temporarilyInstalled: false,
  });

  let tempdir = gTmpD.clone();

  let bootstrapJS = await OS.File.read("data/test_temporary/bootstrap.js", {encoding: "utf-8"});

  for (let newversion of ["1.0", "3.0"]) {
    for (let packed of [false, true]) {
      // ugh, file locking issues with xpis on windows
      if (packed && AppConstants.platform == "win") {
        continue;
      }

      let files = {
        "install.rdf": AddonTestUtils.createInstallRDF({
          id: ID,
          version: newversion,
          name: "Test Bootstrap 1 (temporary)",
        }),
        "bootstrap.js": bootstrapJS,
      };

      let target;
      if (!packed) {
        // Unpacked extensions don't support signing, which means that
        // our mock signing service is not able to give them a
        // privileged signed state, and we can't install them on release
        // builds.
        if (!AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
          continue;
        }

        target = tempdir.clone();
        target.append(ID);

        await AddonTestUtils.promiseWriteFilesToDir(target.path, files);
      } else {
        target = tempdir.clone();
        target.append(`${ID}.xpi`);

        await AddonTestUtils.promiseWriteFilesToZip(target.path, files);
      }

      let onShutdown = waitForBootstrapEvent("shutdown", ID);
      let onUninstall = waitForBootstrapEvent("uninstall", ID);
      let onInstall = waitForBootstrapEvent("install", ID);
      let onStartup = waitForBootstrapEvent("startup", ID);

      await AddonManager.installTemporaryAddon(target);

      let reason = Services.vc.compare(newversion, "2.0") < 0 ?
                   BOOTSTRAP_REASONS.ADDON_DOWNGRADE :
                   BOOTSTRAP_REASONS.ADDON_UPGRADE;

      await checkEvent(onShutdown, {
        reason,
        params: {
          version: "2.0",
        },
      });

      await checkEvent(onUninstall, {
        reason,
        params: {
          version: "2.0",
        },
      });

      await checkEvent(onInstall, {
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

      let signedState = packed ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_UNKNOWN;

      // temporary add-on is installed and started
      checkAddon(ID, addon, {
        version: newversion,
        name: "Test Bootstrap 1 (temporary)",
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
      reason = reason == BOOTSTRAP_REASONS.ADDON_UPGRADE ?
               BOOTSTRAP_REASONS.ADDON_DOWNGRADE :
               BOOTSTRAP_REASONS.ADDON_UPGRADE;

      onShutdown = waitForBootstrapEvent("shutdown", ID);
      onUninstall = waitForBootstrapEvent("uninstall", ID);
      onInstall = waitForBootstrapEvent("install", ID);
      onStartup = waitForBootstrapEvent("startup", ID);

      await promiseRestartManager();

      await checkEvent(onShutdown, {
        reason,
        params: {
          version: newversion,
        },
      });

      await checkEvent(onUninstall, {
        reason,
        params: {
          version: newversion,
        },
      });

      await checkEvent(onInstall, {
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

      Monitor.checkInstalled(ID, "2.0");
      Monitor.checkStarted(ID, "2.0");

      addon = await promiseAddonByID(ID);

      // existing add-on is back
      checkAddon(ID, addon, {
        version: "2.0",
        name: "Test Bootstrap 1",
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

  Monitor.checkNotInstalled(ID);
  Monitor.checkNotStarted(ID);

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
          id: ID
        }
      }
    }
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
          id: ID
        }
      }
    }
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

function promiseWriteWebExtensionToDir(dir, manifest) {
  return AddonTestUtils.promiseWriteFilesToDir(dir.path, {
    "manifest.json": ExtensionTestCommon.generateManifest(manifest),
  });
}

// Install a temporary add-on over the top of an existing add-on.
// Uninstall it and make sure the existing add-on comes back.
add_task(async function test_replace_permanent() {
  await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: ID}},
      version: "1.0",
      name: "Test Bootstrap 1",
    },
  });

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");

  let unpacked_addon = gTmpD.clone();
  unpacked_addon.append(ID);

  await promiseWriteWebExtensionToDir(unpacked_addon, {
    applications: {gecko: {id: ID}},
    version: "2.0",
    name: "Test Bootstrap 1 (temporary)",
  });

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      if (!installingCalled)
        Assert.equal(aInstall.version, "2.0");
      installingCalled = true;
    },
    onInstalled: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      if (!installedCalled)
        Assert.equal(aInstall.version, "2.0");
      installedCalled = true;
    },
    onInstallStarted: (aInstall) => {
      do_throw("onInstallStarted called unexpectedly");
    }
  });

  let addon = await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  Monitor.checkInstalled(ID);
  Monitor.checkStarted(ID);

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

  Monitor.checkInstalled(ID);
  Monitor.checkStarted(ID);

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

  Monitor.checkNotInstalled(ID);
  Monitor.checkNotStarted(ID);

  await promiseRestartManager();
});

// Install a temporary add-on as a version upgrade over the top of an
// existing temporary add-on.
add_task(async function test_replace_temporary_upgrade_presumably_in_a_different_way_from_the_previous_similar_task() {
  const unpackedAddon = gTmpD.clone();
  unpackedAddon.append(ID);

  await promiseWriteWebExtensionToDir(unpackedAddon, {
    applications: {gecko: {id: ID}},
    version: "1.0",
  });

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Increment the version number, re-install it, and make sure it
  // gets marked as an upgrade.
  await promiseWriteWebExtensionToDir(unpackedAddon, {
    applications: {gecko: {id: ID}},
    version: "2.0",
  });

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

  await promiseWriteWebExtensionToDir(unpackedAddon, {
    applications: {gecko: {id: ID}},
    version: "1.0",
  });

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Decrement the version number, re-install, and make sure
  // it gets marked as a downgrade.
  await promiseWriteWebExtensionToDir(unpackedAddon, {
    applications: {gecko: {id: ID}},
    version: "0.8",
  });

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
add_task(async function test_replace_permanent_upgrade_presumably_in_a_different_way_from_the_previous_similar_task() {
  const unpackedAddon = gTmpD.clone();
  unpackedAddon.append(ID);

  await promiseWriteWebExtensionToDir(unpackedAddon, {
    applications: {gecko: {id: ID}},
    version: "1.0",
  });

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

  let info = Monitor.started.get(ID);
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
  let {addon} = await AddonTestUtils.promiseInstallXPI(ADDONS.test_bootstrap1_1);

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");

  await addon.disable();

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkNotStarted(ID);

  let unpacked_addon = gTmpD.clone();
  unpacked_addon.append(ID);

  await promiseWriteWebExtensionToDir(unpacked_addon, {
    applications: {gecko: {id: ID}},
    name: "Test Bootstrap 1 (temporary)",
    version: "2.0",
  });

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  let tempAddon = await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);

  Monitor.checkInstalled(ID, "2.0");
  Monitor.checkStarted(ID);

  // temporary add-on is installed and started
  checkAddon(ID, tempAddon, {
    version: "2.0",
    name: "Test Bootstrap 1 (temporary)",
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

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID);

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

  await addon.uninstall();

  Monitor.checkNotInstalled(ID);
  Monitor.checkNotStarted(ID);

  await promiseRestartManager();
});

// Tests that XPIs with a .zip extension work when loaded temporarily.
add_task({ skip_if: () => AppConstants.MOZ_APP_NAME == "thunderbird" },
         async function test_zip_extension() {
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
