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
  const proxySvc = Ci.nsIProtocolProxyService;
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
    "network.proxy.type": proxySvc.PROXYCONFIG_SYSTEM,
    "network.proxy.http": "",
    "network.proxy.http_port": 0,
    "network.proxy.share_proxy_settings": false,
    "network.proxy.ftp": "",
    "network.proxy.ftp_port": 0,
    "network.proxy.ssl": "",
    "network.proxy.ssl_port": 0,
    "network.proxy.socks": "",
    "network.proxy.socks_port": 0,
    "network.proxy.socks_version": 5,
    "network.proxy.socks_remote_dns": false,
    "network.proxy.no_proxies_on": "localhost, 127.0.0.1",
    "network.proxy.autoconfig_url": "",
    "signon.autologin.proxy": false,
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

  registerCleanupFunction(() => {
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

  async function testSetting(setting, value, expected, expectedValue = value) {
    extension.sendMessage("set", setting, value);
    let data = await extension.awaitMessage("settingData");
    deepEqual(data.value, expectedValue,
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
    await testNoOpSetting(
      "contextMenuShowEvent", "mousedown",
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

  async function testProxy(config, expectedPrefs) {
    // proxyConfig is not supported on Android.
    if (AppConstants.platform === "android") {
      return Promise.resolve();
    }

    let proxyConfig = {
      proxyType: "system",
      autoConfigUrl: "",
      autoLogin: false,
      proxyDNS: false,
      httpProxyAll: false,
      socksVersion: 5,
      passthrough: "localhost, 127.0.0.1",
      http: "",
      ftp: "",
      ssl: "",
      socks: "",
    };
    return testSetting(
      "proxyConfig", config, expectedPrefs, Object.assign(proxyConfig, config)
    );
  }

  await testProxy(
    {proxyType: "none"},
    {"network.proxy.type": proxySvc.PROXYCONFIG_DIRECT},
  );

  await testProxy(
    {
      proxyType: "autoDetect",
      autoLogin: true,
      proxyDNS: true,
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_WPAD,
      "signon.autologin.proxy": true,
      "network.proxy.socks_remote_dns": true,
    },
  );

  await testProxy(
    {
      proxyType: "system",
      autoLogin: false,
      proxyDNS: false,
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_SYSTEM,
      "signon.autologin.proxy": false,
      "network.proxy.socks_remote_dns": false,
    },
  );

  await testProxy(
    {
      proxyType: "autoConfig",
      autoConfigUrl: "http://mozilla.org",
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_PAC,
      "network.proxy.autoconfig_url": "http://mozilla.org",
    },
  );

  await testProxy(
    {
      proxyType: "manual",
      http: "http://www.mozilla.org",
      autoConfigUrl: "",
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_MANUAL,
      "network.proxy.http": "http://www.mozilla.org",
      "network.proxy.http_port": 0,
      "network.proxy.autoconfig_url": "",
    }
  );

  await testProxy(
    {
      proxyType: "manual",
      http: "http://www.mozilla.org:8080",
      httpProxyAll: true,
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_MANUAL,
      "network.proxy.http": "http://www.mozilla.org",
      "network.proxy.http_port": 8080,
      "network.proxy.share_proxy_settings": true,
    }
  );

  await testProxy(
    {
      proxyType: "manual",
      http: "http://www.mozilla.org:8080",
      httpProxyAll: false,
      ftp: "http://www.mozilla.org:8081",
      ssl: "http://www.mozilla.org:8082",
      socks: "mozilla.org:8083",
      socksVersion: 4,
      passthrough: ".mozilla.org",
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_MANUAL,
      "network.proxy.http": "http://www.mozilla.org",
      "network.proxy.http_port": 8080,
      "network.proxy.share_proxy_settings": false,
      "network.proxy.ftp": "http://www.mozilla.org",
      "network.proxy.ftp_port": 8081,
      "network.proxy.ssl": "http://www.mozilla.org",
      "network.proxy.ssl_port": 8082,
      "network.proxy.socks": "mozilla.org",
      "network.proxy.socks_port": 8083,
      "network.proxy.socks_version": 4,
      "network.proxy.no_proxies_on": ".mozilla.org",
    }
  );

  // Test resetting values.
  await testProxy(
    {
      proxyType: "none",
      http: "",
      ftp: "",
      ssl: "",
      socks: "",
      socksVersion: 5,
      passthrough: "",
    },
    {
      "network.proxy.type": proxySvc.PROXYCONFIG_DIRECT,
      "network.proxy.http": "",
      "network.proxy.http_port": 0,
      "network.proxy.ftp": "",
      "network.proxy.ftp_port": 0,
      "network.proxy.ssl": "",
      "network.proxy.ssl_port": 0,
      "network.proxy.socks": "",
      "network.proxy.socks_port": 0,
      "network.proxy.socks_version": 5,
      "network.proxy.no_proxies_on": "",
    }
  );

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

add_task(async function test_bad_value_proxy_config() {
  let background = AppConstants.platform === "android" ?
    async () => {
      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "none",
        }}),
        /proxyConfig is not supported on android/,
        "proxyConfig.set rejects on Android.");

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.get({}),
        /android is not a supported platform for the proxyConfig setting/,
        "proxyConfig.get rejects on Android.");

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.clear({}),
        /android is not a supported platform for the proxyConfig setting/,
        "proxyConfig.clear rejects on Android.");

      browser.test.sendMessage("done");
    } :
    async () => {
      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "abc",
        }}),
        /abc is not a valid value for proxyType/,
        "proxyConfig.set rejects with an invalid proxyType value.");

      for (let protocol of ["http", "ftp", "ssl"]) {
        let value = {proxyType: "manual"};
        value[protocol] = "abc";
        await browser.test.assertRejects(
          browser.browserSettings.proxyConfig.set({value}),
          `abc is not a valid value for ${protocol}.`,
          `proxyConfig.set rejects with an invalid ${protocol} value.`);
      }

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "autoConfig",
        }}),
        /undefined is not a valid value for autoConfigUrl/,
        "proxyConfig.set for type autoConfig rejects with an empty autoConfigUrl value.");

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "autoConfig",
          autoConfigUrl: "abc",
        }}),
        /abc is not a valid value for autoConfigUrl/,
        "proxyConfig.set rejects with an invalid autoConfigUrl value.");

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "manual",
          socksVersion: "abc",
        }}),
        /abc is not a valid value for socksVersion/,
        "proxyConfig.set rejects with an invalid socksVersion value.");

      await browser.test.assertRejects(
        browser.browserSettings.proxyConfig.set({value: {
          proxyType: "manual",
          socksVersion: 3,
        }}),
        /3 is not a valid value for socksVersion/,
        "proxyConfig.set rejects with an invalid socksVersion value.");

      browser.test.sendMessage("done");
    };

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
