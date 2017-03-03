/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  createTempWebExtensionFile,
  promiseAddonEvent,
  promiseCompleteAllInstalls,
  promiseFindAddonUpdates,
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

add_task(async function test_privacy_update() {
  // Create a object to hold the values to which we will initialize the prefs.
  const PREFS = {
    "network.predictor.enabled": true,
    "network.prefetch-next": true,
    "network.http.speculative-parallel-limit": 10,
    "network.dns.disablePrefetch": false,
  };

  const EXTENSION_ID = "test_privacy_addon_update@tests.mozilla.org";
  const PREF_EM_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";

  // Set prefs to our initial values.
  for (let pref in PREFS) {
    Preferences.set(pref, PREFS[pref]);
  }

  do_register_cleanup(() => {
    // Reset the prefs.
    for (let pref in PREFS) {
      Preferences.reset(pref);
    }
  });

  async function background() {
    browser.test.onMessage.addListener(async (msg, data) => {
      let settingData;
      switch (msg) {
        case "get":
          settingData = await browser.privacy.network.networkPredictionEnabled.get({});
          browser.test.sendMessage("privacyData", settingData);
          break;

        case "set":
          await browser.privacy.network.networkPredictionEnabled.set(data);
          settingData = await browser.privacy.network.networkPredictionEnabled.get({});
          browser.test.sendMessage("privacyData", settingData);
          break;
      }
    });
  }

  const testServer = createHttpServer();
  const port = testServer.identity.primaryPort;

  // The test extension uses an insecure update url.
  Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

  testServer.registerPathHandler("/test_update.json", (request, response) => {
    response.write(`{
      "addons": {
        "${EXTENSION_ID}": {
          "updates": [
            {
              "version": "2.0",
              "update_link": "http://localhost:${port}/addons/test_privacy-2.0.xpi"
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
      permissions: ["privacy"],
    },
    background,
  });

  testServer.registerFile("/addons/test_privacy-2.0.xpi", webExtensionFile);

  await promiseStartupManager();

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
      permissions: ["privacy"],
    },
    background,
  });

  await extension.startup();

  // Change the value to false.
  extension.sendMessage("set", {value: false});
  let data = await extension.awaitMessage("privacyData");
  ok(!data.value, "get returns expected value after setting.");

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  equal(addon.version, "1.0", "The installed addon has the expected version.");

  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;

  let promiseInstalled = promiseAddonEvent("onInstalled");
  await promiseCompleteAllInstalls([install]);

  let startupPromise = awaitEvent("ready");

  let [updated_addon] = await promiseInstalled;
  equal(updated_addon.version, "2.0", "The updated addon has the expected version.");

  extension.extension = await startupPromise;
  extension.attachListeners();

  extension.sendMessage("get");
  data = await extension.awaitMessage("privacyData");
  ok(!data.value, "get returns expected value after updating.");

  // Verify the prefs are still set to match the "false" setting.
  for (let pref in PREFS) {
    let msg = `${pref} set correctly.`;
    let expectedValue = pref === "network.http.speculative-parallel-limit" ? 0 : !PREFS[pref];
    equal(Preferences.get(pref), expectedValue, msg);
  }

  await extension.unload();

  await updated_addon.uninstall();

  await promiseShutdownManager();
});
