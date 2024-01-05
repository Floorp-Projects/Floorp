/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.sys.mjs",
  ExtensionSettingsStore:
    "resource://gre/modules/ExtensionSettingsStore.sys.mjs",
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

const { createAppInfo, promiseShutdownManager, promiseStartupManager } =
  AddonTestUtils;

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
    prefNames: ["my.single.pref"],

    initalValues: ["value1"],

    onPrefsChanged(item) {
      lastSetPref = item;
    },

    valueFn(pref, value) {
      return value;
    },

    setCallback(value) {
      return { [this.prefNames[0]]: this.valueFn(null, value) };
    },
  },
};

ExtensionPreferencesManager.addSetting(
  "multiple_prefs",
  SETTINGS.multiple_prefs
);
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
      extensions[1].id,
      setting
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged has not been called yet"
      );
    }
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl with no settings set."
    );

    let prefsChanged = await ExtensionPreferencesManager.setSetting(
      extensions[1].id,
      setting,
      newValue1
    );
    ok(prefsChanged, "setSetting returns true when the pref(s) have been set.");
    checkPrefs(
      settingObj,
      newValue1,
      "setSetting sets the prefs for the first extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        { id: extensions[1].id, value: newValue1, key: setting },
        "onPrefsChanged is called when pref changes"
      );
    }
    levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
      extensions[1].id,
      setting
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl when a pref has been set."
    );

    let checkSetting = await ExtensionPreferencesManager.getSetting(setting);
    equal(
      checkSetting.value,
      newValue1,
      "getSetting returns the expected value."
    );

    let newValue2 = "newValue2";
    prefsChanged = await ExtensionPreferencesManager.setSetting(
      extensions[0].id,
      setting,
      newValue2
    );
    ok(
      !prefsChanged,
      "setSetting returns false when the pref(s) have not been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "setSetting does not set the pref(s) for an earlier extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged isn't called without control change"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.disableSetting(
      extensions[0].id,
      setting
    );
    ok(
      !prefsChanged,
      "disableSetting returns false when the pref(s) have not been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "disableSetting does not change the pref(s) for the non-top extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged isn't called without control change on disable"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.enableSetting(
      extensions[0].id,
      setting
    );
    ok(
      !prefsChanged,
      "enableSetting returns false when the pref(s) have not been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "enableSetting does not change the pref(s) for the non-top extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged isn't called without control change on enable"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(
      extensions[0].id,
      setting
    );
    ok(
      !prefsChanged,
      "removeSetting returns false when the pref(s) have not been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "removeSetting does not change the pref(s) for the non-top extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged isn't called without control change on remove"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.setSetting(
      extensions[0].id,
      setting,
      newValue2
    );
    ok(
      !prefsChanged,
      "setSetting returns false when the pref(s) have not been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "setSetting does not set the pref(s) for an earlier extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        null,
        "onPrefsChanged isn't called without control change again"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.disableSetting(
      extensions[1].id,
      setting
    );
    ok(
      prefsChanged,
      "disableSetting returns true when the pref(s) have been set."
    );
    checkPrefs(
      settingObj,
      newValue2,
      "disableSetting sets the pref(s) to the next value when disabling the top extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        { id: extensions[0].id, key: setting, value: newValue2 },
        "onPrefsChanged is called when control changes on disable"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.enableSetting(
      extensions[1].id,
      setting
    );
    ok(
      prefsChanged,
      "enableSetting returns true when the pref(s) have been set."
    );
    checkPrefs(
      settingObj,
      newValue1,
      "enableSetting sets the pref(s) to the previous value(s)."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        { id: extensions[1].id, key: setting, value: newValue1 },
        "onPrefsChanged is called when control changes on enable"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(
      extensions[1].id,
      setting
    );
    ok(
      prefsChanged,
      "removeSetting returns true when the pref(s) have been set."
    );
    checkPrefs(
      settingObj,
      newValue2,
      "removeSetting sets the pref(s) to the next value when removing the top extension."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        { id: extensions[0].id, key: setting, value: newValue2 },
        "onPrefsChanged is called when control changes on remove"
      );
    }

    prefsChanged = await ExtensionPreferencesManager.removeSetting(
      extensions[0].id,
      setting
    );
    ok(
      prefsChanged,
      "removeSetting returns true when the pref(s) have been set."
    );
    if (settingObj.onPrefsChanged) {
      checkOnPrefsChanged(
        setting,
        { key: setting, initialValue: { "my.single.pref": "value1" } },
        "onPrefsChanged is called when control is entirely removed"
      );
    }
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(
        Preferences.get(settingObj.prefNames[i]),
        settingObj.initalValues[i],
        "removeSetting sets the pref(s) to the initial value(s) when removing the last extension."
      );
    }

    checkSetting = await ExtensionPreferencesManager.getSetting(setting);
    equal(
      checkSetting,
      null,
      "getSetting returns null when nothing has been set."
    );
  }

  // Tests for unsetAll.
  let newValue3 = "newValue3";
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    await ExtensionPreferencesManager.setSetting(
      extensions[0].id,
      setting,
      newValue3
    );
    checkPrefs(settingObj, newValue3, "setSetting set the pref.");
  }

  let setSettings = await ExtensionSettingsStore.getAllForExtension(
    extensions[0].id,
    STORE_TYPE
  );
  deepEqual(
    setSettings,
    Object.keys(SETTINGS),
    "Expected settings were set for extension."
  );
  await ExtensionPreferencesManager.disableAll(extensions[0].id);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(
        Preferences.get(settingObj.prefNames[i]),
        settingObj.initalValues[i],
        "disableAll unset the pref."
      );
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(
    extensions[0].id,
    STORE_TYPE
  );
  deepEqual(
    setSettings,
    Object.keys(SETTINGS),
    "disableAll retains the settings."
  );

  await ExtensionPreferencesManager.enableAll(extensions[0].id);
  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    checkPrefs(settingObj, newValue3, "enableAll re-set the pref.");
  }

  await ExtensionPreferencesManager.removeAll(extensions[0].id);

  for (let setting in SETTINGS) {
    let settingObj = SETTINGS[setting];
    for (let i = 0; i < settingObj.prefNames.length; i++) {
      equal(
        Preferences.get(settingObj.prefNames[i]),
        settingObj.initalValues[i],
        "removeAll unset the pref."
      );
    }
  }

  setSettings = await ExtensionSettingsStore.getAllForExtension(
    extensions[0].id,
    STORE_TYPE
  );
  deepEqual(setSettings, [], "removeAll removed all settings.");

  // Tests for preventing automatic changes to manually edited prefs.
  for (let setting in SETTINGS) {
    let apiValue = "newValue";
    let manualValue = "something different";
    let settingObj = SETTINGS[setting];
    let extension = extensions[1];
    await ExtensionPreferencesManager.setSetting(
      extension.id,
      setting,
      apiValue
    );

    let checkResetPrefs = method => {
      let prefNames = settingObj.prefNames;
      for (let i = 0; i < prefNames.length; i++) {
        if (i === 0) {
          equal(
            Preferences.get(prefNames[0]),
            manualValue,
            `${method} did not change a manually set pref.`
          );
        } else {
          equal(
            Preferences.get(prefNames[i]),
            settingObj.valueFn(prefNames[i], apiValue),
            `${method} did not change another pref when a pref was manually set.`
          );
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
  await ExtensionPreferencesManager.setSetting(
    extensions[1].id,
    setting,
    newValue
  );
  equal(
    Preferences.get(pref),
    settingObj.valueFn(pref, newValue),
    "Uninitialized pref is set."
  );
  await ExtensionPreferencesManager.removeSetting(extensions[1].id, setting);
  ok(!Preferences.has(pref), "removeSetting removed the pref.");

  // Test levelOfControl with a locked pref.
  setting = "multiple_prefs";
  let prefToLock = SETTINGS[setting].prefNames[0];
  Preferences.lock(prefToLock, 1);
  ok(Preferences.locked(prefToLock), `Preference ${prefToLock} is locked.`);
  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    extensions[1].id,
    setting
  );
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when a pref is locked."
  );

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
      browser_specific_settings: { gecko: { id } },
    },
  });

  await extension.startup();

  // We test both a default pref and a user-set pref.  Get the default
  // value off the pref we'll use.  We fake the default pref by setting
  // a value on it before creating the setting.
  Services.prefs.setBoolPref("bar", true);

  function isUndefinedPref(pref) {
    try {
      Services.prefs.getStringPref(pref);
      return false;
    } catch (e) {
      return true;
    }
  }
  ok(isUndefinedPref("foo"), "test pref is not set");

  await ExtensionSettingsStore.initialize();
  let lastItemChange = Promise.withResolvers();
  ExtensionPreferencesManager.addSetting("some-pref", {
    prefNames: ["foo", "bar"],
    onPrefsChanged(item) {
      lastItemChange.resolve(item);
      lastItemChange = Promise.withResolvers();
    },
    setCallback(value) {
      return { [this.prefNames[0]]: value, [this.prefNames[1]]: false };
    },
  });

  await ExtensionPreferencesManager.setSetting(id, "some-pref", "my value");

  let item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "my value", "The value has been set");
  equal(
    Services.prefs.getStringPref("foo"),
    "my value",
    "The user pref has been set"
  );
  equal(
    Services.prefs.getBoolPref("bar"),
    false,
    "The default pref has been set"
  );

  await ExtensionPreferencesManager.disableSetting(id, "some-pref");

  // test that a disabled setting has been returned to the default value.  In this
  // case the pref is not a default pref, so it will be undefined.
  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, undefined, "The value is back to default");
  equal(item.initialValue.foo, undefined, "The initialValue is correct");
  ok(isUndefinedPref("foo"), "user pref is not set");
  equal(
    Services.prefs.getBoolPref("bar"),
    true,
    "The default pref has been restored to the default"
  );

  // test that setSetting() will enable a disabled setting
  await ExtensionPreferencesManager.setSetting(id, "some-pref", "new value");

  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "new value", "The value is set again");
  equal(
    Services.prefs.getStringPref("foo"),
    "new value",
    "The user pref is set again"
  );
  equal(
    Services.prefs.getBoolPref("bar"),
    false,
    "The default pref has been set again"
  );

  // Force settings to be serialized and reloaded to mimick what happens
  // with settings through a restart of Firefox.  Bug 1576266.
  await ExtensionSettingsStore._reloadFile(true);

  // Now unload the extension to test prefs are reset properly.
  let promise = lastItemChange.promise;
  await extension.unload();

  // Test that the pref is unset when an extension is uninstalled.
  item = await promise;
  deepEqual(
    item,
    { key: "some-pref", initialValue: { bar: true } },
    "The value has been reset"
  );
  ok(isUndefinedPref("foo"), "user pref is not set");
  equal(
    Services.prefs.getBoolPref("bar"),
    true,
    "The default pref has been restored to the default"
  );
  Services.prefs.clearUserPref("bar");

  await promiseShutdownManager();
});

