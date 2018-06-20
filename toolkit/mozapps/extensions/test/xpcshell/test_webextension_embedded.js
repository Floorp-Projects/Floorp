/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

BootstrapMonitor.init();

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

// NOTE: the following import needs to be called after the `createAppInfo`
// or it will fail Extension.jsm internally imports AddonManager.jsm and
// AddonManager will raise a ReferenceError exception because it tried to
// access an undefined `Services.appinfo` object.
const { Management } = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

const {
  EmbeddedExtensionManager,
} = ChromeUtils.import("resource://gre/modules/LegacyExtensionsUtils.jsm", {});

function promiseWebExtensionShutdown() {
  return new Promise(resolve => {
    let listener = (event, extension) => {
      Management.off("shutdown", listener);
      resolve(extension);
    };

    Management.on("shutdown", listener);
  });
}

const BOOTSTRAP_WITHOUT_SHUTDOWN = String.raw`
  Components.utils.import("resource://xpcshell-data/BootstrapMonitor.jsm").monitor(this, [
    "install", "startup", "uninstall",
  ]);
`;

const EMBEDDED_WEBEXT_MANIFEST = JSON.stringify({
  name: "embedded webextension addon",
  manifest_version: 2,
  version: "1.0",
});

/**
 *  This test case checks that an hasEmbeddedWebExtension addon property
 *  is persisted and restored correctly across restarts.
 */
add_task(async function has_embedded_webextension_persisted() {
  await promiseStartupManager();
  const ID = "embedded-webextension-addon-persist@tests.mozilla.org";

  const xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    hasEmbeddedWebExtension: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  }, {
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  await promiseInstallFile(xpiFile);

  let addon = await promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");
  equal(addon.hasEmbeddedWebExtension, true,
        "Got the expected hasEmbeddedWebExtension value");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let startupInfo = BootstrapMonitor.started.get(ID);
  ok(("webExtension" in startupInfo.data),
     "Got an webExtension property in the startup bootstrap method params");
  ok(("startup" in startupInfo.data.webExtension),
     "Got the expected 'startup' property in the webExtension object");

  // After restarting the manager, the add-on should still have the
  // hasEmbeddedWebExtension property as expected.
  await promiseRestartManager();

  let persisted = aomStartup.readStartupData()["app-profile"].addons;

  ok(ID in persisted, "Hybrid add-on persisted to bootstrappedAddons.");
  equal(persisted[ID].hasEmbeddedWebExtension, true,
        "hasEmbeddedWebExtension flag persisted to bootstrappedAddons.");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  addon = await promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.hasEmbeddedWebExtension, true, "Got the expected hasEmbeddedWebExtension value");

  // Check that the addon has been installed and started.
  let newStartupInfo = BootstrapMonitor.started.get(ID);
  ok(("webExtension" in newStartupInfo.data),
     "Got an webExtension property in the startup bootstrap method params");
  ok(("startup" in newStartupInfo.data.webExtension),
     "Got the expected 'startup' property in the webExtension object");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  await addon.uninstall();
  await waitUninstall;
});

/**
 *  This test case checks that an addon with hasEmbeddedWebExtension set to true
 *  in its install.rdf gets the expected `embeddedWebExtension` object in the
 *  parameters of its bootstrap methods.
 */
add_task(async function run_embedded_webext_bootstrap() {
  const ID = "embedded-webextension-addon2@tests.mozilla.org";

  const xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    hasEmbeddedWebExtension: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  }, {
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  await AddonManager.installTemporaryAddon(xpiFile);

  let addon = await promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");
  equal(addon.hasEmbeddedWebExtension, true,
        "Got the expected hasEmbeddedWebExtension value");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");

  let installInfo = BootstrapMonitor.installed.get(ID);
  ok(!("webExtension" in installInfo.data),
     "No webExtension property is expected in the install bootstrap method params");

  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let startupInfo = BootstrapMonitor.started.get(ID);

  ok(("webExtension" in startupInfo.data),
     "Got an webExtension property in the startup bootstrap method params");

  ok(("startup" in startupInfo.data.webExtension),
     "Got the expected 'startup' property in the webExtension object");

  const waitForWebExtensionStartup = promiseWebExtensionStartup();

  const embeddedAPI = await startupInfo.data.webExtension.startup();

  // WebExtension startup should have been fully resolved.
  await waitForWebExtensionStartup;

  Assert.deepEqual(
    Object.keys(embeddedAPI.browser.runtime).sort(),
    ["onConnect", "onMessage"],
    `Got the expected 'runtime' in the 'browser' API object`
  );

  // Uninstall the addon and wait that the embedded webextension has been stopped and
  // test the params of the shutdown and uninstall bootstrap method.
  let waitForWebExtensionShutdown = promiseWebExtensionShutdown();
  let waitUninstall = promiseAddonEvent("onUninstalled");
  await addon.uninstall();
  await waitForWebExtensionShutdown;
  await waitUninstall;

  BootstrapMonitor.checkAddonNotStarted(ID, "1.0");

  let shutdownInfo = BootstrapMonitor.stopped.get(ID);
  ok(!("webExtension" in shutdownInfo.data),
     "No webExtension property is expected in the shutdown bootstrap method params");

  let uninstallInfo = BootstrapMonitor.uninstalled.get(ID);
  ok(!("webExtension" in uninstallInfo.data),
     "No webExtension property is expected in the uninstall bootstrap method params");
});

