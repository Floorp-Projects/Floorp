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

add_task(async function test_preference_manager() {
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

  await promiseStartupManager();

  for (let extension of testExtensions) {
    await extension.startup();
  }

  // Create an array actual Extension objects which correspond to the
  // test framework extension wrappers.
  let extensions = testExtensions.map(extension => extension.extension._extension);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    let newValue1 = "newValue1";
    let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1], setting);
    equal(levelOfControl, "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl with no settings set.");
    let settingSet = await ExtensionPreferencesManager.setSetting(extensions[1], setting, newValue1);
    ok(settingSet, "setSetting returns true when the pref(s) have been set.");
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue1),
        "setSetting sets the prefs for the first extension.");
    }
    levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1], setting);
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl when a pref has been set.");

    let newValue2 = "newValue2";
    settingSet = await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue2);
    ok(!settingSet, "setSetting returns false when the pref(s) have not been set.");
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue1),
        "setSetting does not set the pref(s) for an earlier extension.");
    }

    await ExtensionPreferencesManager.unsetSetting(extensions[0], setting);
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue1),
        "unsetSetting does not change the pref(s) for the non-top extension.");
    }

    await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue2);
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue1),
        "setSetting does not set the pref(s) for an earlier extension.");
    }

    await ExtensionPreferencesManager.unsetSetting(extensions[1], setting);
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue2),
        "unsetSetting sets the pref(s) to the next value when removing the top extension.");
    }

    await ExtensionPreferencesManager.unsetSetting(extensions[0], setting);
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
        "unsetSetting sets the pref(s) to the initial value(s) when removing the last extension.");
    }
  }

  // Tests for unsetAll.
  let newValue3 = "newValue3";
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    await ExtensionPreferencesManager.setSetting(extensions[0], setting, newValue3);
    for (let pref of settingObj.prefNames) {
      equal(Preferences.get(pref), settingObj.valueFn(pref, newValue3), `setSetting set the pref for ${pref}.`);
    }
  }

  let setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0], STORE_TYPE);
  deepEqual(setSettings, Object.keys(SETTINGS), "Expected settings were set for extension.");
  await ExtensionPreferencesManager.unsetAll(extensions[0]);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
        "unsetAll unset the pref.");
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0], STORE_TYPE);
  deepEqual(setSettings, [], "unsetAll removed all settings.");

  // Test with an uninitialized pref.
  let setting = "singlePref";
  let settingObj = SETTINGS[setting];
  let pref = settingObj.prefNames[0];
  let newValue = "newValue";
  Preferences.reset(pref);
  await ExtensionPreferencesManager.setSetting(extensions[1], setting, newValue);
  equal(Preferences.get(pref), settingObj.valueFn(pref, newValue),
    "Uninitialized pref is set.");
  await ExtensionPreferencesManager.unsetSetting(extensions[1], setting);
  ok(!Preferences.has(pref), "unsetSetting removed the pref.");

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
