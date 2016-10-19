/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

const {
  createAppInfo,
  createTempWebExtensionFile,
  promiseAddonByID,
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

function awaitEvent(eventName) {
  return new Promise(resolve => {
    let listener = (_eventName, ...args) => {
      if (_eventName === eventName) {
        Management.off(eventName, listener);
        resolve(...args);
      }
    };

    Management.on(eventName, listener);
  });
}

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

function* expectEvents(extension, {onStartupFired, onInstalledFired, onInstalledReason}) {
  extension.sendMessage("get-on-installed-details");
  let details = yield extension.awaitMessage("on-installed-details");
  if (onInstalledFired) {
    equal(details.reason, onInstalledReason, "runtime.onInstalled fired with the correct reason");
  } else {
    equal(details.fired, onInstalledFired, "runtime.onInstalled should not have fired");
  }

  extension.sendMessage("did-on-startup-fire");
  let fired = yield extension.awaitMessage("on-startup-fired");
  equal(fired, onStartupFired, `Expected runtime.onStartup to ${onStartupFired ? "" : "not "} fire`);
}

add_task(function* test_should_fire_on_addon_update() {
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

  yield promiseStartupManager();

  yield extension.startup();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "install",
  });

  let addon = yield promiseAddonByID(EXTENSION_ID);
  equal(addon.version, "1.0", "The installed addon has the correct version");

  let update = yield promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  yield promiseCompleteAllInstalls([install]);

  yield extension.awaitMessage("reloading");

  let startupPromise = awaitEvent("ready");

  let [updated_addon] = yield promiseInstalled;
  equal(updated_addon.version, "2.0", "The updated addon has the correct version");

  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "update",
  });

  yield extension.unload();

  yield updated_addon.uninstall();
  yield promiseShutdownManager();
});

add_task(function* test_should_fire_on_browser_update() {
  const EXTENSION_ID = "test_runtime_on_installed_browser_update@tests.mozilla.org";

  yield promiseStartupManager();

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

  yield extension.startup();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "install",
  });

  let startupPromise = awaitEvent("ready");
  yield promiseRestartManager("1");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: false,
  });

  // Update the browser.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("2");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: true,
    onInstalledReason: "browser_update",
  });

  // Restart the browser.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("2");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: false,
  });

  // Update the browser again.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("3");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: true,
    onInstalledFired: true,
    onInstalledReason: "browser_update",
  });

  yield extension.unload();

  yield promiseShutdownManager();
});

add_task(function* test_should_not_fire_on_reload() {
  const EXTENSION_ID = "test_runtime_on_installed_reload@tests.mozilla.org";

  yield promiseStartupManager();

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

  yield extension.startup();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "install",
  });

  let startupPromise = awaitEvent("ready");
  extension.sendMessage("reload-extension");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: false,
  });

  yield extension.unload();
  yield promiseShutdownManager();
});

add_task(function* test_should_not_fire_on_restart() {
  const EXTENSION_ID = "test_runtime_on_installed_restart@tests.mozilla.org";

  yield promiseStartupManager();

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

  yield extension.startup();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: true,
    onInstalledReason: "install",
  });

  let addon = yield promiseAddonByID(EXTENSION_ID);
  addon.userDisabled = true;

  let startupPromise = awaitEvent("ready");
  addon.userDisabled = false;
  extension.extension = yield startupPromise;
  extension.attachListeners();

  yield expectEvents(extension, {
    onStartupFired: false,
    onInstalledFired: false,
  });

  yield extension.markUnloaded();
  yield promiseShutdownManager();
});
