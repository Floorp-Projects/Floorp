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
    let listener = (_eventName, extension) => {
      if (_eventName === eventName) {
        Management.off(eventName, listener);
        resolve(extension);
      }
    };

    Management.on(eventName, listener);
  });
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
    background() {
      browser.runtime.onUpdateAvailable.addListener(details => {
        browser.test.sendMessage("reloading");
        browser.runtime.reload();
      });

      browser.runtime.onInstalled.addListener(details => {
        browser.test.sendMessage("installed", details);
      });
    },
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
    background() {
      browser.runtime.onInstalled.addListener(details => {
        browser.test.sendMessage("installed", details);
      });
    },
  });

  testServer.registerFile("/addons/test_runtime_on_installed-2.0.xpi", webExtensionFile);

  yield promiseStartupManager();

  yield extension.startup();
  let details = yield extension.awaitMessage("installed");
  equal(details.reason, "install", "runtime.onInstalled fired with the correct reason");

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

  details = yield extension.awaitMessage("installed");
  equal(details.reason, "update", "runtime.onInstalled fired with the correct reason");

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
    background() {
      let onInstalledDetails = null;

      browser.runtime.onInstalled.addListener(details => {
        onInstalledDetails = details;
      });

      browser.test.onMessage.addListener(message => {
        if (message == "get-on-installed-details") {
          browser.test.sendMessage("on-installed-details", onInstalledDetails);
        }
      });
    },
  });

  yield extension.startup();

  extension.sendMessage("get-on-installed-details");
  let details = yield extension.awaitMessage("on-installed-details");
  equal(details.reason, "install", "runtime.onInstalled fired with the correct reason");

  let startupPromise = awaitEvent("ready");
  yield promiseRestartManager("1");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details, null, "runtime.onInstalled should not have fired");

  // Update the browser.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("2");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details.reason, "browser_update", "runtime.onInstalled fired with the correct reason");

  // Restart the browser.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("2");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details, null, "runtime.onInstalled should not have fired");

  // Update the browser again.
  startupPromise = awaitEvent("ready");
  yield promiseRestartManager("3");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details.reason, "browser_update", "runtime.onInstalled fired with the correct reason");

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
    background() {
      let onInstalledDetails = null;

      browser.runtime.onInstalled.addListener(details => {
        onInstalledDetails = details;
      });

      browser.test.onMessage.addListener(message => {
        if (message == "reload-extension") {
          browser.runtime.reload();
        } else if (message == "get-on-installed-details") {
          browser.test.sendMessage("on-installed-details", onInstalledDetails);
        }
      });
    },
  });

  yield extension.startup();

  extension.sendMessage("get-on-installed-details");
  let details = yield extension.awaitMessage("on-installed-details");
  equal(details.reason, "install", "runtime.onInstalled fired with the correct reason");

  let startupPromise = awaitEvent("ready");
  extension.sendMessage("reload-extension");
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details, null, "runtime.onInstalled should not have fired");

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
    background() {
      let onInstalledDetails = null;

      browser.runtime.onInstalled.addListener(details => {
        onInstalledDetails = details;
      });

      browser.test.onMessage.addListener(message => {
        if (message == "get-on-installed-details") {
          browser.test.sendMessage("on-installed-details", onInstalledDetails);
        }
      });
    },
  });

  yield extension.startup();

  extension.sendMessage("get-on-installed-details");
  let details = yield extension.awaitMessage("on-installed-details");
  equal(details.reason, "install", "runtime.onInstalled fired with the correct reason");

  let addon = yield promiseAddonByID(EXTENSION_ID);
  addon.userDisabled = true;

  let startupPromise = awaitEvent("ready");
  addon.userDisabled = false;
  extension.extension = yield startupPromise;
  extension.attachListeners();

  extension.sendMessage("get-on-installed-details");
  details = yield extension.awaitMessage("on-installed-details");
  equal(details, null, "runtime.onInstalled should not have fired");

  yield extension.markUnloaded();
  yield promiseShutdownManager();
});
