/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const STORE_TYPE = "prefs";

// Test settings to use with the preferences manager.
const SETTINGS = {
  multiple_prefs: {
    prefNames: ["my.pref.1", "my.pref.2", "my.pref.3"],

    initalValues: ["value1", "value2", "value3"],

    valueFn(pref, value) {
      return `${pref}-${value}`;
    },

    setCallback(value) {
      let prefs = {};
      for (let pref of this.prefNames) {
        prefs[pref] = this.valueFn(pref, value);
      }
      return prefs;
    },
  },

  singlePref: {
    prefNames: [
      "my.single.pref",
    ],

    initalValues: ["value1"],

    valueFn(pref, value) {
      return value;
    },

    setCallback(value) {
      return {[this.prefNames[0]]: this.valueFn(null, value)};
    },
  },
};

ExtensionPreferencesManager.addSetting("multiple_prefs", SETTINGS.multiple_prefs);
ExtensionPreferencesManager.addSetting("singlePref", SETTINGS.singlePref);

// Set initial values for prefs.
for (let setting in SETTINGS) {
  setting = SETTINGS[setting];
  for (let i = 0; i < setting.prefNames.length; i++) {
    Preferences.set(setting.prefNames[i], setting.initalValues[i]);
  }
}

function checkPrefs(settingObj, value, msg) {
  for (let pref of settingObj.prefNames) {
    equal(Preferences.get(pref), settingObj.valueFn(pref, value), msg);
  }
}

add_task(async function test_preference_manager() {
  await promiseStartupManager();

  // Create an array of test framework extension wrappers to install.
  let testExtensions = [
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {},
    }),
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {},
    }),
  ];

  for (let extension of testExtensions) {
    await extension.startup();
  }

  // Create an array actual Extension objects which correspond to the
  // test framework extension wrappers.
  let extensions = testExtensions.map(extension => extension.extension);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    let newValue1 = "newValue1";
    let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
      extensions[1], setting);
    equal(levelOfControl, "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl with no settings set.");

    let prefsChanged = await ExtensionPreferencesManager.setSetting(
      extensions[1], setting, newValue1);
    ok(prefsChanged, "setSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue1,
      "setSetting sets the prefs for the first extension.");
    levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1], setting);
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl when a pref has been set.");

    let newValue2 = "newValue2";
    prefsChanged = await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue2);
    ok(!prefsChanged, "setSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
      "setSetting does not set the pref(s) for an earlier extension.");

    prefsChanged = await ExtensionPreferencesManager.disableSetting(extensions[0], setting);
    ok(!prefsChanged, "disableSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
      "disableSetting does not change the pref(s) for the non-top extension.");

    prefsChanged = await ExtensionPreferencesManager.enableSetting(extensions[0], setting);
    ok(!prefsChanged, "enableSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
      "enableSetting does not change the pref(s) for the non-top extension.");

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[0], setting);
    ok(!prefsChanged, "removeSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
      "removeSetting does not change the pref(s) for the non-top extension.");

    prefsChanged = await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue2);
    ok(!prefsChanged, "setSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
      "setSetting does not set the pref(s) for an earlier extension.");

    prefsChanged = await ExtensionPreferencesManager.disableSetting(extensions[1], setting);
    ok(prefsChanged, "disableSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue2,
      "disableSetting sets the pref(s) to the next value when disabling the top extension.");

    prefsChanged = await ExtensionPreferencesManager.enableSetting(extensions[1], setting);
    ok(prefsChanged, "enableSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue1,
      "enableSetting sets the pref(s) to the previous value(s).");

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[1], setting);
    ok(prefsChanged, "removeSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue2,
      "removeSetting sets the pref(s) to the next value when removing the top extension.");

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[0], setting);
    ok(prefsChanged, "removeSetting returns true when the pref(s) have been set.");
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
        "removeSetting sets the pref(s) to the initial value(s) when removing the last extension.");
    }
  }

  // Tests for unsetAll.
  let newValue3 = "newValue3";
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue3);
    checkPrefs(settingObj, newValue3, "setSetting set the pref.");
  }

  let setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0], STORE_TYPE);
  deepEqual(setSettings, Object.keys(SETTINGS), "Expected settings were set for extension.");
  await ExtensionPreferencesManager.disableAll(extensions[0]);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
        "disableAll unset the pref.");
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0], STORE_TYPE);
  deepEqual(setSettings, Object.keys(SETTINGS), "disableAll retains the settings.");

  await ExtensionPreferencesManager.enableAll(extensions[0]);
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    checkPrefs(settingObj, newValue3, "enableAll re-set the pref.");
  }

  await ExtensionPreferencesManager.removeAll(extensions[0]);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
        "removeAll unset the pref.");
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0], STORE_TYPE);
  deepEqual(setSettings, [], "removeAll removed all settings.");

  // Tests for preventing automatic changes to manually edited prefs.
  for (let setting in SETTINGS) {
    let apiValue = "newValue";
    let manualValue = "something different";
    let settingObj = SETTINGS[setting];
    let extension = extensions[1];
    await ExtensionPreferencesManager.setSetting(extension, setting, apiValue);

    let checkResetPrefs = (method) => {
      let prefNames = settingObj.prefNames;
      for (let i = 0; i < prefNames.length; i++) {
        if (i === 0) {
          equal(Preferences.get(prefNames[0]), manualValue,
                `${method} did not change a manually set pref.`);
        } else {
          equal(Preferences.get(prefNames[i]),
                settingObj.valueFn(prefNames[i], apiValue),
                `${method} did not change another pref when a pref was manually set.`);
        }
      }
    };

    // Manually set the preference to a different value.
    Preferences.set(settingObj.prefNames[0], manualValue);

    await ExtensionPreferencesManager.disableAll(extension);
    checkResetPrefs("disableAll");

    await ExtensionPreferencesManager.enableAll(extension);
    checkResetPrefs("enableAll");

    await ExtensionPreferencesManager.removeAll(extension);
    checkResetPrefs("removeAll");
  }

  // Test with an uninitialized pref.
  let setting = "singlePref";
  let settingObj = SETTINGS[setting];
  let pref = settingObj.prefNames[0];
  let newValue = "newValue";
  Preferences.reset(pref);
  await ExtensionPreferencesManager.setSetting(extensions[1], setting, newValue);
  equal(Preferences.get(pref), settingObj.valueFn(pref, newValue),
    "Uninitialized pref is set.");
  await ExtensionPreferencesManager.removeSetting(extensions[1], setting);
  ok(!Preferences.has(pref), "removeSetting removed the pref.");

  // Test levelOfControl with a locked pref.
  setting = "multiple_prefs";
  let prefToLock = SETTINGS[setting].prefNames[0];
  Preferences.lock(prefToLock, 1);
  ok(Preferences.locked(prefToLock), `Preference ${prefToLock} is locked.`);
  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1], setting);
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when a pref is locked.");

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});
