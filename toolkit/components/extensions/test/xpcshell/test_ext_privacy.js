/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
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

add_task(async function test_privacy() {
  // Create an object to hold the values to which we will initialize the prefs.
  const SETTINGS = {
    "network.networkPredictionEnabled": {
      "network.predictor.enabled": true,
      "network.prefetch-next": true,
      "network.http.speculative-parallel-limit": 10,
      "network.dns.disablePrefetch": false,
    },
    "websites.hyperlinkAuditingEnabled": {
      "browser.send_pings": true,
    },
  };

  async function background() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      let data = args[0];
      // The second argument is the end of the api name,
      // e.g., "network.networkPredictionEnabled".
      let apiObj = args[1].split(".").reduce((o, i) => o[i], browser.privacy);
      let settingData;
      switch (msg) {
        case "get":
          settingData = await apiObj.get(data);
          browser.test.sendMessage("gotData", settingData);
          break;

        case "set":
          await apiObj.set(data);
          settingData = await apiObj.get({});
          browser.test.sendMessage("afterSet", settingData);
          break;

        case "clear":
          await apiObj.clear(data);
          settingData = await apiObj.get({});
          browser.test.sendMessage("afterClear", settingData);
          break;
      }
    });
  }

  // Set prefs to our initial values.
  for (let setting in SETTINGS) {
    for (let pref in SETTINGS[setting]) {
      Preferences.set(pref, SETTINGS[setting][pref]);
    }
  }

  do_register_cleanup(() => {
    // Reset the prefs.
    for (let setting in SETTINGS) {
      for (let pref in SETTINGS[setting]) {
        Preferences.reset(pref);
      }
    }
  });

  // Create an array of extensions to install.
  let testExtensions = [
    ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["privacy"],
      },
      useAddonManager: "temporary",
    }),

    ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["privacy"],
      },
      useAddonManager: "temporary",
    }),
  ];

  await promiseStartupManager();

  for (let extension of testExtensions) {
    await extension.startup();
  }

  for (let setting in SETTINGS) {
    testExtensions[0].sendMessage("get", {}, setting);
    let data = await testExtensions[0].awaitMessage("gotData");
    ok(data.value, "get returns expected value.");
    equal(data.levelOfControl, "controllable_by_this_extension",
      "get returns expected levelOfControl.");

    testExtensions[0].sendMessage("get", {incognito: true}, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(data.value, "get returns expected value with incognito.");
    equal(data.levelOfControl, "not_controllable",
      "get returns expected levelOfControl with incognito.");

    // Change the value to false.
    testExtensions[0].sendMessage("set", {value: false}, setting);
    data = await testExtensions[0].awaitMessage("afterSet");
    ok(!data.value, "get returns expected value after setting.");
    equal(data.levelOfControl, "controlled_by_this_extension",
      "get returns expected levelOfControl after setting.");

    // Verify the prefs have been set to match the "false" setting.
    for (let pref in SETTINGS[setting]) {
      let msg = `${pref} set correctly for ${setting}`;
      if (pref === "network.http.speculative-parallel-limit") {
        equal(Preferences.get(pref), 0, msg);
      } else {
        equal(Preferences.get(pref), !SETTINGS[setting][pref], msg);
      }
    }

    // Change the value with a newer extension.
    testExtensions[1].sendMessage("set", {value: true}, setting);
    data = await testExtensions[1].awaitMessage("afterSet");
    ok(data.value, "get returns expected value after setting via newer extension.");
    equal(data.levelOfControl, "controlled_by_this_extension",
      "get returns expected levelOfControl after setting.");

    // Verify the prefs have been set to match the "true" setting.
    for (let pref in SETTINGS[setting]) {
      let msg = `${pref} set correctly for ${setting}`;
      if (pref === "network.http.speculative-parallel-limit") {
        equal(Preferences.get(pref), ExtensionPreferencesManager.getDefaultValue(pref), msg);
      } else {
        equal(Preferences.get(pref), SETTINGS[setting][pref], msg);
      }
    }

    // Change the value with an older extension.
    testExtensions[0].sendMessage("set", {value: false}, setting);
    data = await testExtensions[0].awaitMessage("afterSet");
    ok(data.value, "Newer extension remains in control.");
    equal(data.levelOfControl, "controlled_by_other_extensions",
      "get returns expected levelOfControl when controlled by other.");

    // Clear the value of the newer extension.
    testExtensions[1].sendMessage("clear", {}, setting);
    data = await testExtensions[1].awaitMessage("afterClear");
    ok(!data.value, "Older extension gains control.");
    equal(data.levelOfControl, "controllable_by_this_extension",
      "Expected levelOfControl returned after clearing.");

    testExtensions[0].sendMessage("get", {}, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(!data.value, "Current, older extension has control.");
    equal(data.levelOfControl, "controlled_by_this_extension",
      "Expected levelOfControl returned after clearing.");

    // Set the value again with the newer extension.
    testExtensions[1].sendMessage("set", {value: true}, setting);
    data = await testExtensions[1].awaitMessage("afterSet");
    ok(data.value, "get returns expected value after setting via newer extension.");
    equal(data.levelOfControl, "controlled_by_this_extension",
      "get returns expected levelOfControl after setting.");

    // Unload the newer extension. Expect the older extension to regain control.
    await testExtensions[1].unload();
    testExtensions[0].sendMessage("get", {}, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(!data.value, "Older extension regained control.");
    equal(data.levelOfControl, "controlled_by_this_extension",
      "Expected levelOfControl returned after unloading.");

    // Reload the extension for the next iteration of the loop.
    testExtensions[1] = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["privacy"],
      },
      useAddonManager: "temporary",
    });
    await testExtensions[1].startup();

    // Clear the value of the older extension.
    testExtensions[0].sendMessage("clear", {}, setting);
    data = await testExtensions[0].awaitMessage("afterClear");
    ok(data.value, "Setting returns to original value when all are cleared.");
    equal(data.levelOfControl, "controllable_by_this_extension",
      "Expected levelOfControl returned after clearing.");

    // Verify that our initial values were restored.
    for (let pref in SETTINGS[setting]) {
      equal(Preferences.get(pref), SETTINGS[setting][pref], `${pref} was reset to its initial value.`);
    }
  }

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});