/**
 *  This test case checks that an addon with hasEmbeddedWebExtension can be reloaded
 *  without raising unexpected exceptions due to race conditions.
 */
add_task(async function reload_embedded_webext_bootstrap() {
  const ID = "embedded-webextension-addon2@tests.mozilla.org";

  // No embedded webextension should be currently around.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked here");

  const xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    hasEmbeddedWebExtension: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  }, {
    "bootstrap.js": BOOTSTRAP_MONITOR_BOOTSTRAP_JS,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  await AddonManager.installTemporaryAddon(xpiFile);

  let addon = await promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");
  equal(addon.isActive, true, "The Addon is active");
  equal(addon.appDisabled, false, "The addon is not app disabled");
  equal(addon.userDisabled, false, "The addon is not user disabled");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  // Only one embedded extension.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const embeddedWebExtension = EmbeddedExtensionManager.embeddedExtensionsByAddonId.get(ID);

  let startupInfo = BootstrapMonitor.started.get(ID);
  await startupInfo.data.webExtension.startup();

  const waitForAddonDisabled = promiseAddonEvent("onDisabled");
  await addon.disable();
  await waitForAddonDisabled;

  // No embedded webextension should be currently around.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked here");

  const waitForAddonEnabled = promiseAddonEvent("onEnabled");
  await addon.enable();
  await waitForAddonEnabled;

  // Only one embedded extension.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const embeddedWebExtensionAfterEnabled = EmbeddedExtensionManager.embeddedExtensionsByAddonId.get(ID);
  notEqual(embeddedWebExtensionAfterEnabled, embeddedWebExtension,
           "Got a new EmbeddedExtension instance after the addon has been disabled and then enabled");

  startupInfo = BootstrapMonitor.started.get(ID);
  await startupInfo.data.webExtension.startup();

  const waitForReinstalled = promiseAddonEvent("onInstalled");
  await addon.reload();
  await waitForReinstalled;

  // No leaked embedded extension after the previous reloads.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const embeddedWebExtensionAfterReload = EmbeddedExtensionManager.embeddedExtensionsByAddonId.get(ID);
  notEqual(embeddedWebExtensionAfterReload, embeddedWebExtensionAfterEnabled,
           "Got a new EmbeddedExtension instance after the addon has been reloaded");

  startupInfo = BootstrapMonitor.started.get(ID);
  await startupInfo.data.webExtension.startup();

  // Uninstall the test addon
  let waitUninstalled = promiseAddonEvent("onUninstalled");
  await addon.uninstall();
  await waitUninstalled;

  // No leaked embedded extension after uninstalling.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked after the addon uninstall");
});

/**
 *  This test case checks that an addon with hasEmbeddedWebExtension without
 *  a bootstrap shutdown method stops the embedded webextension.
 */
add_task(async function shutdown_embedded_webext_without_bootstrap_shutdown() {
  const ID = "embedded-webextension-without-shutdown@tests.mozilla.org";

  // No embedded webextension should be currently around.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked here");

  const xpiFile = createTempXPIFile({
    id: ID,
    name: "Test Add-on",
    version: "1.0",
    bootstrap: true,
    hasEmbeddedWebExtension: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1.9.2"
    }]
  }, {
    "bootstrap.js": BOOTSTRAP_WITHOUT_SHUTDOWN,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  await AddonManager.installTemporaryAddon(xpiFile);

  let addon = await promiseAddonByID(ID);

  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.version, "1.0", "Got the expected version");
  equal(addon.isActive, true, "The Addon is active");
  equal(addon.appDisabled, false, "The addon is not app disabled");
  equal(addon.userDisabled, false, "The addon is not user disabled");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  // Only one embedded extension.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const startupInfo = BootstrapMonitor.started.get(ID);

  const waitForWebExtensionStartup = promiseWebExtensionStartup();

  await startupInfo.data.webExtension.startup();

  // WebExtension startup should have been fully resolved.
  await waitForWebExtensionStartup;

  // Fake the BootstrapMonitor notification, or the shutdown checks defined in head_addons.js
  // will fail because of the missing shutdown method.
  const fakeShutdownInfo = Object.assign(startupInfo, {
    event: "shutdown",
    reason: 2,
  });
  Services.obs.notifyObservers({}, "bootstrapmonitor-event", JSON.stringify(fakeShutdownInfo));

  // Uninstall the addon.
  const waitUninstalled = promiseAddonEvent("onUninstalled");
  await addon.uninstall();
  await waitUninstalled;

  // No leaked embedded extension after uninstalling.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked after the addon uninstall");
});
