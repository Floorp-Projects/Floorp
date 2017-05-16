/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  createTempWebExtensionFile,
  promiseAddonEvent,
  promiseCompleteAllInstalls,
  promiseFindAddonUpdates,
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

// Allow for unsigned addons.
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

function background() {
  let onInstalledDetails = null;
  let onStartupFired = false;

  browser.runtime.onInstalled.addListener(details => {
    onInstalledDetails = details;
  });

  browser.runtime.onStartup.addListener(() => {
    onStartupFired = true;
  });

  browser.test.onMessage.addListener(message => {
    if (message === "get-on-installed-details") {
      onInstalledDetails = onInstalledDetails || {fired: false};
      browser.test.sendMessage("on-installed-details", onInstalledDetails);
    } else if (message === "did-on-startup-fire") {
      browser.test.sendMessage("on-startup-fired", onStartupFired);
    } else if (message === "reload-extension") {
      browser.runtime.reload();
    }
  });

  browser.runtime.onUpdateAvailable.addListener(details => {
    browser.test.sendMessage("reloading");
    browser.runtime.reload();
  });
}

async function expectEvents(extension, {onStartupFired, onInstalledFired, onInstalledReason, onInstalledTemporary, onInstalledPrevious}) {
  extension.sendMessage("get-on-installed-details");
  let details = await extension.awaitMessage("on-installed-details");
  if (onInstalledFired) {
    equal(details.reason, onInstalledReason, "runtime.onInstalled fired with the correct reason");
    equal(details.temporary, onInstalledTemporary, "runtime.onInstalled fired with the correct temporary flag");
    if (onInstalledPrevious) {
      equal(details.previousVersion, onInstalledPrevious, "runtime.onInstalled after update with correct previousVersion");
    }
  } else {
    equal(details.fired, onInstalledFired, "runtime.onInstalled should not have fired");
  }

  extension.sendMessage("did-on-startup-fire");
  let fired = await extension.awaitMessage("on-startup-fired");
  equal(fired, onStartupFired, `Expected runtime.onStartup to ${onStartupFired ? "" : "not "} fire`);
}

add_task(async function test_should_fire_on_addon_update() {
  Preferences.set("extensions.logging.enabled", false);

  await promiseStartupManager();

  const EXTENSION_ID = "test_runtime_on_installed_addon_update@tests.mozilla.org";

  const PREF_EM_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";

  // The test extension uses an insecure update url.
  Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

  const testServer = createHttpServer();
  const port = testServer.identity.primaryPort;

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
          "update_url": `http://localhost:${port}/test_update.json`,
        },
      },
    },
    background,
  });

  testServer.registerPathHandler("/test_update.json", (request, response) => {
    response.write(`{
      "addons": {
        "${EXTENSION_ID}": {
          "updates": [
            {
              "version": "2.0",
              "update_link": "http://localhost:${port}/addons/test_runtime_on_installed-2.0.xpi"
            }
          ]
        }
      }
    }`);
  });

  let webExtensionFile = createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
  });

  testServer.registerFile("/addons/test_runtime_on_installed-2.0.xpi", webExtensionFile);

  await extension.startup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "install",
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  equal(addon.version, "1.0", "The installed addon has the correct version");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  await promiseCompleteAllInstalls([install]);

  await extension.awaitMessage("reloading");

  let [updated_addon] = await promiseInstalled;
  equal(updated_addon.version, "2.0", "The updated addon has the correct version");

  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "update",
    onInstalledPrevious: "1.0",
  });

  await extension.unload();

  await promiseShutdownManager();
});

add_task(async function test_should_fire_on_browser_update() {
  const EXTENSION_ID = "test_runtime_on_installed_browser_update@tests.mozilla.org";

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "install",
  });

  await promiseRestartManager("1");

  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: false,
  });

  // Update the browser.
  await promiseRestartManager("2");
  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "browser_update",
  });

  // Restart the browser.
  await promiseRestartManager("2");
  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: false,
  });

  // Update the browser again.
  await promiseRestartManager("3");
  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "browser_update",
  });

  await extension.unload();

  await promiseShutdownManager();
});

add_task(async function test_should_not_fire_on_reload() {
  const EXTENSION_ID = "test_runtime_on_installed_reload@tests.mozilla.org";

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "install",
  });

  extension.sendMessage("reload-extension");
  extension.setRestarting();
  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: false,
  });

  await extension.unload();
  await promiseShutdownManager();
});

add_task(async function test_should_not_fire_on_restart() {
  const EXTENSION_ID = "test_runtime_on_installed_restart@tests.mozilla.org";

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledTemporary: false,
    onInstalledReason: "install",
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  addon.userDisabled = true;

  addon.userDisabled = false;
  await extension.awaitStartup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: false,
  });

  await extension.markUnloaded();
  await promiseShutdownManager();
});

add_task(async function test_temporary_installation() {
  const EXTENSION_ID = "test_runtime_on_installed_addon_temporary@tests.mozilla.org";

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      "version": "1.0",
      "applications": {
        "gecko": {
          "id": EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "install",
    onInstalledTemporary: true,
  });

  await extension.unload();
  await promiseShutdownManager();
});
