/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { GMPTestUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/GMPProvider.sys.mjs"
);
const { GMPInstallManager } = ChromeUtils.import(
  "resource://gre/modules/GMPInstallManager.jsm"
);
const {
  GMPPrefs,
  GMP_PLUGIN_IDS,
  OPEN_H264_ID,
  WIDEVINE_ID,
} = ChromeUtils.import("resource://gre/modules/GMPUtils.jsm");
const { UpdateUtils } = ChromeUtils.import(
  "resource://gre/modules/UpdateUtils.jsm"
);

XPCOMUtils.defineLazyGetter(
  this,
  "pluginsBundle",
  () => new Localization(["toolkit/about/aboutPlugins.ftl"])
);

var gMockAddons = new Map();
var gMockEmeAddons = new Map();

const mockH264Addon = Object.freeze({
  id: OPEN_H264_ID,
  isValid: true,
  isInstalled: false,
  nameId: "plugins-openh264-name",
  descriptionId: "plugins-openh264-description",
});
gMockAddons.set(mockH264Addon.id, mockH264Addon);

const mockWidevineAddon = Object.freeze({
  id: WIDEVINE_ID,
  isValid: true,
  isInstalled: false,
  nameId: "plugins-widevine-name",
  descriptionId: "plugins-widevine-description",
});
gMockAddons.set(mockWidevineAddon.id, mockWidevineAddon);
gMockEmeAddons.set(mockWidevineAddon.id, mockWidevineAddon);

var gInstalledAddonId = "";
var gPrefs = Services.prefs;
var gGetKey = GMPPrefs.getPrefKey;

const MockGMPInstallManagerPrototype = {
  checkForAddons: () =>
    Promise.resolve({
      usedFallback: true,
      addons: [...gMockAddons.values()],
    }),

  installAddon: addon => {
    gInstalledAddonId = addon.id;
    return Promise.resolve();
  },
};

add_setup(async () => {
  Assert.deepEqual(
    GMP_PLUGIN_IDS,
    Array.from(gMockAddons.keys()),
    "set of mock addons matches the actual set of plugins"
  );

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // The GMPProvider does not register until the first content process
  // is launched, so we simulate that by firing this notification.
  Services.obs.notifyObservers(null, "ipc:first-content-process-created");

  await promiseStartupManager();

  gPrefs.setBoolPref(GMPPrefs.KEY_LOGGING_DUMP, true);
  gPrefs.setIntPref(GMPPrefs.KEY_LOGGING_LEVEL, 0);
  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, true);
  for (let addon of gMockAddons.values()) {
    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id), true);
    gPrefs.setBoolPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id),
      true
    );
  }
});

add_task(async function test_notInstalled() {
  for (let addon of gMockAddons.values()) {
    gPrefs.setCharPref(gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id), "");
    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), false);
  }

  let addons = await promiseAddonsByIDs([...gMockAddons.keys()]);
  Assert.equal(addons.length, gMockAddons.size);

  for (let addon of addons) {
    Assert.ok(!addon.isInstalled);
    Assert.equal(addon.type, "plugin");
    Assert.equal(addon.version, "");

    let mockAddon = gMockAddons.get(addon.id);

    Assert.notEqual(mockAddon, null);
    let name = await pluginsBundle.formatValue(mockAddon.nameId);
    Assert.equal(addon.name, name);
    let description = await pluginsBundle.formatValue(mockAddon.descriptionId);
    Assert.equal(addon.description, description);

    Assert.ok(!addon.isActive);
    Assert.ok(!addon.appDisabled);
    Assert.ok(addon.userDisabled);

    Assert.equal(
      addon.blocklistState,
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED
    );
    Assert.equal(addon.scope, AddonManager.SCOPE_APPLICATION);
    Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
    Assert.equal(addon.operationsRequiringRestart, AddonManager.PENDING_NONE);

    Assert.equal(
      addon.permissions,
      AddonManager.PERM_CAN_UPGRADE | AddonManager.PERM_CAN_ENABLE
    );

    Assert.equal(addon.updateDate, null);

    Assert.ok(addon.isCompatible);
    Assert.ok(addon.isPlatformCompatible);
    Assert.ok(addon.providesUpdatesSecurely);
    Assert.ok(!addon.foreignInstall);

    let libraries = addon.pluginLibraries;
    Assert.ok(libraries);
    Assert.equal(libraries.length, 0);
    Assert.equal(addon.pluginFullpath, "");
  }
});

