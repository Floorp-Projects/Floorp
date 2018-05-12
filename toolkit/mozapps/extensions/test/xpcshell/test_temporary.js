/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const ID = "bootstrap1@tests.mozilla.org";
const sampleRDFManifest = {
  id: ID,
  version: "1.0",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }],
  name: "Test Bootstrap 1 (temporary)",
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

BootstrapMonitor.init();

// Partial list of bootstrap reasons from XPIProvider.jsm
const BOOTSTRAP_REASONS = {
  APP_STARTUP: 1,
  ADDON_INSTALL: 5,
  ADDON_UNINSTALL: 6,
  ADDON_UPGRADE: 7,
  ADDON_DOWNGRADE: 8,
};

function waitForBootstrapEvent(expectedEvent, addonId) {
  return new Promise(resolve => {
    const observer = {
      observe: (subject, topic, data) => {
        const info = JSON.parse(data);
        const targetAddonId = info.data.id;
        if (targetAddonId === addonId && info.event === expectedEvent) {
          resolve(info);
          Services.obs.removeObserver(observer, "bootstrapmonitor-event");
        } else {
          info(
            `Ignoring bootstrap event: ${info.event} for ${targetAddonId}`);
        }
      },
    };
    Services.obs.addObserver(observer, "bootstrapmonitor-event");
  });
}

// Install a temporary add-on with no existing add-on present.
// Restart and make sure it has gone away.
add_task(async function() {
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

  await AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  const install = BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let info = BootstrapMonitor.started.get(ID);
  Assert.equal(info.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  let addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let onShutdown = waitForBootstrapEvent("shutdown", ID);
  let onUninstall = waitForBootstrapEvent("uninstall", ID);

  await promiseRestartManager();

  let shutdown = await onShutdown;
  equal(shutdown.reason, BOOTSTRAP_REASONS.ADDON_UNINSTALL);

  let uninstall = await onUninstall;
  equal(uninstall.reason, BOOTSTRAP_REASONS.ADDON_UNINSTALL);

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  await promiseRestartManager();
});

// Install a temporary add-on over the top of an existing add-on.
// Restart and make sure the existing add-on comes back.
add_task(async function() {
  await promiseInstallAllFiles([do_get_addon("test_bootstrap1_2")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "2.0");
  BootstrapMonitor.checkAddonStarted(ID, "2.0");

  let addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "2.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

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
          bootstrap: true,
          targetApplications: [{
            id: "xpcshell@tests.mozilla.org",
            minVersion: "1",
            maxVersion: "1"
          }],
          name: "Test Bootstrap 1 (temporary)",
        }),
        "bootstrap.js": bootstrapJS,
      };

      let target;
      if (!packed) {
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

      let shutdown = await onShutdown;
      equal(shutdown.data.version, "2.0");
      equal(shutdown.reason, reason);

      let uninstall = await onUninstall;
      equal(uninstall.data.version, "2.0");
      equal(uninstall.reason, reason);

      let install = await onInstall;
      equal(install.data.version, newversion);
      equal(install.reason, reason);
      equal(install.data.oldVersion, "2.0");

      let startup = await onStartup;
      equal(startup.data.version, newversion);
      equal(startup.reason, reason);
      equal(startup.data.oldVersion, "2.0");

      addon = await promiseAddonByID(ID);

      let signedState = packed ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_UNKNOWN;

      // temporary add-on is installed and started
      Assert.notEqual(addon, null);
      Assert.equal(addon.version, newversion);
      Assert.equal(addon.name, "Test Bootstrap 1 (temporary)");
      Assert.ok(addon.isCompatible);
      Assert.ok(!addon.appDisabled);
      Assert.ok(addon.isActive);
      Assert.equal(addon.type, "extension");
      Assert.equal(addon.signedState, mozinfo.addon_signing ? signedState : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

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

      shutdown = await onShutdown;
      equal(shutdown.data.version, newversion);
      equal(shutdown.reason, reason);

      uninstall = await onUninstall;
      equal(uninstall.data.version, newversion);
      equal(uninstall.reason, reason);

      install = await onInstall;
      equal(install.data.version, "2.0");
      equal(install.reason, reason);
      equal(install.data.oldVersion, newversion);

      startup = await onStartup;
      equal(startup.data.version, "2.0");
      // We don't actually propagate the upgrade/downgrade reason across
      // the browser restart when a temporary addon is removed.  See
      // bug 1359558 for detailed reasoning.
      equal(startup.reason, BOOTSTRAP_REASONS.APP_STARTUP);

      BootstrapMonitor.checkAddonInstalled(ID, "2.0");
      BootstrapMonitor.checkAddonStarted(ID, "2.0");

      addon = await promiseAddonByID(ID);

      // existing add-on is back
      Assert.notEqual(addon, null);
      Assert.equal(addon.version, "2.0");
      Assert.equal(addon.name, "Test Bootstrap 1");
      Assert.ok(addon.isCompatible);
      Assert.ok(!addon.appDisabled);
      Assert.ok(addon.isActive);
      Assert.equal(addon.type, "extension");
      Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

      Services.obs.notifyObservers(target, "flush-cache-entry");
      target.remove(true);
    }
  }

  // remove original add-on
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

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

  await Promise.all([
    AddonManager.installTemporaryAddon(webext),
    promiseWebExtensionStartup(),
  ]);
  let addon = await promiseAddonByID(ID);

  // temporary add-on is installed and started
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test WebExtension 1 (temporary)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

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

  await Promise.all([
    AddonManager.installTemporaryAddon(webext),
    promiseWebExtensionStartup(),
  ]);
  addon = await promiseAddonByID(ID);

  // temporary add-on is installed and started
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "2.0");
  Assert.equal(addon.name, "Test WebExtension 1 (temporary)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.ok(addon.isWebExtension);
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();
});

