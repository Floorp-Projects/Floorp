/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var TEST_PLUGIN_DESCRIPTION = "Flash plug-in for testing purposes.";

// This verifies that plugins exist and can be enabled and disabled.
var gID = null;

function setTestPluginState(state) {
  let tags = AM_Cc["@mozilla.org/plugin/host;1"].getService(AM_Ci.nsIPluginHost)
    .getPluginTags();
  for (let tag of tags) {
    do_print("Checking tag: " + tag.description);
    if (tag.description == TEST_PLUGIN_DESCRIPTION) {
      tag.enabledState = state;
      return;
    }
  }
  throw Error("No plugin tag found for the test plugin");
}

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("plugin.load_flash_only", false);

  setTestPluginState(AM_Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  startupManager();
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  run_test_1();
}

// Finds the test plugin library
function get_test_plugin() {
  var pluginEnum = Services.dirsvc.get("APluginsDL", AM_Ci.nsISimpleEnumerator);
  while (pluginEnum.hasMoreElements()) {
    let dir = pluginEnum.getNext().QueryInterface(AM_Ci.nsILocalFile);
    let plugin = dir.clone();
    // OSX plugin
    plugin.append("npswftest.plugin");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    plugin = dir.clone();
    // *nix plugin
    plugin.append("libnpswftest.so");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    // Windows plugin
    plugin = dir.clone();
    plugin.append("npswftest.dll");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
  }
  return null;
}

function getFileSize(aFile) {
  if (!aFile.isDirectory())
    return aFile.fileSize;

  let size = 0;
  let entries = aFile.directoryEntries.QueryInterface(AM_Ci.nsIDirectoryEnumerator);
  let entry;
  while ((entry = entries.nextFile))
    size += getFileSize(entry);
  entries.close();
  return size;
}

function getPluginLastModifiedTime(aPluginFile) {
  // On OS X we use the bundle contents last modified time as using
  // the package directories modified date may be outdated.
  // See bug 313700.
  try {
    let localFileMac = aPluginFile.QueryInterface(AM_Ci.nsILocalFileMac);
    if (localFileMac) {
      return localFileMac.bundleContentsLastModifiedTime;
    }
  } catch (e) {
  }

  return aPluginFile.lastModifiedTime;
}

// Tests that the test plugin exists
function run_test_1() {
  var testPlugin = get_test_plugin();
  do_check_neq(testPlugin, null);

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    do_check_true(addons.length > 0);

    addons.forEach(function(p) {
      if (p.description == TEST_PLUGIN_DESCRIPTION)
        gID = p.id;
    });

    do_check_neq(gID, null);

    AddonManager.getAddonByID(gID, function(p) {
      do_check_neq(p, null);
      do_check_eq(p.name, "Shockwave Flash");
      do_check_eq(p.description, TEST_PLUGIN_DESCRIPTION);
      do_check_eq(p.creator, null);
      do_check_eq(p.version, "1.0.0.0");
      do_check_eq(p.type, "plugin");
      do_check_eq(p.userDisabled, "askToActivate");
      do_check_false(p.appDisabled);
      do_check_true(p.isActive);
      do_check_true(p.isCompatible);
      do_check_true(p.providesUpdatesSecurely);
      do_check_eq(p.blocklistState, 0);
      do_check_eq(p.permissions, AddonManager.PERM_CAN_DISABLE | AddonManager.PERM_CAN_ENABLE);
      do_check_eq(p.pendingOperations, 0);
      do_check_true(p.size > 0);
      do_check_eq(p.size, getFileSize(testPlugin));
      do_check_true(p.updateDate > 0);
      do_check_true("isCompatibleWith" in p);
      do_check_true("findUpdates" in p);

      let lastModifiedTime = getPluginLastModifiedTime(testPlugin);
      do_check_eq(p.updateDate.getTime(), lastModifiedTime);
      do_check_eq(p.installDate.getTime(), lastModifiedTime);

      run_test_2(p);
    });
  });
}

// Tests that disabling a plugin works
function run_test_2(p) {
  let test = {};
  test[gID] = [
    ["onDisabling", false],
    "onDisabled",
    ["onPropertyChanged", ["userDisabled"]]
  ];
  prepare_test(test);

  p.userDisabled = true;

  ensure_test_completed();

  do_check_true(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_false(p.isActive);

  AddonManager.getAddonByID(gID, function(p2) {
    do_check_neq(p2, null);
    do_check_true(p2.userDisabled);
    do_check_false(p2.appDisabled);
    do_check_false(p2.isActive);
    do_check_eq(p2.name, "Shockwave Flash");

    run_test_3(p2);
  });
}

// Tests that enabling a plugin works
function run_test_3(p) {
  let test = {};
  test[gID] = [
    ["onEnabling", false],
    "onEnabled"
  ];
  prepare_test(test);

  p.userDisabled = false;

  ensure_test_completed();

  do_check_false(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_true(p.isActive);

  AddonManager.getAddonByID(gID, function(p2) {
    do_check_neq(p2, null);
    do_check_false(p2.userDisabled);
    do_check_false(p2.appDisabled);
    do_check_true(p2.isActive);
    do_check_eq(p2.name, "Shockwave Flash");

    do_execute_soon(run_test_4);
  });
}

// Verify that after a restart the test plugin has the same ID
function run_test_4() {
  restartManager();

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_eq(p.name, "Shockwave Flash");

    Services.prefs.clearUserPref("plugins.click_to_play");

    do_execute_soon(do_test_finished);
  });
}
