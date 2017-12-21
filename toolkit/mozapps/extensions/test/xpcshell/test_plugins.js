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
    info("Checking tag: " + tag.description);
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
    let dir = pluginEnum.getNext().QueryInterface(AM_Ci.nsIFile);
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
  Assert.notEqual(testPlugin, null);

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    Assert.ok(addons.length > 0);

    addons.forEach(function(p) {
      if (p.description == TEST_PLUGIN_DESCRIPTION)
        gID = p.id;
    });

    Assert.notEqual(gID, null);

    AddonManager.getAddonByID(gID, function(p) {
      Assert.notEqual(p, null);
      Assert.equal(p.name, "Shockwave Flash");
      Assert.equal(p.description, TEST_PLUGIN_DESCRIPTION);
      Assert.equal(p.creator, null);
      Assert.equal(p.version, "1.0.0.0");
      Assert.equal(p.type, "plugin");
      Assert.equal(p.userDisabled, "askToActivate");
      Assert.ok(!p.appDisabled);
      Assert.ok(p.isActive);
      Assert.ok(p.isCompatible);
      Assert.ok(p.providesUpdatesSecurely);
      Assert.equal(p.blocklistState, 0);
      Assert.equal(p.permissions, AddonManager.PERM_CAN_DISABLE | AddonManager.PERM_CAN_ENABLE);
      Assert.equal(p.pendingOperations, 0);
      Assert.ok(p.size > 0);
      Assert.equal(p.size, getFileSize(testPlugin));
      Assert.ok(p.updateDate > 0);
      Assert.ok("isCompatibleWith" in p);
      Assert.ok("findUpdates" in p);

      let lastModifiedTime = getPluginLastModifiedTime(testPlugin);
      Assert.equal(p.updateDate.getTime(), lastModifiedTime);
      Assert.equal(p.installDate.getTime(), lastModifiedTime);

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

  Assert.ok(p.userDisabled);
  Assert.ok(!p.appDisabled);
  Assert.ok(!p.isActive);

  AddonManager.getAddonByID(gID, function(p2) {
    Assert.notEqual(p2, null);
    Assert.ok(p2.userDisabled);
    Assert.ok(!p2.appDisabled);
    Assert.ok(!p2.isActive);
    Assert.equal(p2.name, "Shockwave Flash");

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

  Assert.ok(!p.userDisabled);
  Assert.ok(!p.appDisabled);
  Assert.ok(p.isActive);

  AddonManager.getAddonByID(gID, function(p2) {
    Assert.notEqual(p2, null);
    Assert.ok(!p2.userDisabled);
    Assert.ok(!p2.appDisabled);
    Assert.ok(p2.isActive);
    Assert.equal(p2.name, "Shockwave Flash");

    executeSoon(run_test_4);
  });
}

// Verify that after a restart the test plugin has the same ID
function run_test_4() {
  restartManager();

  AddonManager.getAddonByID(gID, function(p) {
    Assert.notEqual(p, null);
    Assert.equal(p.name, "Shockwave Flash");

    Services.prefs.clearUserPref("plugins.click_to_play");

    executeSoon(do_test_finished);
  });
}
