/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const OPENH264_PLUGIN_ID       = "gmp-gmpopenh264";
const OPENH264_PREF_BRANCH     = "media." + OPENH264_PLUGIN_ID + ".";
const OPENH264_PREF_ENABLED    = OPENH264_PREF_BRANCH + "enabled";
const OPENH264_PREF_PATH       = OPENH264_PREF_BRANCH + "path";
const OPENH264_PREF_VERSION    = OPENH264_PREF_BRANCH + "version";
const OPENH264_PREF_LASTUPDATE = OPENH264_PREF_BRANCH + "lastUpdate";
const OPENH264_PREF_AUTOUPDATE = OPENH264_PREF_BRANCH + "autoupdate";

XPCOMUtils.defineLazyGetter(this, "pluginsBundle",
  () => Services.strings.createBundle("chrome://global/locale/plugins.properties"));

let gProfileDir = null;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
  run_next_test();
}

add_task(function* test_notInstalled() {
  Services.prefs.setCharPref(OPENH264_PREF_PATH, "");
  Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, false);

  let addons = yield promiseAddonsByIDs([OPENH264_PLUGIN_ID]);
  Assert.equal(addons.length, 1);
  let addon = addons[0];

  Assert.ok(!addon.isInstalled);
  Assert.equal(addon.type, "plugin");
  Assert.equal(addon.version, "");

  let name = pluginsBundle.GetStringFromName("openH264_name");
  Assert.equal(addon.name, name);
  let description = pluginsBundle.GetStringFromName("openH264_description");
  Assert.equal(addon.description, description);

  Assert.ok(!addon.isActive);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.userDisabled);

  Assert.equal(addon.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(addon.size, 0);
  Assert.equal(addon.scope, AddonManager.SCOPE_APPLICATION);
  Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  Assert.equal(addon.operationsRequiringRestart, AddonManager.PENDING_NONE);

  Assert.equal(addon.permissions, AddonManager.PERM_CAN_UPGRADE |
                                  AddonManager.PERM_CAN_ENABLE);

  Assert.equal(addon.updateDate, null);

  Assert.ok(addon.isCompatible);
  Assert.ok(addon.isPlatformCompatible);
  Assert.ok(addon.providesUpdatesSecurely);
  Assert.ok(!addon.foreignInstall);

  let mimetypes = addon.pluginMimeTypes;
  Assert.ok(mimetypes);
  Assert.equal(mimetypes.length, 0);
  let libraries = addon.pluginLibraries;
  Assert.ok(libraries);
  Assert.equal(libraries.length, 0);
  Assert.equal(addon.pluginFullpath, "");
});

add_task(function* test_installed() {
  const TEST_DATE = new Date(2013, 0, 1, 12);
  const TEST_VERSION = "1.2.3.4";

  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("openh264");
  file.append("testDir");

  Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, false);
  Services.prefs.setCharPref(OPENH264_PREF_LASTUPDATE, "" + TEST_DATE.getTime());
  Services.prefs.setCharPref(OPENH264_PREF_VERSION, TEST_VERSION);
  Services.prefs.setCharPref(OPENH264_PREF_PATH, file.path);

  let addons = yield promiseAddonsByIDs([OPENH264_PLUGIN_ID]);
  Assert.equal(addons.length, 1);
  let addon = addons[0];

  Assert.ok(addon.isInstalled);
  Assert.equal(addon.type, "plugin");
  let name = pluginsBundle.GetStringFromName("openH264_name");
  Assert.equal(addon.name, name);
  Assert.equal(addon.version, TEST_VERSION);

  Assert.ok(!addon.isActive);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.userDisabled);

  Assert.equal(addon.permissions, AddonManager.PERM_CAN_UPGRADE |
                                  AddonManager.PERM_CAN_ENABLE);

  Assert.equal(addon.updateDate.getTime(), TEST_DATE.getTime());

  let mimetypes = addon.pluginMimeTypes;
  Assert.ok(mimetypes);
  Assert.equal(mimetypes.length, 0);
  let libraries = addon.pluginLibraries;
  Assert.ok(libraries);
  Assert.equal(libraries.length, 1);
  Assert.equal(libraries[0], "testDir");
  let fullpath = addon.pluginFullpath;
  Assert.equal(fullpath.length, 1);
  Assert.equal(fullpath[0], file.path);
});

add_task(function* test_enable() {
  Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, true);

  let addons = yield promiseAddonsByIDs([OPENH264_PLUGIN_ID]);
  Assert.equal(addons.length, 1);
  let addon = addons[0];

  Assert.ok(addon.isActive);
  Assert.ok(!addon.appDisabled);
  Assert.ok(!addon.userDisabled);

  Assert.equal(addon.permissions, AddonManager.PERM_CAN_UPGRADE |
                                  AddonManager.PERM_CAN_DISABLE);
});

add_task(function* test_autoUpdatePrefPersistance() {
  Services.prefs.clearUserPref(OPENH264_PREF_AUTOUPDATE);
  let addons = yield promiseAddonsByIDs([OPENH264_PLUGIN_ID]);
  let prefs = Services.prefs;
  Assert.equal(addons.length, 1);
  let addon = addons[0];

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
  Assert.ok(!prefs.getBoolPref(OPENH264_PREF_AUTOUPDATE));

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
  Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_ENABLE);
  Assert.ok(prefs.getBoolPref(OPENH264_PREF_AUTOUPDATE));

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
  Assert.ok(!prefs.prefHasUserValue(OPENH264_PREF_AUTOUPDATE));
});

add_task(function* test_pluginRegistration() {
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("openh264");
  file.append("testDir");

  let addedPath = null
  let removedPath = null;
  let clearPaths = () => addedPath = removedPath = null;

  let MockGMPService = {
    addPluginDirectory: path => addedPath = path,
    removePluginDirectory: path => removedPath = path,
  };

  let OpenH264Scope = Cu.import("resource://gre/modules/addons/OpenH264Provider.jsm");
  OpenH264Scope.gmpService = MockGMPService;

  // Check that the OpenH264 plugin gets registered after startup.
  Services.prefs.setCharPref(OPENH264_PREF_PATH, file.path);
  clearPaths();
  yield promiseRestartManager();
  Assert.equal(addedPath, file.path);
  Assert.equal(removedPath, null);

  // Check that clearing the path doesn't trigger registration.
  clearPaths();
  Services.prefs.clearUserPref(OPENH264_PREF_PATH);
  Assert.equal(addedPath, null);
  Assert.equal(removedPath, file.path);

  // Restarting with no path set should not trigger registration.
  clearPaths();
  yield promiseRestartManager();
  Assert.equal(addedPath, null);
  Assert.equal(removedPath, null);

  // Changing the pref mid-session should cause unregistration and registration.
  Services.prefs.setCharPref(OPENH264_PREF_PATH, file.path);
  clearPaths();
  let file2 = file.clone();
  file2.append("foo");
  Services.prefs.setCharPref(OPENH264_PREF_PATH, file2.path);
  Assert.equal(addedPath, file2.path);
  Assert.equal(removedPath, file.path);
});
