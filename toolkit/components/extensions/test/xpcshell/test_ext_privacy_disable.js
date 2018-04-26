/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  // eslint-disable-next-line no-shadow
  const {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionPreferencesManager",
                               "resource://gre/modules/ExtensionPreferencesManager.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

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

function awaitPrefChange(prefName) {
  return new Promise(resolve => {
    let listener = (args) => {
      Preferences.ignore(prefName, listener);
      resolve();
    };

    Preferences.observe(prefName, listener);
  });
}

add_task(async function test_disable() {
  const OLD_ID = "old_id@tests.mozilla.org";
  const NEW_ID = "new_id@tests.mozilla.org";

  const PREF_TO_WATCH = "network.http.speculative-parallel-limit";

  // Create an object to hold the values to which we will initialize the prefs.
  const PREFS = {
    "network.predictor.enabled": true,
    "network.prefetch-next": true,
    "network.http.speculative-parallel-limit": 10,
    "network.dns.disablePrefetch": false,
  };

  // Set prefs to our initial values.
  for (let pref in PREFS) {
    Preferences.set(pref, PREFS[pref]);
  }

  registerCleanupFunction(() => {
    // Reset the prefs.
    for (let pref in PREFS) {
      Preferences.reset(pref);
    }
  });

  function checkPrefs(expected) {
    for (let pref in PREFS) {
      let msg = `${pref} set correctly.`;
      let expectedValue = expected ? PREFS[pref] : !PREFS[pref];
      if (pref === "network.http.speculative-parallel-limit") {
        expectedValue = expected ? ExtensionPreferencesManager.getDefaultValue(pref) : 0;
      }
      equal(Preferences.get(pref), expectedValue, msg);
    }
  }

  async function background() {
    browser.test.onMessage.addListener(async (msg, data) => {
      await browser.privacy.network.networkPredictionEnabled.set(data);
      let settingData = await browser.privacy.network.networkPredictionEnabled.get({});
      browser.test.sendMessage("privacyData", settingData);
    });
  }

  // Create an array of extensions to install.
  let testExtensions = [
    ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        applications: {
          gecko: {
            id: OLD_ID,
          },
        },
        permissions: ["privacy"],
      },
      useAddonManager: "temporary",
    }),

    ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        applications: {
          gecko: {
            id: NEW_ID,
          },
        },
        permissions: ["privacy"],
      },
      useAddonManager: "temporary",
    }),
  ];

  await promiseStartupManager();

  for (let extension of testExtensions) {
    await extension.startup();
  }

  // Set the value to true for the older extension.
  testExtensions[0].sendMessage("set", {value: true});
  let data = await testExtensions[0].awaitMessage("privacyData");
  ok(data.value, "Value set to true for the older extension.");

  // Set the value to false for the newest extension.
  testExtensions[1].sendMessage("set", {value: false});
  data = await testExtensions[1].awaitMessage("privacyData");
  ok(!data.value, "Value set to false for the newest extension.");

  // Verify the prefs have been set to match the "false" setting.
  checkPrefs(false);

  // Disable the newest extension.
  let disabledPromise = awaitPrefChange(PREF_TO_WATCH);
  let newAddon = await AddonManager.getAddonByID(NEW_ID);
  newAddon.userDisabled = true;
  await disabledPromise;

  // Verify the prefs have been set to match the "true" setting.
  checkPrefs(true);

  // Disable the older extension.
  disabledPromise = awaitPrefChange(PREF_TO_WATCH);
  let oldAddon = await AddonManager.getAddonByID(OLD_ID);
  oldAddon.userDisabled = true;
  await disabledPromise;

  // Verify the prefs have reverted back to their initial values.
  for (let pref in PREFS) {
    equal(Preferences.get(pref), PREFS[pref], `${pref} reset correctly.`);
  }

  // Re-enable the newest extension.
  let enabledPromise = awaitEvent("ready");
  newAddon.userDisabled = false;
  await enabledPromise;

  // Verify the prefs have been set to match the "false" setting.
  checkPrefs(false);

  // Re-enable the older extension.
  enabledPromise = awaitEvent("ready");
  oldAddon.userDisabled = false;
  await enabledPromise;

  // Verify the prefs have remained set to match the "false" setting.
  checkPrefs(false);

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});
