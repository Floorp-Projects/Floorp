"use strict";

const { JSONFile } = ChromeUtils.importESModule(
  "resource://gre/modules/JSONFile.sys.mjs"
);

const aomStartup = Cc["@mozilla.org/addons/addon-manager-startup;1"].getService(
  Ci.amIAddonManagerStartup
);

const gProfDir = do_get_profile();

Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION
);
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42.0", "42.0");

const DUMMY_ID = "@dummy";
const DUMMY_ADDONS = {
  addons: {
    "@dummy": {
      lastModifiedTime: 1337,
      rootURI: "resource:///modules/themes/dummy/",
      version: "1",
    },
  },
};

const TEST_ADDON_ID = "@test-theme";
const TEST_THEME = {
  lastModifiedTime: 1337,
  rootURI: "resource:///modules/themes/test/",
  version: "1",
};

const TEST_ADDONS = {
  addons: {
    "@test-theme": TEST_THEME,
  },
};

// Utility to write out various addonStartup.json files.
async function writeAOMStartupData(data) {
  let jsonFile = new JSONFile({
    path: PathUtils.join(gProfDir.path, "addonStartup.json.lz4"),
    compression: "lz4",
  });
  jsonFile.data = data;
  await jsonFile._save();
  return aomStartup.readStartupData();
}

// This tests that any buitin removed from the build will
// get removed from the state data.
add_task(async function test_startup_missing_builtin() {
  let startupData = await writeAOMStartupData({
    "app-builtin": DUMMY_ADDONS,
  });
  Assert.ok(
    !!startupData["app-builtin"].addons[DUMMY_ID],
    "non-existant addon is in startup data"
  );

  await AddonTestUtils.promiseStartupManager();
  await AddonTestUtils.promiseShutdownManager();

  // This data is flushed on shutdown, so we check it after shutdown.
  startupData = aomStartup.readStartupData();
  Assert.equal(
    startupData["app-builtin"].addons[DUMMY_ID],
    undefined,
    "non-existant addon is removed from startup data"
  );
});

// This test verifies that a builtin installed prior to the
// second scan is not overwritten by old state data during
// the scan.
add_task(async function test_startup_default_theme_moved() {
  let startupData = await writeAOMStartupData({
    "app-profile": DUMMY_ADDONS,
    "app-builtin": TEST_ADDONS,
  });
  Assert.ok(
    !!startupData["app-profile"].addons[DUMMY_ID],
    "non-existant addon is in startup data"
  );
  Assert.ok(
    !!startupData["app-builtin"].addons[TEST_ADDON_ID],
    "test addon is in startup data"
  );

  let themeDef = {
    manifest: {
      browser_specific_settings: { gecko: { id: TEST_ADDON_ID } },
      version: "1.1",
      theme: {},
    },
  };

  await setupBuiltinExtension(themeDef, "second-loc");
  await AddonTestUtils.promiseStartupManager("44");
  await AddonManager.maybeInstallBuiltinAddon(
    TEST_ADDON_ID,
    "1.1",
    "resource://second-loc/"
  );
  await AddonManagerPrivate.getNewSideloads();

  let addon = await AddonManager.getAddonByID(TEST_ADDON_ID);
  Assert.ok(!addon.foreignInstall, "addon was not marked as a foreign install");
  Assert.equal(addon.version, "1.1", "addon version is correct");

  await AddonTestUtils.promiseShutdownManager();

  // This data is flushed on shutdown, so we check it after shutdown.
  startupData = aomStartup.readStartupData();
  Assert.equal(
    startupData["app-builtin"].addons[TEST_ADDON_ID].version,
    "1.1",
    "startup data is correct in cache"
  );
  Assert.equal(
    startupData["app-builtin"].addons[DUMMY_ID],
    undefined,
    "non-existant addon is removed from startup data"
  );
});

// This test verifies that a builtin addon being updated
// is not marked as a foreignInstall.
add_task(async function test_startup_builtin_not_foreign() {
  let startupData = await writeAOMStartupData({
    "app-profile": DUMMY_ADDONS,
    "app-builtin": {
      addons: {
        "@test-theme": {
          ...TEST_THEME,
          rootURI: "resource://second-loc/",
        },
      },
    },
  });
  Assert.ok(
    !!startupData["app-profile"].addons[DUMMY_ID],
    "non-existant addon is in startup data"
  );
  Assert.ok(
    !!startupData["app-builtin"].addons[TEST_ADDON_ID],
    "test addon is in startup data"
  );

  let themeDef = {
    manifest: {
      browser_specific_settings: { gecko: { id: TEST_ADDON_ID } },
      version: "1.1",
      theme: {},
    },
  };

  await setupBuiltinExtension(themeDef, "second-loc");
  await AddonTestUtils.promiseStartupManager("43");
  await AddonManager.maybeInstallBuiltinAddon(
    TEST_ADDON_ID,
    "1.1",
    "resource://second-loc/"
  );
  await AddonManagerPrivate.getNewSideloads();

  let addon = await AddonManager.getAddonByID(TEST_ADDON_ID);
  Assert.ok(!addon.foreignInstall, "addon was not marked as a foreign install");
  Assert.equal(addon.version, "1.1", "addon version is correct");

  await AddonTestUtils.promiseShutdownManager();

  // This data is flushed on shutdown, so we check it after shutdown.
  startupData = aomStartup.readStartupData();
  Assert.equal(
    startupData["app-builtin"].addons[TEST_ADDON_ID].version,
    "1.1",
    "startup data is correct in cache"
  );
  Assert.equal(
    startupData["app-builtin"].addons[DUMMY_ID],
    undefined,
    "non-existant addon is removed from startup data"
  );
});
