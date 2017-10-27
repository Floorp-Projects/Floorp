/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

add_task(async function test_browser_settings() {
  const PERM_DENY_ACTION = Services.perms.DENY_ACTION;
  const PERM_UNKNOWN_ACTION = Services.perms.UNKNOWN_ACTION;

  // Create an object to hold the values to which we will initialize the prefs.
  const PREFS = {
    "browser.cache.disk.enable": true,
    "browser.cache.memory.enable": true,
    "dom.popup_allowed_events": Preferences.get("dom.popup_allowed_events"),
    "image.animation_mode": "none",
    "permissions.default.desktop-notification": PERM_UNKNOWN_ACTION,
  };

  async function background() {
    browser.test.onMessage.addListener(async (msg, apiName, value) => {
      let apiObj = browser.browserSettings[apiName];
      await apiObj.set({value});
      browser.test.sendMessage("settingData", await apiObj.get({}));
    });
  }

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

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
  });

  await promiseStartupManager();
  await extension.startup();

  async function testSetting(setting, value, expected) {
    extension.sendMessage("set", setting, value);
    let data = await extension.awaitMessage("settingData");
    equal(data.value, value,
          `The ${setting} setting has the expected value.`);
    equal(data.levelOfControl, "controlled_by_this_extension",
          `The ${setting} setting has the expected levelOfControl.`);
    for (let pref in expected) {
      equal(Preferences.get(pref), expected[pref], `${pref} set correctly for ${value}`);
    }
  }

  await testSetting(
    "cacheEnabled", false,
    {
      "browser.cache.disk.enable": false,
      "browser.cache.memory.enable": false,
    });
  await testSetting(
    "cacheEnabled", true,
    {
      "browser.cache.disk.enable": true,
      "browser.cache.memory.enable": true,
    });

  await testSetting(
    "allowPopupsForUserEvents", false,
    {"dom.popup_allowed_events": ""});
  await testSetting(
    "allowPopupsForUserEvents", true,
    {"dom.popup_allowed_events": PREFS["dom.popup_allowed_events"]});

  for (let value of ["normal", "none", "once"]) {
    await testSetting(
      "imageAnimationBehavior", value,
      {"image.animation_mode": value});
  }

  await testSetting(
    "webNotificationsDisabled", true,
    {"permissions.default.desktop-notification": PERM_DENY_ACTION});
  await testSetting(
    "webNotificationsDisabled", false,
    {
      // This pref is not defaulted on Android.
      "permissions.default.desktop-notification":
        AppConstants.MOZ_BUILD_APP !== "browser" ? undefined : PERM_UNKNOWN_ACTION,
    });

  await extension.unload();

  await promiseShutdownManager();
});
