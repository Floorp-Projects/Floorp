/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that plugins exist and can be enabled and disabled.
var gID = null;

function setTestPluginState(state) {
  let tags = AM_Cc["@mozilla.org/plugin/host;1"].getService(AM_Ci.nsIPluginHost)
    .getPluginTags();
  for (let tag of tags) {
    if (tag.name == "Test Plug-in") {
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
    plugin.append("Test.plugin");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    plugin = dir.clone();
    // *nix plugin
    plugin.append("libnptest.so");
    if (plugin.exists()) {
      plugin.normalize();
      return plugin;
    }
    // Windows plugin
    plugin = dir.clone();
    plugin.append("nptest.dll");
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
  while (entry = entries.nextFile)
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
      if (p.name == "Test Plug-in")
        gID = p.id;
    });

    do_check_neq(gID, null);

    AddonManager.getAddonByID(gID, function(p) {
      do_check_neq(p, null);
      do_check_eq(p.name, "Test Plug-in");
      do_check_eq(p.description,
                  "Plug-in for testing purposes.\u2122 " +
                    "(\u0939\u093f\u0928\u094d\u0926\u0940 " +
                    "\u4e2d\u6587 " +
                    "\u0627\u0644\u0639\u0631\u0628\u064a\u0629)");
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

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_true(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_false(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    run_test_3(p);
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

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_false(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_true(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    do_execute_soon(run_test_4);
  });
}

// Verify that after a restart the test plugin has the same ID
function run_test_4() {
  restartManager();

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_eq(p.name, "Test Plug-in");

    Services.prefs.clearUserPref("plugins.click_to_play");

    do_execute_soon(do_test_finished);
  });
}
