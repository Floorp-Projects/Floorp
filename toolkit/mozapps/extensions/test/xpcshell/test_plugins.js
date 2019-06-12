/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var TEST_PLUGIN_DESCRIPTION = "Flash plug-in for testing purposes.";

// This verifies that plugins exist and can be enabled and disabled.
var gID = null;

function setTestPluginState(state) {
  let tags = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost)
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

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("plugin.load_flash_only", false);

  setTestPluginState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseStartupManager();

  run_test_1();
}

// Finds the test plugin library
function get_test_plugin() {
  for (let dir of Services.dirsvc.get("APluginsDL", Ci.nsISimpleEnumerator)) {
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

function getPluginLastModifiedTime(aPluginFile) {
  // On OS X we use the bundle contents last modified time as using
  // the package directories modified date may be outdated.
  // See bug 313700.
  try {
    let localFileMac = aPluginFile.QueryInterface(Ci.nsILocalFileMac);
    if (localFileMac) {
      return localFileMac.bundleContentsLastModifiedTime;
    }
  } catch (e) {
  }

  return aPluginFile.lastModifiedTime;
}

// Tests that the test plugin exists
async function run_test_1() {
  var testPlugin = get_test_plugin();
  Assert.notEqual(testPlugin, null);

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  Assert.ok(addons.length > 0);

  addons.forEach(function(p) {
    if (p.description == TEST_PLUGIN_DESCRIPTION)
      gID = p.id;
  });

  Assert.notEqual(gID, null);

  let p = await AddonManager.getAddonByID(gID);
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
  Assert.ok(p.updateDate > 0);
  Assert.ok("isCompatibleWith" in p);
  Assert.ok("findUpdates" in p);

  let lastModifiedTime = getPluginLastModifiedTime(testPlugin);
  Assert.equal(p.updateDate.getTime(), lastModifiedTime);
  Assert.equal(p.installDate.getTime(), lastModifiedTime);

  run_test_2(p);
}

// Tests that disabling a plugin works
async function run_test_2(p) {
  await expectEvents(
    {
      addonEvents: {
        [gID]: [
          {event: "onDisabling"},
          {event: "onDisabled"},
          {event: "onPropertyChanged",
           properties: ["userDisabled"]},
        ],
      },
    },
    () => p.disable());

  Assert.ok(p.userDisabled);
  Assert.ok(!p.appDisabled);
  Assert.ok(!p.isActive);

  let p2 = await AddonManager.getAddonByID(gID);
  Assert.notEqual(p2, null);
  Assert.ok(p2.userDisabled);
  Assert.ok(!p2.appDisabled);
  Assert.ok(!p2.isActive);
  Assert.equal(p2.name, "Shockwave Flash");

  run_test_3(p2);
}

// Tests that enabling a plugin works
async function run_test_3(p) {
  await expectEvents(
    {
      addonEvents: {
        [gID]: [
          {event: "onEnabling"},
          {event: "onEnabled"},
        ],
      },
    },
    () => p.enable());

  Assert.ok(!p.userDisabled);
  Assert.ok(!p.appDisabled);
  Assert.ok(p.isActive);

  let p2 = await AddonManager.getAddonByID(gID);
  Assert.notEqual(p2, null);
  Assert.ok(!p2.userDisabled);
  Assert.ok(!p2.appDisabled);
  Assert.ok(p2.isActive);
  Assert.equal(p2.name, "Shockwave Flash");

  executeSoon(run_test_4);
}

// Verify that after a restart the test plugin has the same ID
async function run_test_4() {
  await promiseRestartManager();

  let p = await AddonManager.getAddonByID(gID);
  Assert.notEqual(p, null);
  Assert.equal(p.name, "Shockwave Flash");

  Services.prefs.clearUserPref("plugins.click_to_play");

  executeSoon(do_test_finished);
}