add_task(async function test_installed() {
  const TEST_DATE = new Date(2013, 0, 1, 12);
  const TEST_VERSION = "1.2.3.4";
  const TEST_TIME_SEC = Math.round(TEST_DATE.getTime() / 1000);

  let addons = await promiseAddonsByIDs([...gMockAddons.keys()]);
  Assert.equal(addons.length, gMockAddons.size);

  for (let addon of addons) {
    let mockAddon = gMockAddons.get(addon.id);
    Assert.notEqual(mockAddon, null);

    let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
    file.append(addon.id);
    file.append(TEST_VERSION);
    gPrefs.setBoolPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, mockAddon.id),
      false
    );
    gPrefs.setIntPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, mockAddon.id),
      TEST_TIME_SEC
    );
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, mockAddon.id),
      TEST_VERSION
    );

    Assert.ok(addon.isInstalled);
    Assert.equal(addon.type, "plugin");
    Assert.ok(!addon.isActive);
    Assert.ok(!addon.appDisabled);
    Assert.ok(addon.userDisabled);

    let name = await pluginsBundle.formatValue(mockAddon.nameId);
    Assert.equal(addon.name, name);
    Assert.equal(addon.version, TEST_VERSION);

    Assert.equal(
      addon.permissions,
      AddonManager.PERM_CAN_UPGRADE | AddonManager.PERM_CAN_ENABLE
    );

    Assert.equal(addon.updateDate.getTime(), TEST_TIME_SEC * 1000);

    let libraries = addon.pluginLibraries;
    Assert.ok(libraries);
    Assert.equal(libraries.length, 1);
    Assert.equal(libraries[0], TEST_VERSION);
    let fullpath = addon.pluginFullpath;
    Assert.equal(fullpath.length, 1);
    Assert.equal(fullpath[0], file.path);
  }
});

add_task(async function test_enable() {
  let addons = await promiseAddonsByIDs([...gMockAddons.keys()]);
  Assert.equal(addons.length, gMockAddons.size);

  for (let addon of addons) {
    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), true);

    Assert.ok(addon.isActive);
    Assert.ok(!addon.appDisabled);
    Assert.ok(!addon.userDisabled);

    Assert.equal(
      addon.permissions,
      AddonManager.PERM_CAN_UPGRADE | AddonManager.PERM_CAN_DISABLE
    );
  }
});

add_task(async function test_globalEmeDisabled() {
  let addons = await promiseAddonsByIDs([...gMockEmeAddons.keys()]);
  Assert.equal(addons.length, gMockEmeAddons.size);

  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, false);
  for (let addon of addons) {
    Assert.ok(!addon.isActive);
    Assert.ok(addon.appDisabled);
    Assert.ok(!addon.userDisabled);

    Assert.equal(addon.permissions, 0);
  }
  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, true);
});

add_task(async function test_autoUpdatePrefPersistance() {
  let addons = await promiseAddonsByIDs([...gMockAddons.keys()]);
  Assert.equal(addons.length, gMockAddons.size);

  for (let addon of addons) {
    let autoupdateKey = gGetKey(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id);
    gPrefs.clearUserPref(autoupdateKey);

    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
    Assert.ok(!gPrefs.getBoolPref(autoupdateKey));

    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
    Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_ENABLE);
    Assert.ok(gPrefs.getBoolPref(autoupdateKey));

    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
    Assert.ok(!gPrefs.prefHasUserValue(autoupdateKey));
  }
});

function createMockPluginFilesIfNeeded(aFile, aPluginId) {
  function createFile(aFileName) {
    let f = aFile.clone();
    f.append(aFileName);
    if (!f.exists()) {
      f.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    }
  }

  let id = aPluginId.substring(4);
  let libName = AppConstants.DLL_PREFIX + id + AppConstants.DLL_SUFFIX;

  createFile(libName);
  if (aPluginId == WIDEVINE_ID) {
    createFile("manifest.json");
  } else {
    createFile(id + ".info");
  }
}

