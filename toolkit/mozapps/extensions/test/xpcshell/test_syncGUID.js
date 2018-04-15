/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/Services.jsm");

// restartManager() mucks with XPIProvider.jsm importing, so we hack around.
this.__defineGetter__("XPIProvider", function() {
  let scope = {};
  return ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", scope)
                   .XPIProvider;
});

const addonId = "addon1@tests.mozilla.org";

function run_test() {
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  run_next_test();
}

const ADDONS = [
  {
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    bootstrap: true,
    description: "Test Description",

    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"}],
  },
  {
    id: "addon2@tests.mozilla.org",
    version: "2.0",
    name: "Real Test 2",
    bootstrap: true,
    description: "Test Description",

    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"}],
  },
];

const XPIS = ADDONS.map(addon => createTempXPIFile(addon));

const UUID_PATTERN = /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i;

add_test(async function test_getter_and_setter() {
  // Our test add-on requires a restart.
  let listener = {
    onInstallEnded: function onInstallEnded() {
     AddonManager.removeInstallListener(listener);
     // never restart directly inside an onInstallEnded handler!
     executeSoon(async function getter_setter_install_ended() {
      restartManager();

      let addon = await AddonManager.getAddonByID(addonId);
      Assert.notEqual(addon, null);
      Assert.notEqual(addon.syncGUID, null);
      Assert.ok(UUID_PATTERN.test(addon.syncGUID));

      let newGUID = "foo";

      addon.syncGUID = newGUID;
      Assert.equal(newGUID, addon.syncGUID);

      // Verify change made it to DB.
      let newAddon = await AddonManager.getAddonByID(addonId);
      Assert.notEqual(newAddon, null);
      Assert.equal(newGUID, newAddon.syncGUID);

      run_next_test();
     });
    }
  };

  AddonManager.addInstallListener(listener);

  let install = await AddonManager.getInstallForFile(XPIS[0]);
  install.install();
});

add_test(async function test_fetch_by_guid_unknown_guid() {
  let addon = await XPIProvider.getAddonBySyncGUID("XXXX");
  Assert.equal(null, addon);
  run_next_test();
});

// Ensure setting an extension to an existing syncGUID results in error.
add_test(async function test_error_on_duplicate_syncguid_insert() {
  const installNames = ["test_install1", "test_install2_1"];
  const installIDs = ["addon1@tests.mozilla.org", "addon2@tests.mozilla.org"];

  let installCount = 0;

  let listener = {
    onInstallEnded: function onInstallEnded() {
      installCount++;

      if (installCount == installNames.length) {
       AddonManager.removeInstallListener(listener);
       executeSoon(async function duplicate_syncguid_install_ended() {
        restartManager();

        let addons = await AddonManager.getAddonsByIDs(installIDs);
        let initialGUID = addons[1].syncGUID;

        try {
          addons[1].syncGUID = addons[0].syncGUID;
          do_throw("Should not get here.");
        } catch (e) {
          Assert.ok(e.message.startsWith("Addon sync GUID conflict"));
          restartManager();

          let addon = await AddonManager.getAddonByID(installIDs[1]);
          Assert.equal(initialGUID, addon.syncGUID);
          run_next_test();
        }
       });
      }
    }
  };

  AddonManager.addInstallListener(listener);

  for (let xpi of XPIS) {
    let install = await AddonManager.getInstallForFile(xpi);
    install.install();
  }
});

add_test(async function test_fetch_by_guid_known_guid() {
  let addon = await AddonManager.getAddonByID(addonId);
  Assert.notEqual(null, addon);
  Assert.notEqual(null, addon.syncGUID);

  let syncGUID = addon.syncGUID;

  let newAddon = await XPIProvider.getAddonBySyncGUID(syncGUID);
  Assert.notEqual(null, newAddon);
  Assert.equal(syncGUID, newAddon.syncGUID);

  run_next_test();
});

add_test(async function test_addon_manager_get_by_sync_guid() {
  let addon = await AddonManager.getAddonByID(addonId);
  Assert.notEqual(null, addon.syncGUID);

  let syncGUID = addon.syncGUID;

  let newAddon = await AddonManager.getAddonBySyncGUID(syncGUID);
  Assert.notEqual(null, newAddon);
  Assert.equal(addon.id, newAddon.id);
  Assert.equal(syncGUID, newAddon.syncGUID);

  let missing = await AddonManager.getAddonBySyncGUID("DOES_NOT_EXIST");
  Assert.equal(undefined, missing);

  run_next_test();
});