// Install a temporary add-on over the top of an existing add-on.
// Uninstall it and make sure the existing add-on comes back.
add_task(async function() {
  await promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let tempdir = gTmpD.clone();
  await promiseWriteInstallRDFToDir({
    id: ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (temporary)",
  }, tempdir);

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(ID);

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

  await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  let addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  // temporary add-on is installed and started
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "2.0");
  Assert.equal(addon.name, "Test Bootstrap 1 (temporary)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_UNKNOWN : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();

  await new Promise(executeSoon);
  addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  // existing add-on is back
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  unpacked_addon.remove(true);
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  await promiseRestartManager();
});

// Install a temporary add-on as a version upgrade over the top of an
// existing temporary add-on.
add_task(async function() {
  const tempdir = gTmpD.clone();

  await promiseWriteInstallRDFToDir(sampleRDFManifest, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  const unpackedAddon = tempdir.clone();
  unpackedAddon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpackedAddon, "bootstrap.js");

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Increment the version number, re-install it, and make sure it
  // gets marked as an upgrade.
  await promiseWriteInstallRDFToDir(Object.assign({}, sampleRDFManifest, {
    version: "2.0"
  }), tempdir, "bootstrap1@tests.mozilla.org");

  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUninstall = waitForBootstrapEvent("uninstall", ID);
  const onInstall = waitForBootstrapEvent("install", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  const shutdown = await onShutdown;
  equal(shutdown.data.version, "1.0");
  equal(shutdown.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const uninstall = await onUninstall;
  equal(uninstall.data.version, "1.0");
  equal(uninstall.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const install = await onInstall;
  equal(install.data.version, "2.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);
  equal(install.data.oldVersion, "1.0");

  const startup = await onStartup;
  equal(startup.data.version, "2.0");
  equal(startup.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);
  equal(startup.data.oldVersion, "1.0");

  const addon = await promiseAddonByID(ID);
  addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Install a temporary add-on as a version downgrade over the top of an
// existing temporary add-on.
add_task(async function() {
  const tempdir = gTmpD.clone();

  await promiseWriteInstallRDFToDir(sampleRDFManifest, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  const unpackedAddon = tempdir.clone();
  unpackedAddon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpackedAddon, "bootstrap.js");

  await AddonManager.installTemporaryAddon(unpackedAddon);

  // Decrement the version number, re-install, and make sure
  // it gets marked as a downgrade.
  await promiseWriteInstallRDFToDir(Object.assign({}, sampleRDFManifest, {
    version: "0.8"
  }), tempdir, "bootstrap1@tests.mozilla.org");

  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUninstall = waitForBootstrapEvent("uninstall", ID);
  const onInstall = waitForBootstrapEvent("install", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  const shutdown = await onShutdown;
  equal(shutdown.data.version, "1.0");
  equal(shutdown.reason, BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  const uninstall = await onUninstall;
  equal(uninstall.data.version, "1.0");
  equal(uninstall.reason, BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  const install = await onInstall;
  equal(install.data.version, "0.8");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  const startup = await onStartup;
  equal(startup.data.version, "0.8");
  equal(startup.reason, BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  const addon = await promiseAddonByID(ID);
  addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Installing a temporary add-on over an existing add-on with the same
// version number should be installed as an upgrade.
add_task(async function() {
  const tempdir = gTmpD.clone();

  await promiseWriteInstallRDFToDir(sampleRDFManifest, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  const unpackedAddon = tempdir.clone();
  unpackedAddon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpackedAddon, "bootstrap.js");

  const onInitialInstall = waitForBootstrapEvent("install", ID);
  const onInitialStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  const initialInstall = await onInitialInstall;
  equal(initialInstall.data.version, "1.0");
  equal(initialInstall.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  const initialStartup = await onInitialStartup;
  equal(initialStartup.data.version, "1.0");
  equal(initialStartup.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  let info = BootstrapMonitor.started.get(ID);
  Assert.equal(info.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);

  // Install it again.
  const onShutdown = waitForBootstrapEvent("shutdown", ID);
  const onUninstall = waitForBootstrapEvent("uninstall", ID);
  const onInstall = waitForBootstrapEvent("install", ID);
  const onStartup = waitForBootstrapEvent("startup", ID);
  await AddonManager.installTemporaryAddon(unpackedAddon);

  const shutdown = await onShutdown;
  equal(shutdown.data.version, "1.0");
  equal(shutdown.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const uninstall = await onUninstall;
  equal(uninstall.data.version, "1.0");
  equal(uninstall.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const reInstall = await onInstall;
  equal(reInstall.data.version, "1.0");
  equal(reInstall.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const startup = await onStartup;
  equal(startup.data.version, "1.0");
  equal(startup.reason, BOOTSTRAP_REASONS.ADDON_UPGRADE);

  const addon = await promiseAddonByID(ID);
  addon.uninstall();

  unpackedAddon.remove(true);
  await promiseRestartManager();
});

// Install a temporary add-on over the top of an existing disabled add-on.
// After restart, the existing add-on should continue to be installed and disabled.
add_task(async function() {
  await promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = await promiseAddonByID(ID);

  addon.userDisabled = true;

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID);

  let tempdir = gTmpD.clone();
  await promiseWriteInstallRDFToDir({
    id: ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (temporary)",
  }, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  await AddonManager.installTemporaryAddon(unpacked_addon);

  Assert.ok(extInstallCalled);

  let tempAddon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "2.0");
  BootstrapMonitor.checkAddonStarted(ID);

  // temporary add-on is installed and started
  Assert.notEqual(tempAddon, null);
  Assert.equal(tempAddon.version, "2.0");
  Assert.equal(tempAddon.name, "Test Bootstrap 1 (temporary)");
  Assert.ok(tempAddon.isCompatible);
  Assert.ok(!tempAddon.appDisabled);
  Assert.ok(tempAddon.isActive);
  Assert.equal(tempAddon.type, "extension");
  Assert.equal(tempAddon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_UNKNOWN : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  tempAddon.uninstall();
  unpacked_addon.remove(true);

  addon.userDisabled = false;
  await new Promise(executeSoon);
  addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID);

  // existing add-on is back
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  await promiseRestartManager();
});

// Installing a temporary add-on when there is already a temporary
// add-on should fail.
add_task(async function() {
  await AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));

  let addon = await promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.ok(!addon.isWebExtension);
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  await AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  await promiseRestartManager();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
});

// Check that a temporary add-on is marked as such.
add_task(async function() {
  await AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));
  const addon = await promiseAddonByID(ID);

  notEqual(addon, null);
  equal(addon.temporarilyInstalled, true);

  await promiseRestartManager();
});

// Check that a permanent add-on is not marked as temporarily installed.
add_task(async function() {
  await promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);
  const addon = await promiseAddonByID(ID);

  notEqual(addon, null);
  equal(addon.temporarilyInstalled, false);

  await promiseRestartManager();
});
