/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionPreferencesManager",
                               "resource://gre/modules/ExtensionPreferencesManager.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

let lastSetPref;

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

    onPrefsChanged(item) {
      lastSetPref = item;
    },

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

function checkOnPrefsChanged(setting, value, msg) {
  if (value) {
    deepEqual(lastSetPref, value, msg);
    lastSetPref = null;
  } else {
    ok(!lastSetPref, msg);
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
      extensions[1].id, setting);
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(setting, null, "onPrefsChanged has not been called yet");
    }
    equal(levelOfControl, "controllable_by_this_extension",
          "getLevelOfControl returns correct levelOfControl with no settings set.");

    let prefsChanged = await ExtensionPreferencesManager.setSetting(
      extensions[1].id, setting, newValue1);
    ok(prefsChanged, "setSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue1,
               "setSetting sets the prefs for the first extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, {id: extensions[1].id, value: newValue1, key: setting},
        "onPrefsChanged is called when pref changes");
    }
    levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1].id, setting);
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl when a pref has been set.");

    let checkSetting = await ExtensionPreferencesManager.getSetting(setting);
    equal(checkSetting.value, newValue1, "getSetting returns the expected value.");

    let newValue2 = "newValue2";
    prefsChanged = await ExtensionPreferencesManager.setSetting(extensions[0].id, setting, newValue2);
    ok(!prefsChanged, "setSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
               "setSetting does not set the pref(s) for an earlier extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, null, "onPrefsChanged isn't called without control change");
    }

    prefsChanged = await ExtensionPreferencesManager.disableSetting(extensions[0].id, setting);
    ok(!prefsChanged, "disableSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
               "disableSetting does not change the pref(s) for the non-top extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, null, "onPrefsChanged isn't called without control change on disable");
    }

    prefsChanged = await ExtensionPreferencesManager.enableSetting(extensions[0].id, setting);
    ok(!prefsChanged, "enableSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
               "enableSetting does not change the pref(s) for the non-top extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, null, "onPrefsChanged isn't called without control change on enable");
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[0].id, setting);
    ok(!prefsChanged, "removeSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
               "removeSetting does not change the pref(s) for the non-top extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, null, "onPrefsChanged isn't called without control change on remove");
    }

    prefsChanged = await ExtensionPreferencesManager.setSetting(extensions[0].id, setting, newValue2);
    ok(!prefsChanged, "setSetting returns false when the pref(s) have not been set.");
    checkPrefs(settingObj, newValue1,
               "setSetting does not set the pref(s) for an earlier extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, null, "onPrefsChanged isn't called without control change again");
    }

    prefsChanged = await ExtensionPreferencesManager.disableSetting(extensions[1].id, setting);
    ok(prefsChanged, "disableSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue2,
               "disableSetting sets the pref(s) to the next value when disabling the top extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, {id: extensions[0].id, key: setting, value: newValue2},
        "onPrefsChanged is called when control changes on disable");
    }

    prefsChanged = await ExtensionPreferencesManager.enableSetting(extensions[1].id, setting);
    ok(prefsChanged, "enableSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue1,
               "enableSetting sets the pref(s) to the previous value(s).");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, {id: extensions[1].id, key: setting, value: newValue1},
        "onPrefsChanged is called when control changes on enable");
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[1].id, setting);
    ok(prefsChanged, "removeSetting returns true when the pref(s) have been set.");
    checkPrefs(settingObj, newValue2,
               "removeSetting sets the pref(s) to the next value when removing the top extension.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, {id: extensions[0].id, key: setting, value: newValue2},
        "onPrefsChanged is called when control changes on remove");
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(extensions[0].id, setting);
    ok(prefsChanged, "removeSetting returns true when the pref(s) have been set.");
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting, {key: setting, initialValue: {"my.single.pref": "value1"}},
        "onPrefsChanged is called when control is entirely removed");
    }
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
            "removeSetting sets the pref(s) to the initial value(s) when removing the last extension.");
    }

    checkSetting = await ExtensionPreferencesManager.getSetting(setting);
    equal(checkSetting, null, "getSetting returns null when nothing has been set.");
  }

  // Tests for unsetAll.
  let newValue3 = "newValue3";
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    await ExtensionPreferencesManager.setSetting(extensions[0].id, setting, newValue3);
    checkPrefs(settingObj, newValue3, "setSetting set the pref.");
  }

  let setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0].id, STORE_TYPE);
  deepEqual(setSettings, Object.keys(SETTINGS), "Expected settings were set for extension.");
  await ExtensionPreferencesManager.disableAll(extensions[0].id);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
            "disableAll unset the pref.");
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0].id, STORE_TYPE);
  deepEqual(setSettings, Object.keys(SETTINGS), "disableAll retains the settings.");

  await ExtensionPreferencesManager.enableAll(extensions[0].id);
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    checkPrefs(settingObj, newValue3, "enableAll re-set the pref.");
  }

  await ExtensionPreferencesManager.removeAll(extensions[0].id);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(Preferences.get(settingObj.prefNames[i]), settingObj.initalValues[i],
            "removeAll unset the pref.");
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(extensions[0].id, STORE_TYPE);
  deepEqual(setSettings, [], "removeAll removed all settings.");

  // Tests for preventing automatic changes to manually edited prefs.
  for (let setting in SETTINGS) {
    let apiValue = "newValue";
    let manualValue = "something different";
    let settingObj = SETTINGS[setting];
    let extension = extensions[1];
    await ExtensionPreferencesManager.setSetting(extension.id, setting, apiValue);

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

    await ExtensionPreferencesManager.disableAll(extension.id);
    checkResetPrefs("disableAll");

    await ExtensionPreferencesManager.enableAll(extension.id);
    checkResetPrefs("enableAll");

    await ExtensionPreferencesManager.removeAll(extension.id);
    checkResetPrefs("removeAll");
  }

  // Test with an uninitialized pref.
  let setting = "singlePref";
  let settingObj = SETTINGS[setting];
  let pref = settingObj.prefNames[0];
  let newValue = "newValue";
  Preferences.reset(pref);
  await ExtensionPreferencesManager.setSetting(extensions[1].id, setting, newValue);
  equal(Preferences.get(pref), settingObj.valueFn(pref, newValue),
        "Uninitialized pref is set.");
  await ExtensionPreferencesManager.removeSetting(extensions[1].id, setting);
  ok(!Preferences.has(pref), "removeSetting removed the pref.");

  // Test levelOfControl with a locked pref.
  setting = "multiple_prefs";
  let prefToLock = SETTINGS[setting].prefNames[0];
  Preferences.lock(prefToLock, 1);
  ok(Preferences.locked(prefToLock), `Preference ${prefToLock} is locked.`);
  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(extensions[1].id, setting);
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when a pref is locked.");

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});

add_task(async function test_preference_manager_set_when_disabled() {
  await promiseStartupManager();

  let id = "@set-disabled-pref";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      applications: {gecko: {id}},
    },
  });

  await extension.startup();

  await ExtensionSettingsStore.initialize();
  ExtensionPreferencesManager.addSetting("some-pref", {
    pref_names: ["foo"],
    setCallback(value) {
      return {foo: value};
    },
  });

  // We want to test that ExtensionPreferencesManager.setSetting() will enable a
  // disabled setting so we will manually add and disable it in
  // ExtensionSettingsStore.
  await ExtensionSettingsStore.addSetting(
    id, "prefs", "some-pref", "my value", () => "default");

  let item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "my value", "The value is set");

  ExtensionSettingsStore.disable(id, "prefs", "some-pref");

  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.initialValue, "default", "The value is back to default");

  await ExtensionPreferencesManager.setSetting(id, "some-pref", "new value");

  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "new value", "The value is set again");

  await extension.unload();

  await promiseShutdownManager();
});
