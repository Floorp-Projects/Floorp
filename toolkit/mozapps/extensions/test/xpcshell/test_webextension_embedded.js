/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

BootstrapMonitor.init();

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");
startupManager();

// NOTE: the following import needs to be called after the `createAppInfo`
// or it will fail Extension.jsm internally imports AddonManager.jsm and
// AddonManager will raise a ReferenceError exception because it tried to
// access an undefined `Services.appinfo` object.
const { Management } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

const {
  EmbeddedExtensionManager,
} = Components.utils.import("resource://gre/modules/LegacyExtensionsUtils.jsm");

// Wait the startup of the embedded webextension.
function promiseWebExtensionStartup() {
  return new Promise(resolve => {
    let listener = (event, extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

function promiseWebExtensionShutdown() {
  return new Promise(resolve => {
    let listener = (event, extension) => {
      Management.off("shutdown", listener);
      resolve(extension);
    };

    Management.on("shutdown", listener);
  });
}

const BOOTSTRAP = String.raw`
  Components.utils.import("resource://xpcshell-data/BootstrapMonitor.jsm").monitor(this);
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
add_task(function* has_embedded_webextension_persisted() {
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
    "bootstrap.js": BOOTSTRAP,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  yield promiseInstallFile(xpiFile);

  let addon = yield promiseAddonByID(ID);

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
  yield promiseRestartManager();

  let persisted = JSON.parse(Services.prefs.getCharPref("extensions.bootstrappedAddons"));
  ok(ID in persisted, "Hybrid add-on persisted to bootstrappedAddons.");
  equal(persisted[ID].hasEmbeddedWebExtension, true,
        "hasEmbeddedWebExtension flag persisted to bootstrappedAddons.");

  // Check that the addon has been installed and started.
  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  addon = yield promiseAddonByID(ID);
  notEqual(addon, null, "Got an addon object as expected");
  equal(addon.hasEmbeddedWebExtension, true, "Got the expected hasEmbeddedWebExtension value");

  // Check that the addon has been installed and started.
  let newStartupInfo = BootstrapMonitor.started.get(ID);
  ok(("webExtension" in newStartupInfo.data),
     "Got an webExtension property in the startup bootstrap method params");
  ok(("startup" in newStartupInfo.data.webExtension),
     "Got the expected 'startup' property in the webExtension object");

  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstall;
});

/**
 *  This test case checks that an addon with hasEmbeddedWebExtension set to true
 *  in its install.rdf gets the expected `embeddedWebExtension` object in the
 *  parameters of its bootstrap methods.
 */
add_task(function* run_embedded_webext_bootstrap() {
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
    "bootstrap.js": BOOTSTRAP,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  yield AddonManager.installTemporaryAddon(xpiFile);

  let addon = yield promiseAddonByID(ID);

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

  const embeddedAPI = yield startupInfo.data.webExtension.startup();

  // WebExtension startup should have been fully resolved.
  yield waitForWebExtensionStartup;

  Assert.deepEqual(
    Object.keys(embeddedAPI.browser.runtime).sort(),
    ["onConnect", "onMessage"],
    `Got the expected 'runtime' in the 'browser' API object`
  );

  // Uninstall the addon and wait that the embedded webextension has been stopped and
  // test the params of the shutdown and uninstall bootstrap method.
  let waitForWebExtensionShutdown = promiseWebExtensionShutdown();
  let waitUninstall = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitForWebExtensionShutdown;
  yield waitUninstall;

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
add_task(function* reload_embedded_webext_bootstrap() {
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
    "bootstrap.js": BOOTSTRAP,
    "webextension/manifest.json": EMBEDDED_WEBEXT_MANIFEST,
  });

  yield AddonManager.installTemporaryAddon(xpiFile);

  let addon = yield promiseAddonByID(ID);

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
  yield startupInfo.data.webExtension.startup();

  const waitForAddonDisabled = promiseAddonEvent("onDisabled");
  addon.userDisabled = true;
  yield waitForAddonDisabled;

  // No embedded webextension should be currently around.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked here");

  const waitForAddonEnabled = promiseAddonEvent("onEnabled");
  addon.userDisabled = false;
  yield waitForAddonEnabled;

  // Only one embedded extension.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const embeddedWebExtensionAfterEnabled = EmbeddedExtensionManager.embeddedExtensionsByAddonId.get(ID);
  notEqual(embeddedWebExtensionAfterEnabled, embeddedWebExtension,
           "Got a new EmbeddedExtension instance after the addon has been disabled and then enabled");

  startupInfo = BootstrapMonitor.started.get(ID);
  yield startupInfo.data.webExtension.startup();

  const waitForReinstalled = promiseAddonEvent("onInstalled");
  addon.reload();
  yield waitForReinstalled;

  // No leaked embedded extension after the previous reloads.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked extension instances");

  const embeddedWebExtensionAfterReload = EmbeddedExtensionManager.embeddedExtensionsByAddonId.get(ID);
  notEqual(embeddedWebExtensionAfterReload, embeddedWebExtensionAfterEnabled,
           "Got a new EmbeddedExtension instance after the addon has been reloaded");

  startupInfo = BootstrapMonitor.started.get(ID);
  yield startupInfo.data.webExtension.startup();

  // Uninstall the test addon
  let waitUninstalled = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  yield waitUninstalled;

  // No leaked embedded extension after uninstalling.
  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "No embedded extension instance should be tracked after the addon uninstall");
});
