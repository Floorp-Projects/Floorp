/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPreferencesManager",
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Currently security.tls.version.min has a different default
// value in Nightly and Beta as opposed to Release builds.
const tlsMinPref = Services.prefs.getIntPref("security.tls.version.min");
if (tlsMinPref != 1 && tlsMinPref != 3) {
  ok(false, "This test expects security.tls.version.min set to 1 or 3.");
}
const tlsMinVer = tlsMinPref === 3 ? "TLSv1.2" : "TLSv1";

add_task(async function test_privacy() {
  // Create an object to hold the values to which we will initialize the prefs.
  const SETTINGS = {
    "network.networkPredictionEnabled": {
      "network.predictor.enabled": true,
      "network.prefetch-next": true,
      // This pref starts with a numerical value and we need to use whatever the
      // default is or we encounter issues when the pref is reset during the test.
      "network.http.speculative-parallel-limit": ExtensionPreferencesManager.getDefaultValue(
        "network.http.speculative-parallel-limit"
      ),
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

  registerCleanupFunction(() => {
    // Reset the prefs.
    for (let setting in SETTINGS) {
      for (let pref in SETTINGS[setting]) {
        Preferences.reset(pref);
      }
    }
  });

  await promiseStartupManager();

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

  for (let extension of testExtensions) {
    await extension.startup();
  }

  for (let setting in SETTINGS) {
    testExtensions[0].sendMessage("get", {}, setting);
    let data = await testExtensions[0].awaitMessage("gotData");
    ok(data.value, "get returns expected value.");
    equal(
      data.levelOfControl,
      "controllable_by_this_extension",
      "get returns expected levelOfControl."
    );

    testExtensions[0].sendMessage("get", { incognito: true }, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(data.value, "get returns expected value with incognito.");
    equal(
      data.levelOfControl,
      "not_controllable",
      "get returns expected levelOfControl with incognito."
    );

    // Change the value to false.
    testExtensions[0].sendMessage("set", { value: false }, setting);
    data = await testExtensions[0].awaitMessage("afterSet");
    ok(!data.value, "get returns expected value after setting.");
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      "get returns expected levelOfControl after setting."
    );

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
    testExtensions[1].sendMessage("set", { value: true }, setting);
    data = await testExtensions[1].awaitMessage("afterSet");
    ok(
      data.value,
      "get returns expected value after setting via newer extension."
    );
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      "get returns expected levelOfControl after setting."
    );

    // Verify the prefs have been set to match the "true" setting.
    for (let pref in SETTINGS[setting]) {
      let msg = `${pref} set correctly for ${setting}`;
      if (pref === "network.http.speculative-parallel-limit") {
        equal(
          Preferences.get(pref),
          ExtensionPreferencesManager.getDefaultValue(pref),
          msg
        );
      } else {
        equal(Preferences.get(pref), SETTINGS[setting][pref], msg);
      }
    }

    // Change the value with an older extension.
    testExtensions[0].sendMessage("set", { value: false }, setting);
    data = await testExtensions[0].awaitMessage("afterSet");
    ok(data.value, "Newer extension remains in control.");
    equal(
      data.levelOfControl,
      "controlled_by_other_extensions",
      "get returns expected levelOfControl when controlled by other."
    );

    // Clear the value of the newer extension.
    testExtensions[1].sendMessage("clear", {}, setting);
    data = await testExtensions[1].awaitMessage("afterClear");
    ok(!data.value, "Older extension gains control.");
    equal(
      data.levelOfControl,
      "controllable_by_this_extension",
      "Expected levelOfControl returned after clearing."
    );

    testExtensions[0].sendMessage("get", {}, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(!data.value, "Current, older extension has control.");
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      "Expected levelOfControl returned after clearing."
    );

    // Set the value again with the newer extension.
    testExtensions[1].sendMessage("set", { value: true }, setting);
    data = await testExtensions[1].awaitMessage("afterSet");
    ok(
      data.value,
      "get returns expected value after setting via newer extension."
    );
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      "get returns expected levelOfControl after setting."
    );

    // Unload the newer extension. Expect the older extension to regain control.
    await testExtensions[1].unload();
    testExtensions[0].sendMessage("get", {}, setting);
    data = await testExtensions[0].awaitMessage("gotData");
    ok(!data.value, "Older extension regained control.");
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      "Expected levelOfControl returned after unloading."
    );

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
    equal(
      data.levelOfControl,
      "controllable_by_this_extension",
      "Expected levelOfControl returned after clearing."
    );

    // Verify that our initial values were restored.
    for (let pref in SETTINGS[setting]) {
      equal(
        Preferences.get(pref),
        SETTINGS[setting][pref],
        `${pref} was reset to its initial value.`
      );
    }
  }

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});

