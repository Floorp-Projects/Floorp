"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

Services.prefs.setBoolPref("extensions.eventPages.enabled", true);

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  // Create an object to hold the values to which we will initialize the prefs.
  const PREFS = {
    "browser.cache.disk.enable": true,
    "browser.cache.memory.enable": true,
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
});

// Other tests exist for all the settings, this smoke tests that the
// settings will startup an event page.
add_task(async function test_browser_settings() {
  let setExt = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["browserSettings", "privacy"],
    },
    background() {
      browser.test.onMessage.addListener(async (msg, apiName, value) => {
        let apiObj = apiName.split(".").reduce((o, i) => o[i], browser);
        let result = await apiObj.set({ value });
        if (msg === "set") {
          browser.test.assertTrue(result, "set returns true.");
        } else {
          browser.test.assertFalse(result, "set returns false for a no-op.");
        }
      });
    },
  });
  await setExt.startup();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["browserSettings", "privacy"],
      background: { persistent: false },
    },
    background() {
      browser.browserSettings.cacheEnabled.onChange.addListener(() => {
        browser.test.log("cacheEnabled received");
        browser.test.sendMessage("cacheEnabled");
      });
      browser.browserSettings.homepageOverride.onChange.addListener(() => {
        browser.test.sendMessage("homepageOverride");
      });
      browser.browserSettings.newTabPageOverride.onChange.addListener(() => {
        browser.test.sendMessage("newTabPageOverride");
      });
      browser.privacy.services.passwordSavingEnabled.onChange.addListener(
        () => {
          browser.test.sendMessage("passwordSavingEnabled");
        }
      );
    },
  });
  await extension.startup();

  await extension.terminateBackground({ disableResetIdleForTest: true });
  assertPersistentListeners(extension, "browserSettings", "cacheEnabled", {
    primed: true,
  });

  info(`testing cacheEnabled`);
  setExt.sendMessage("set", "browserSettings.cacheEnabled", false);
  await extension.awaitMessage("cacheEnabled");
  ok(true, "cacheEnabled.onChange fired");

  await extension.terminateBackground({ disableResetIdleForTest: true });
  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: true,
  });

  info(`testing homepageOverride`);
  Preferences.set("browser.startup.homepage", "http://homepage.example.com");
  await extension.awaitMessage("homepageOverride");
  ok(true, "homepageOverride.onChange fired");

  if (
    AppConstants.platform !== "android" &&
    AppConstants.MOZ_APP_NAME !== "thunderbird"
  ) {
    await extension.terminateBackground({ disableResetIdleForTest: true });
    assertPersistentListeners(
      extension,
      "browserSettings",
      "newTabPageOverride",
      {
        primed: true,
      }
    );

    info(`testing newTabPageOverride`);
    AboutNewTab.newTabURL = "http://homepage.example.com";
    await extension.awaitMessage("newTabPageOverride");
    ok(true, "newTabPageOverride.onChange fired");
  }

  await extension.terminateBackground({ disableResetIdleForTest: true });
  assertPersistentListeners(
    extension,
    "privacy",
    "services.passwordSavingEnabled",
    {
      primed: true,
    }
  );

  info(`testing passwordSavingEnabled`);
  setExt.sendMessage("set", "privacy.services.passwordSavingEnabled", true);
  await extension.awaitMessage("passwordSavingEnabled");
  ok(true, "passwordSavingEnabled.onChange fired");

  await AddonTestUtils.promiseRestartManager();
  await setExt.awaitStartup();
  await extension.awaitStartup();
  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");

  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: true,
  });

  info(`testing homepageOverride after AOM restart`);
  Preferences.set("browser.startup.homepage", "http://test.example.com");
  await extension.awaitMessage("homepageOverride");
  ok(true, "homepageOverride.onChange fired");

  await extension.unload();
  await setExt.unload();
});
