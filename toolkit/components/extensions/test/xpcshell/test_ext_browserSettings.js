/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const SETTINGS_ID = "test_settings_staged_restart_webext@tests.mozilla.org";

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

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
    "browser.tabs.closeTabByDblclick": false,
    "browser.tabs.loadBookmarksInTabs": false,
    "browser.search.openintab": false,
    "browser.tabs.insertRelatedAfterCurrent": true,
    "browser.tabs.insertAfterCurrent": false,
    "browser.display.document_color_use": 1,
    "browser.display.use_document_fonts": 1,
    "browser.zoom.full": true,
    "browser.zoom.siteSpecific": true,
  };

  async function background() {
    let listeners = new Set([]);
    browser.test.onMessage.addListener(async (msg, apiName, value) => {
      let apiObj = browser.browserSettings[apiName];
      if (msg == "get") {
        browser.test.sendMessage("settingData", await apiObj.get({}));
        return;
      }

      // set and setNoOp

      // Don't add more than one listner per apiName.  We leave the
      // listener to ensure we do not get more calls than we expect.
      if (!listeners.has(apiName)) {
        apiObj.onChange.addListener(details => {
          browser.test.sendMessage("onChange", {
            details,
            setting: apiName,
          });
        });
        listeners.add(apiName);
      }
      let result = await apiObj.set({ value });
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
    let dataChange = await extension.awaitMessage("onChange");
    equal(setting, dataChange.setting, "onChange fired");
    equal(
      data.value,
      dataChange.details.value,
      "onChange fired with correct value"
    );
    deepEqual(
      data.value,
      expectedValue,
      `The ${setting} setting has the expected value.`
    );
    equal(
      data.levelOfControl,
      "controlled_by_this_extension",
      `The ${setting} setting has the expected levelOfControl.`
    );
    for (let pref in expected) {
      equal(
        Preferences.get(pref),
        expected[pref],
        `${pref} set correctly for ${value}`
      );
    }
  }

  async function testNoOpSetting(setting, value, expected) {
    extension.sendMessage("setNoOp", setting, value);
    await extension.awaitMessage("no-op set");
    for (let pref in expected) {
      equal(
        Preferences.get(pref),
        expected[pref],
        `${pref} set correctly for ${value}`
      );
    }
  }

  await testSetting("cacheEnabled", false, {
    "browser.cache.disk.enable": false,
    "browser.cache.memory.enable": false,
  });
  await testSetting("cacheEnabled", true, {
    "browser.cache.disk.enable": true,
    "browser.cache.memory.enable": true,
  });

  await testSetting("allowPopupsForUserEvents", false, {
    "dom.popup_allowed_events": "",
  });
  await testSetting("allowPopupsForUserEvents", true, {
    "dom.popup_allowed_events": PREFS["dom.popup_allowed_events"],
  });

  for (let value of ["normal", "none", "once"]) {
    await testSetting("imageAnimationBehavior", value, {
      "image.animation_mode": value,
    });
  }

  await testSetting("webNotificationsDisabled", true, {
    "permissions.default.desktop-notification": PERM_DENY_ACTION,
  });
  await testSetting("webNotificationsDisabled", false, {
    // This pref is not defaulted on Android.
    "permissions.default.desktop-notification":
      AppConstants.MOZ_BUILD_APP !== "browser"
        ? undefined
        : PERM_UNKNOWN_ACTION,
  });

  // This setting is a no-op on Android.
  if (AppConstants.platform === "android") {
    await testNoOpSetting("contextMenuShowEvent", "mouseup", {
      "ui.context_menus.after_mouseup": false,
    });
  } else {
    await testSetting("contextMenuShowEvent", "mouseup", {
      "ui.context_menus.after_mouseup": true,
    });
  }

  // "mousedown" is also a no-op on Windows.
  if (["android", "win"].includes(AppConstants.platform)) {
    await testNoOpSetting("contextMenuShowEvent", "mousedown", {
      "ui.context_menus.after_mouseup": AppConstants.platform === "win",
    });
  } else {
    await testSetting("contextMenuShowEvent", "mousedown", {
      "ui.context_menus.after_mouseup": false,
    });
  }

  if (AppConstants.platform !== "android") {
    await testSetting("closeTabsByDoubleClick", true, {
      "browser.tabs.closeTabByDblclick": true,
    });
    await testSetting("closeTabsByDoubleClick", false, {
      "browser.tabs.closeTabByDblclick": false,
    });
  }

  extension.sendMessage("get", "ftpProtocolEnabled");
  let data = await extension.awaitMessage("settingData");
  equal(data.value, false);
  equal(
    data.levelOfControl,
    "not_controllable",
    `ftpProtocolEnabled is not controllable.`
  );

  await testSetting("newTabPosition", "afterCurrent", {
    "browser.tabs.insertRelatedAfterCurrent": false,
    "browser.tabs.insertAfterCurrent": true,
  });
  await testSetting("newTabPosition", "atEnd", {
    "browser.tabs.insertRelatedAfterCurrent": false,
    "browser.tabs.insertAfterCurrent": false,
  });
  await testSetting("newTabPosition", "relatedAfterCurrent", {
    "browser.tabs.insertRelatedAfterCurrent": true,
    "browser.tabs.insertAfterCurrent": false,
  });

  await testSetting("openBookmarksInNewTabs", true, {
    "browser.tabs.loadBookmarksInTabs": true,
  });
  await testSetting("openBookmarksInNewTabs", false, {
    "browser.tabs.loadBookmarksInTabs": false,
  });

  await testSetting("openSearchResultsInNewTabs", true, {
    "browser.search.openintab": true,
  });
  await testSetting("openSearchResultsInNewTabs", false, {
    "browser.search.openintab": false,
  });

  await testSetting("openUrlbarResultsInNewTabs", true, {
    "browser.urlbar.openintab": true,
  });
  await testSetting("openUrlbarResultsInNewTabs", false, {
    "browser.urlbar.openintab": false,
  });

  await testSetting("overrideDocumentColors", "high-contrast-only", {
    "browser.display.document_color_use": 0,
  });
  await testSetting("overrideDocumentColors", "never", {
    "browser.display.document_color_use": 1,
  });
  await testSetting("overrideDocumentColors", "always", {
    "browser.display.document_color_use": 2,
  });

  await testSetting("useDocumentFonts", false, {
    "browser.display.use_document_fonts": 0,
  });
  await testSetting("useDocumentFonts", true, {
    "browser.display.use_document_fonts": 1,
  });

  await testSetting("zoomFullPage", true, {
    "browser.zoom.full": true,
  });
  await testSetting("zoomFullPage", false, {
    "browser.zoom.full": false,
  });

  await testSetting("zoomSiteSpecific", true, {
    "browser.zoom.siteSpecific": true,
  });
  await testSetting("zoomSiteSpecific", false, {
    "browser.zoom.siteSpecific": false,
  });

  await extension.unload();
  await promiseShutdownManager();
});