add_task(async function test_pluginRegistration() {
  const TEST_VERSION = "1.2.3.4";

  let addedPaths = [];
  let removedPaths = [];
  let clearPaths = () => {
    addedPaths = [];
    removedPaths = [];
  };

  const MockGMPService = {
    addPluginDirectory: path => {
      if (!addedPaths.includes(path)) {
        addedPaths.push(path);
      }
    },
    removePluginDirectory: path => {
      if (!removedPaths.includes(path)) {
        removedPaths.push(path);
      }
    },
    removeAndDeletePluginDirectory: path => {
      if (!removedPaths.includes(path)) {
        removedPaths.push(path);
      }
    },
  };

  let profD = do_get_profile();
  for (let addon of gMockAddons.values()) {
    await GMPTestUtils.overrideGmpService(MockGMPService, () =>
      testAddon(addon)
    );
  }

  async function testAddon(addon) {
    let file = profD.clone();
    file.append(addon.id);
    file.append(TEST_VERSION);

    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), true);

    // Test that plugin registration fails if the plugin dynamic library and
    // info files are not present.
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION
    );
    clearPaths();
    await promiseRestartManager();
    Assert.equal(addedPaths.indexOf(file.path), -1);
    Assert.deepEqual(removedPaths, [file.path]);

    // Create dummy GMP library/info files, and test that plugin registration
    // succeeds during startup, now that we've added GMP info/lib files.
    createMockPluginFilesIfNeeded(file, addon.id);

    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION
    );
    clearPaths();
    await promiseRestartManager();
    Assert.notEqual(addedPaths.indexOf(file.path), -1);
    Assert.deepEqual(removedPaths, []);

    // Setting the ABI to something invalid should cause plugin to be removed at startup.
    clearPaths();
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_ABI, addon.id),
      "invalid-ABI"
    );
    await promiseRestartManager();
    Assert.equal(addedPaths.indexOf(file.path), -1);
    Assert.deepEqual(removedPaths, [file.path]);

    // Setting the ABI to expected ABI should cause registration at startup.
    clearPaths();
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION
    );
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_ABI, addon.id),
      UpdateUtils.ABI
    );
    await promiseRestartManager();
    Assert.notEqual(addedPaths.indexOf(file.path), -1);
    Assert.deepEqual(removedPaths, []);

    // Check that clearing the version doesn't trigger registration.
    clearPaths();
    gPrefs.clearUserPref(gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id));
    Assert.deepEqual(addedPaths, []);
    Assert.deepEqual(removedPaths, [file.path]);

    // Restarting with no version set should not trigger registration.
    clearPaths();
    await promiseRestartManager();
    Assert.equal(addedPaths.indexOf(file.path), -1);
    Assert.equal(removedPaths.indexOf(file.path), -1);

    // Changing the pref mid-session should cause unregistration and registration.
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION
    );
    clearPaths();
    const TEST_VERSION_2 = "5.6.7.8";
    let file2 = Services.dirsvc.get("ProfD", Ci.nsIFile);
    file2.append(addon.id);
    file2.append(TEST_VERSION_2);
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION_2
    );
    Assert.deepEqual(addedPaths, [file2.path]);
    Assert.deepEqual(removedPaths, [file.path]);

    // Disabling the plugin should cause unregistration.
    gPrefs.setCharPref(
      gGetKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      TEST_VERSION
    );
    clearPaths();
    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), false);
    Assert.deepEqual(addedPaths, []);
    Assert.deepEqual(removedPaths, [file.path]);

    // Restarting with the plugin disabled should not cause registration.
    clearPaths();
    await promiseRestartManager();
    Assert.equal(addedPaths.indexOf(file.path), -1);
    Assert.equal(removedPaths.indexOf(file.path), -1);

    // Re-enabling the plugin should cause registration.
    clearPaths();
    gPrefs.setBoolPref(gGetKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), true);
    Assert.deepEqual(addedPaths, [file.path]);
    Assert.deepEqual(removedPaths, []);
  }
});

add_task(async function test_periodicUpdate() {
  // The GMPInstallManager constructor has an empty body,
  // so replacing the prototype is safe.
  let originalInstallManager = GMPInstallManager.prototype;
  GMPInstallManager.prototype = MockGMPInstallManagerPrototype;

  let addons = await promiseAddonsByIDs([...gMockAddons.keys()]);
  Assert.equal(addons.length, gMockAddons.size);

  for (let addon of addons) {
    gPrefs.clearUserPref(gGetKey(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id));

    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
    gPrefs.setIntPref(GMPPrefs.KEY_UPDATE_LAST_CHECK, 0);
    let result = await addon.findUpdates(
      {},
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
    Assert.strictEqual(result, false);

    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
    gPrefs.setIntPref(GMPPrefs.KEY_UPDATE_LAST_CHECK, Date.now() / 1000 - 60);
    result = await addon.findUpdates(
      {},
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
    Assert.strictEqual(result, false);

    const SEC_IN_A_DAY = 24 * 60 * 60;
    gPrefs.setIntPref(
      GMPPrefs.KEY_UPDATE_LAST_CHECK,
      Date.now() / 1000 - 2 * SEC_IN_A_DAY
    );
    gInstalledAddonId = "";
    result = await addon.findUpdates(
      {},
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
    Assert.strictEqual(result, true);
    Assert.equal(gInstalledAddonId, addon.id);
  }

  GMPInstallManager.prototype = originalInstallManager;
});
