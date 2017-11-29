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
    "ui.context_menus.after_mouseup": false,
    "browser.tabs.loadBookmarksInTabs": false,
    "browser.search.openintab": false,
  };

  async function background() {
    browser.test.onMessage.addListener(async (msg, apiName, value) => {
      let apiObj = browser.browserSettings[apiName];
      let result = await apiObj.set({value});
      if (msg === "set") {
        browser.test.assertTrue(result, "set returns true.");
        browser.test.sendMessage("settingData", await apiObj.get({}));
      } else {
        browser.test.assertFalse(result, "set returns false for a no-op.");
        browser.test.sendMessage("no-op set");
      }
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

  async function testNoOpSetting(setting, value, expected) {
    extension.sendMessage("setNoOp", setting, value);
    await extension.awaitMessage("no-op set");
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

  // This setting is a no-op on Android.
  if (AppConstants.platform === "android") {
    await testNoOpSetting("contextMenuShowEvent", "mouseup",
      {"ui.context_menus.after_mouseup": false});
  } else {
    await testSetting(
      "contextMenuShowEvent", "mouseup",
      {"ui.context_menus.after_mouseup": true});
  }

  // "mousedown" is also a no-op on Windows.
  if (["android", "win"].includes(AppConstants.platform)) {
    await testNoOpSetting("contextMenuShowEvent", "mousedown",
      {"ui.context_menus.after_mouseup": AppConstants.platform === "win"});
  } else {
    await testSetting(
      "contextMenuShowEvent", "mousedown",
      {"ui.context_menus.after_mouseup": false});
  }

  await testSetting(
    "openBookmarksInNewTabs", true,
    {"browser.tabs.loadBookmarksInTabs": true});
  await testSetting(
    "openBookmarksInNewTabs", false,
    {"browser.tabs.loadBookmarksInTabs": false});

  await testSetting(
    "openSearchResultsInNewTabs", true,
    {"browser.search.openintab": true});
  await testSetting(
    "openSearchResultsInNewTabs", false,
    {"browser.search.openintab": false});

  await extension.unload();
  await promiseShutdownManager();
});

add_task(async function test_bad_value() {
  async function background() {
    await browser.test.assertRejects(
      browser.browserSettings.contextMenuShowEvent.set({value: "bad"}),
      /bad is not a valid value for contextMenuShowEvent/,
      "contextMenuShowEvent.set rejects with an invalid value.");

    browser.test.sendMessage("done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browserSettings"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