add_task(async function test_bad_value() {
  async function background() {
    await browser.test.assertRejects(
      browser.browserSettings.contextMenuShowEvent.set({ value: "bad" }),
      /bad is not a valid value for contextMenuShowEvent/,
      "contextMenuShowEvent.set rejects with an invalid value."
    );

    await browser.test.assertRejects(
      browser.browserSettings.overrideDocumentColors.set({ value: 2 }),
      /2 is not a valid value for overrideDocumentColors/,
      "overrideDocumentColors.set rejects with an invalid value."
    );

    await browser.test.assertRejects(
      browser.browserSettings.overrideDocumentColors.set({ value: "bad" }),
      /bad is not a valid value for overrideDocumentColors/,
      "overrideDocumentColors.set rejects with an invalid value."
    );

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

add_task(async function test_bad_value_android() {
  if (AppConstants.platform !== "android") {
    return;
  }

  async function background() {
    await browser.test.assertRejects(
      browser.browserSettings.closeTabsByDoubleClick.set({ value: true }),
      /android is not a supported platform for the closeTabsByDoubleClick setting/,
      "closeTabsByDoubleClick.set rejects on Android."
    );

    await browser.test.assertRejects(
      browser.browserSettings.closeTabsByDoubleClick.get({}),
      /android is not a supported platform for the closeTabsByDoubleClick setting/,
      "closeTabsByDoubleClick.get rejects on Android."
    );

    await browser.test.assertRejects(
      browser.browserSettings.closeTabsByDoubleClick.clear({}),
      /android is not a supported platform for the closeTabsByDoubleClick setting/,
      "closeTabsByDoubleClick.clear rejects on Android."
    );

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

// Verifies settings remain after a staged update on restart.
add_task(async function delay_updates_settings_after_restart() {
  let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
  AddonTestUtils.registerJSON(server, "/test_update.json", {
    addons: {
      "test_settings_staged_restart_webext@tests.mozilla.org": {
        updates: [
          {
            version: "2.0",
            update_link:
              "http://example.com/addons/test_settings_staged_restart_v2.xpi",
          },
        ],
      },
    },
  });
  const update_xpi = AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Delay Upgrade",
      version: "2.0",
      applications: {
        gecko: { id: SETTINGS_ID },
      },
      permissions: ["browserSettings"],
    },
  });
  server.registerFile(
    `/addons/test_settings_staged_restart_v2.xpi`,
    update_xpi
  );

  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: SETTINGS_ID,
          update_url: `http://example.com/test_update.json`,
        },
      },
      permissions: ["browserSettings"],
    },
    background() {
      browser.runtime.onUpdateAvailable.addListener(async details => {
        if (details) {
          await browser.browserSettings.webNotificationsDisabled.set({
            value: true,
          });
          if (details.version) {
            // This should be the version of the pending update.
            browser.test.assertEq("2.0", details.version, "correct version");
            browser.test.notifyPass("delay");
          }
        } else {
          browser.test.fail("no details object passed");
        }
      });
      browser.test.sendMessage("ready");
    },
  });

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let prefname = "permissions.default.desktop-notification";
  let val = Services.prefs.getIntPref(prefname);
  Assert.notEqual(val, 2, "webNotificationsDisabled pref not set");

  let update = await AddonTestUtils.promiseFindAddonUpdates(extension.addon);
  let install = update.updateAvailable;
  Assert.ok(install, `install is available ${update.error}`);

  await AddonTestUtils.promiseCompleteAllInstalls([install]);

  Assert.equal(install.state, AddonManager.STATE_POSTPONED);
  await extension.awaitFinish("delay");

  // restarting allows upgrade to proceed
  await AddonTestUtils.promiseRestartManager();

  await extension.awaitStartup();

  // If an update is not handled correctly we would fail here.  Bug 1639705.
  val = Services.prefs.getIntPref(prefname);
  Assert.equal(val, 2, "webNotificationsDisabled pref set");

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();

  val = Services.prefs.getIntPref(prefname);
  Assert.notEqual(val, 2, "webNotificationsDisabled pref not set");
});