// This test can be used for any settings that are added which utilize only
// boolean prefs.
add_task(async function test_privacy_boolean_prefs() {
  // Create an object to hold the values to which we will initialize the prefs.
  const SETTINGS = {
    "network.webRTCIPHandlingPolicy": {
      "media.peerconnection.ice.default_address_only": false,
      "media.peerconnection.ice.no_host": false,
      "media.peerconnection.ice.proxy_only": false,
    },
    "network.peerConnectionEnabled": {
      "media.peerconnection.enabled": true,
    },
  };

  async function background() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      let data = args[0];
      // The second argument is the end of the api name,
      // e.g., "network.webRTCIPHandlingPolicy".
      let apiObj = args[1].split(".").reduce((o, i) => o[i], browser.privacy);
      let settingData;
      switch (msg) {
        case "set":
          await apiObj.set(data);
          settingData = await apiObj.get({});
          browser.test.sendMessage("settingData", settingData);
          break;

        case "clear":
          await apiObj.clear(data);
          settingData = await apiObj.get({});
          browser.test.sendMessage("settingData", settingData);
          break;
      }
    });
  }

  // Set prefs to our initial values.
  for (let setting in SETTINGS) {
    for (let pref in SETTINGS[setting]) {
      Preferences.set(pref, SETTINGS[setting][pref]);
    }
  }

  do_register_cleanup(() => {
    // Reset the prefs.
    for (let setting in SETTINGS) {
      for (let pref in SETTINGS[setting]) {
        Preferences.reset(pref);
      }
    }
  });

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["privacy"],
    },
    useAddonManager: "temporary",
  });

  await promiseStartupManager();
  await extension.startup();

  async function testSetting(setting, value, truePrefs) {
    extension.sendMessage("set", {value: value}, setting);
    let data = await extension.awaitMessage("settingData");
    equal(data.value, value);
    for (let pref in SETTINGS[setting]) {
      let prefValue = Preferences.get(pref);
      equal(prefValue, truePrefs.includes(pref), `${pref} set correctly for ${value}`);
    }
  }

  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "default_public_and_private_interfaces",
    ["media.peerconnection.ice.default_address_only"]);

  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "default_public_interface_only",
    ["media.peerconnection.ice.default_address_only", "media.peerconnection.ice.no_host"]);

  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "disable_non_proxied_udp",
    ["media.peerconnection.ice.proxy_only"]);

  await testSetting("network.webRTCIPHandlingPolicy", "default", []);

  await testSetting("network.peerConnectionEnabled", false, []);
  await testSetting("network.peerConnectionEnabled", true, ["media.peerconnection.enabled"]);

  await extension.unload();

  await promiseShutdownManager();
});

add_task(async function test_exceptions() {
  async function background() {
    await browser.test.assertRejects(
      browser.privacy.network.networkPredictionEnabled.set({value: true, scope: "regular_only"}),
      "Firefox does not support the regular_only settings scope.",
      "Expected rejection calling set with invalid scope.");

    await browser.test.assertRejects(
      browser.privacy.network.networkPredictionEnabled.clear({scope: "incognito_persistent"}),
      "Firefox does not support the incognito_persistent settings scope.",
      "Expected rejection calling clear with invalid scope.");

    browser.test.notifyPass("exceptionTests");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["privacy"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("exceptionTests");
  await extension.unload();
});