add_task(async function test_preference_default_upgraded() {
  await promiseStartupManager();

  let id = "@upgrade-pref";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: { gecko: { id } },
    },
  });

  await extension.startup();

  // We set the default value for a pref here so it will be
  // picked up by EPM.
  let defaultPrefs = Services.prefs.getDefaultBranch(null);
  defaultPrefs.setStringPref("bar", "initial default");

  await ExtensionSettingsStore.initialize();
  ExtensionPreferencesManager.addSetting("some-pref", {
    prefNames: ["bar"],
    setCallback(value) {
      return { [this.prefNames[0]]: value };
    },
  });

  await ExtensionPreferencesManager.setSetting(id, "some-pref", "new value");
  let item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "new value", "The value is set");

  defaultPrefs.setStringPref("bar", "new default");

  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "new value", "The value is still set");

  let prefsChanged = await ExtensionPreferencesManager.removeSetting(
    id,
    "some-pref"
  );
  ok(prefsChanged, "pref changed on removal of setting.");
  equal(Preferences.get("bar"), "new default", "default value is correct");

  await extension.unload();
  await promiseShutdownManager();
});

add_task(async function test_preference_select() {
  await promiseStartupManager();

  let extensionData = {
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: { gecko: { id: "@one" } },
    },
  };
  let one = ExtensionTestUtils.loadExtension(extensionData);

  await one.startup();

  // We set the default value for a pref here so it will be
  // picked up by EPM.
  let defaultPrefs = Services.prefs.getDefaultBranch(null);
  defaultPrefs.setStringPref("bar", "initial default");

  await ExtensionSettingsStore.initialize();
  ExtensionPreferencesManager.addSetting("some-pref", {
    prefNames: ["bar"],
    setCallback(value) {
      return { [this.prefNames[0]]: value };
    },
  });

  ok(
    await ExtensionPreferencesManager.setSetting(
      one.id,
      "some-pref",
      "new value"
    ),
    "setting was changed"
  );
  let item = await ExtensionPreferencesManager.getSetting("some-pref");
  equal(item.value, "new value", "The value is set");

  // User-set the setting.
  await ExtensionPreferencesManager.selectSetting(null, "some-pref");
  item = await ExtensionPreferencesManager.getSetting("some-pref");
  deepEqual(
    item,
    { key: "some-pref", initialValue: {} },
    "The value is user-set"
  );

  // Extensions installed before cannot gain control again.
  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    one.id,
    "some-pref"
  );
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when user-set."
  );

  // Enabling the top-precedence addon does not take over a user-set setting.
  await ExtensionPreferencesManager.disableSetting(one.id, "some-pref");
  await ExtensionPreferencesManager.enableSetting(one.id, "some-pref");
  item = await ExtensionPreferencesManager.getSetting("some-pref");
  deepEqual(
    item,
    { key: "some-pref", initialValue: {} },
    "The value is user-set"
  );

  // Upgrading does not override the user-set setting.
  extensionData.manifest.version = "2.0";
  extensionData.manifest.incognito = "not_allowed";
  await one.upgrade(extensionData);
  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    one.id,
    "some-pref"
  );
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when user-set after addon upgrade."
  );

  // We can re-select the extension.
  await ExtensionPreferencesManager.selectSetting(one.id, "some-pref");
  item = await ExtensionPreferencesManager.getSetting("some-pref");
  deepEqual(item.value, "new value", "The value is extension set");

  // An extension installed after user-set can take over the setting.
  await ExtensionPreferencesManager.selectSetting(null, "some-pref");
  item = await ExtensionPreferencesManager.getSetting("some-pref");
  deepEqual(
    item,
    { key: "some-pref", initialValue: {} },
    "The value is user-set"
  );

  let two = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: { gecko: { id: "@two" } },
    },
  });

  await two.startup();
  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    two.id,
    "some-pref"
  );
  equal(
    levelOfControl,
    "controllable_by_this_extension",
    "getLevelOfControl returns correct levelOfControl when user-set after addon install."
  );

  await ExtensionPreferencesManager.setSetting(
    two.id,
    "some-pref",
    "another value"
  );
  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "another value", "The value is set");

  // A new installed extension can override a user selected extension.
  let three = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: { gecko: { id: "@three" } },
    },
  });

  // user selects specific extension to take control
  await ExtensionPreferencesManager.selectSetting(one.id, "some-pref");

  // two cannot control
  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    two.id,
    "some-pref"
  );
  equal(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns correct levelOfControl when user-set after addon install."
  );

  // three can control after install
  await three.startup();
  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    three.id,
    "some-pref"
  );
  equal(
    levelOfControl,
    "controllable_by_this_extension",
    "getLevelOfControl returns correct levelOfControl when user-set after addon install."
  );

  await ExtensionPreferencesManager.setSetting(
    three.id,
    "some-pref",
    "third value"
  );
  item = ExtensionSettingsStore.getSetting("prefs", "some-pref");
  equal(item.value, "third value", "The value is set");

  // We have returned to precedence based settings.
  await ExtensionPreferencesManager.removeSetting(three.id, "some-pref");
  await ExtensionPreferencesManager.removeSetting(two.id, "some-pref");
  item = await ExtensionPreferencesManager.getSetting("some-pref");
  equal(item.value, "new value", "The value is extension set");

  await one.unload();
  await two.unload();
  await three.unload();
  await promiseShutdownManager();
});

add_task(async function test_preference_select() {
  let prefNames = await ExtensionPreferencesManager.getManagedPrefDetails();
  // Just check a subset of settings that are in this test file.
  Assert.ok(prefNames.size > 0, "some prefs exist");
  for (let settingName in SETTINGS) {
    let setting = SETTINGS[settingName];
    for (let prefName of setting.prefNames) {
      Assert.equal(
        prefNames.get(prefName),
        settingName,
        "setting retrieved prefNames"
      );
    }
  }
});