add_task(async function test_privacy_other_prefs() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.tls.version.min");
    Services.prefs.clearUserPref("security.tls.version.max");
  });

  const cookieSvc = Ci.nsICookieService;

  // Create an object to hold the values to which we will initialize the prefs.
  const SETTINGS = {
    "network.webRTCIPHandlingPolicy": {
      "media.peerconnection.ice.default_address_only": false,
      "media.peerconnection.ice.no_host": false,
      "media.peerconnection.ice.proxy_only_if_behind_proxy": false,
      "media.peerconnection.ice.proxy_only": false,
    },
    "network.tlsVersionRestriction": {
      "security.tls.version.min": tlsMinPref,
      "security.tls.version.max": 4,
    },
    "network.peerConnectionEnabled": {
      "media.peerconnection.enabled": true,
    },
    "services.passwordSavingEnabled": {
      "signon.rememberSignons": true,
    },
    "websites.referrersEnabled": {
      "network.http.sendRefererHeader": 2,
    },
    "websites.resistFingerprinting": {
      "privacy.resistFingerprinting": true,
    },
    "websites.firstPartyIsolate": {
      "privacy.firstparty.isolate": true,
    },
    "websites.cookieConfig": {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_ACCEPT,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
  };

  let defaultPrefs = new Preferences({ defaultBranch: true });
  let defaultCookieBehavior = defaultPrefs.get("network.cookie.cookieBehavior");
  let defaultBehavior;
  switch (defaultCookieBehavior) {
    case cookieSvc.BEHAVIOR_ACCEPT:
      defaultBehavior = "allow_all";
      break;
    case cookieSvc.BEHAVIOR_REJECT_FOREIGN:
      defaultBehavior = "reject_third_party";
      break;
    case cookieSvc.BEHAVIOR_REJECT:
      defaultBehavior = "reject_all";
      break;
    case cookieSvc.BEHAVIOR_LIMIT_FOREIGN:
      defaultBehavior = "allow_visited";
      break;
    case cookieSvc.BEHAVIOR_REJECT_TRACKER:
      defaultBehavior = "reject_trackers";
      break;
    default:
      ok(
        false,
        `Unexpected cookie behavior encountered: ${defaultCookieBehavior}`
      );
      break;
  }

  async function background() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      let data = args[0];
      // The second argument is the end of the api name,
      // e.g., "network.webRTCIPHandlingPolicy".
      let apiObj = args[1].split(".").reduce((o, i) => o[i], browser.privacy);
      let settingData;
      switch (msg) {
        case "set":
          try {
            await apiObj.set(data);
          } catch (e) {
            browser.test.sendMessage("settingThrowsException", {
              message: e.message,
            });
            break;
          }
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

  registerCleanupFunction(() => {
    // Reset the prefs.
    for (let setting in SETTINGS) {
      for (let pref in SETTINGS[setting]) {
        Preferences.reset(pref);
      }
    }
  });

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["privacy"],
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  async function testSetting(setting, value, expected, expectedValue = value) {
    extension.sendMessage("set", { value: value }, setting);
    let data = await extension.awaitMessage("settingData");
    deepEqual(
      data.value,
      expectedValue,
      `Got expected result on setting ${setting} to ${uneval(value)}`
    );
    for (let pref in expected) {
      equal(
        Preferences.get(pref),
        expected[pref],
        `${pref} set correctly for ${expected[pref]}`
      );
    }
  }

  async function testSettingException(setting, value, expected) {
    extension.sendMessage("set", { value: value }, setting);
    let data = await extension.awaitMessage("settingThrowsException");
    equal(data.message, expected);
  }

  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "default_public_and_private_interfaces",
    {
      "media.peerconnection.ice.default_address_only": true,
      "media.peerconnection.ice.no_host": false,
      "media.peerconnection.ice.proxy_only_if_behind_proxy": false,
      "media.peerconnection.ice.proxy_only": false,
    }
  );
  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "default_public_interface_only",
    {
      "media.peerconnection.ice.default_address_only": true,
      "media.peerconnection.ice.no_host": true,
      "media.peerconnection.ice.proxy_only_if_behind_proxy": false,
      "media.peerconnection.ice.proxy_only": false,
    }
  );
  await testSetting(
    "network.webRTCIPHandlingPolicy",
    "disable_non_proxied_udp",
    {
      "media.peerconnection.ice.default_address_only": true,
      "media.peerconnection.ice.no_host": true,
      "media.peerconnection.ice.proxy_only_if_behind_proxy": true,
      "media.peerconnection.ice.proxy_only": false,
    }
  );
  await testSetting("network.webRTCIPHandlingPolicy", "proxy_only", {
    "media.peerconnection.ice.default_address_only": false,
    "media.peerconnection.ice.no_host": false,
    "media.peerconnection.ice.proxy_only_if_behind_proxy": false,
    "media.peerconnection.ice.proxy_only": true,
  });
  await testSetting("network.webRTCIPHandlingPolicy", "default", {
    "media.peerconnection.ice.default_address_only": false,
    "media.peerconnection.ice.no_host": false,
    "media.peerconnection.ice.proxy_only_if_behind_proxy": false,
    "media.peerconnection.ice.proxy_only": false,
  });

  await testSetting("network.peerConnectionEnabled", false, {
    "media.peerconnection.enabled": false,
  });
  await testSetting("network.peerConnectionEnabled", true, {
    "media.peerconnection.enabled": true,
  });

  await testSetting("websites.referrersEnabled", false, {
    "network.http.sendRefererHeader": 0,
  });
  await testSetting("websites.referrersEnabled", true, {
    "network.http.sendRefererHeader": 2,
  });

  await testSetting("websites.resistFingerprinting", false, {
    "privacy.resistFingerprinting": false,
  });
  await testSetting("websites.resistFingerprinting", true, {
    "privacy.resistFingerprinting": true,
  });

  await testSetting("websites.firstPartyIsolate", false, {
    "privacy.firstparty.isolate": false,
  });
  await testSetting("websites.firstPartyIsolate", true, {
    "privacy.firstparty.isolate": true,
  });

  await testSetting("websites.trackingProtectionMode", "always", {
    "privacy.trackingprotection.enabled": true,
    "privacy.trackingprotection.pbmode.enabled": true,
  });
  await testSetting("websites.trackingProtectionMode", "never", {
    "privacy.trackingprotection.enabled": false,
    "privacy.trackingprotection.pbmode.enabled": false,
  });
  await testSetting("websites.trackingProtectionMode", "private_browsing", {
    "privacy.trackingprotection.enabled": false,
    "privacy.trackingprotection.pbmode.enabled": true,
  });

  await testSetting("services.passwordSavingEnabled", false, {
    "signon.rememberSignons": false,
  });
  await testSetting("services.passwordSavingEnabled", true, {
    "signon.rememberSignons": true,
  });

  await testSetting(
    "websites.cookieConfig",
    { behavior: "reject_third_party", nonPersistentCookies: true },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_REJECT_FOREIGN,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_SESSION,
    }
  );
  // A missing nonPersistentCookies property should default to false.
  await testSetting(
    "websites.cookieConfig",
    { behavior: "reject_third_party" },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_REJECT_FOREIGN,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: "reject_third_party", nonPersistentCookies: false }
  );
  // A missing behavior property should reset the pref.
  await testSetting(
    "websites.cookieConfig",
    { nonPersistentCookies: true },
    {
      "network.cookie.cookieBehavior": defaultCookieBehavior,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_SESSION,
    },
    { behavior: defaultBehavior, nonPersistentCookies: true }
  );
  await testSetting(
    "websites.cookieConfig",
    { behavior: "reject_all" },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_REJECT,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: "reject_all", nonPersistentCookies: false }
  );
  await testSetting(
    "websites.cookieConfig",
    { behavior: "allow_visited" },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_LIMIT_FOREIGN,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: "allow_visited", nonPersistentCookies: false }
  );
  await testSetting(
    "websites.cookieConfig",
    { behavior: "allow_all" },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_ACCEPT,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: "allow_all", nonPersistentCookies: false }
  );
  await testSetting(
    "websites.cookieConfig",
    { nonPersistentCookies: true },
    {
      "network.cookie.cookieBehavior": defaultCookieBehavior,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_SESSION,
    },
    { behavior: defaultBehavior, nonPersistentCookies: true }
  );
  await testSetting(
    "websites.cookieConfig",
    { nonPersistentCookies: false },
    {
      "network.cookie.cookieBehavior": defaultCookieBehavior,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: defaultBehavior, nonPersistentCookies: false }
  );
  await testSetting(
    "websites.cookieConfig",
    { behavior: "reject_trackers" },
    {
      "network.cookie.cookieBehavior": cookieSvc.BEHAVIOR_REJECT_TRACKER,
      "network.cookie.lifetimePolicy": cookieSvc.ACCEPT_NORMALLY,
    },
    { behavior: "reject_trackers", nonPersistentCookies: false }
  );

  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.2",
      maximum: "TLSv1.3",
    },
    {
      "security.tls.version.min": 3,
      "security.tls.version.max": 4,
    }
  );

  // Single values
  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.3",
    },
    {
      "security.tls.version.min": 4,
      "security.tls.version.max": 4,
    },
    {
      minimum: "TLSv1.3",
      maximum: "TLSv1.3",
    }
  );

  // Single values
  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.3",
    },
    {
      "security.tls.version.min": 4,
      "security.tls.version.max": 4,
    },
    {
      minimum: "TLSv1.3",
      maximum: "TLSv1.3",
    }
  );

  // Invalid values.
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      minimum: "invalid",
      maximum: "invalid",
    },
    "Setting TLS version invalid is not allowed for security reasons."
  );

  // Invalid values.
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      minimum: "invalid2",
    },
    "Setting TLS version invalid2 is not allowed for security reasons."
  );

  // Invalid values.
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      maximum: "invalid3",
    },
    "Setting TLS version invalid3 is not allowed for security reasons."
  );

  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.2",
    },
    {
      "security.tls.version.min": 3,
      "security.tls.version.max": 4,
    },
    {
      minimum: "TLSv1.2",
      maximum: "TLSv1.3",
    }
  );

  await testSetting(
    "network.tlsVersionRestriction",
    {
      maximum: "TLSv1.2",
    },
    {
      "security.tls.version.min": tlsMinPref,
      "security.tls.version.max": 3,
    },
    {
      minimum: tlsMinVer,
      maximum: "TLSv1.2",
    }
  );

  // Not supported version.
  if (tlsMinPref === 3) {
    await testSettingException(
      "network.tlsVersionRestriction",
      {
        minimum: "TLSv1",
      },
      "Setting TLS version TLSv1 is not allowed for security reasons."
    );

    await testSettingException(
      "network.tlsVersionRestriction",
      {
        minimum: "TLSv1.1",
      },
      "Setting TLS version TLSv1.1 is not allowed for security reasons."
    );

    await testSettingException(
      "network.tlsVersionRestriction",
      {
        maximum: "TLSv1",
      },
      "Setting TLS version TLSv1 is not allowed for security reasons."
    );

    await testSettingException(
      "network.tlsVersionRestriction",
      {
        maximum: "TLSv1.1",
      },
      "Setting TLS version TLSv1.1 is not allowed for security reasons."
    );
  }

  // Min vs Max
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.3",
      maximum: "TLSv1.2",
    },
    "Setting TLS min version grater than the max version is not allowed."
  );

  // Min vs Max (with default max)
  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.2",
      maximum: "TLSv1.2",
    },
    {
      "security.tls.version.min": 3,
      "security.tls.version.max": 3,
    }
  );
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.3",
    },
    "Setting TLS min version grater than the max version is not allowed."
  );

  // Max vs Min
  await testSetting(
    "network.tlsVersionRestriction",
    {
      minimum: "TLSv1.3",
      maximum: "TLSv1.3",
    },
    {
      "security.tls.version.min": 4,
      "security.tls.version.max": 4,
    }
  );
  await testSettingException(
    "network.tlsVersionRestriction",
    {
      maximum: "TLSv1.2",
    },
    "Setting TLS max version lower than the min version is not allowed."
  );

  // Empty value.
  await testSetting(
    "network.tlsVersionRestriction",
    {},
    {
      "security.tls.version.min": tlsMinPref,
      "security.tls.version.max": 4,
    },
    {
      minimum: tlsMinVer,
      maximum: "TLSv1.3",
    }
  );

  await extension.unload();

  await promiseShutdownManager();
});

add_task(async function test_exceptions() {
  async function background() {
    await browser.test.assertRejects(
      browser.privacy.network.networkPredictionEnabled.set({
        value: true,
        scope: "regular_only",
      }),
      "Firefox does not support the regular_only settings scope.",
      "Expected rejection calling set with invalid scope."
    );

    await browser.test.assertRejects(
      browser.privacy.network.networkPredictionEnabled.clear({
        scope: "incognito_persistent",
      }),
      "Firefox does not support the incognito_persistent settings scope.",
      "Expected rejection calling clear with invalid scope."
    );

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
