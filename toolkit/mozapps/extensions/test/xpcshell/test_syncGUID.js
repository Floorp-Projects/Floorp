/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// restartManager() mucks with XPIProvider.jsm importing, so we hack around.
this.__defineGetter__("XPIProvider", function() {
  let scope = {};
  return ChromeUtils.import(
    "resource://gre/modules/addons/XPIProvider.jsm",
    scope
  ).XPIProvider;
});

const UUID_PATTERN = /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i;

const ADDONS = [
  {
    id: "addon1@tests.mozilla.org",
    name: "Test 1",
  },
  {
    id: "addon2@tests.mozilla.org",
    name: "Real Test 2",
  },
];

let xpis;

add_task(async function setup() {
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  await promiseStartupManager();

  xpis = await Promise.all(
    ADDONS.map(info =>
      createTempWebExtensionFile({
        manifest: {
          name: info.name,
          applications: { gecko: { id: info.id } },
        },
      })
    )
  );
});

add_task(async function test_getter_and_setter() {
  await promiseInstallFile(xpis[0]);

  let addon = await AddonManager.getAddonByID(ADDONS[0].id);
  Assert.notEqual(addon, null);
  Assert.notEqual(addon.syncGUID, null);
  Assert.ok(UUID_PATTERN.test(addon.syncGUID));

  let newGUID = "foo";

  addon.syncGUID = newGUID;
  Assert.equal(newGUID, addon.syncGUID);

  // Verify change made it to DB.
  let newAddon = await AddonManager.getAddonByID(ADDONS[0].id);
  Assert.notEqual(newAddon, null);
  Assert.equal(newGUID, newAddon.syncGUID);
});

add_task(async function test_fetch_by_guid_unknown_guid() {
  let addon = await XPIProvider.getAddonBySyncGUID("XXXX");
  Assert.equal(null, addon);
});

// Ensure setting an extension to an existing syncGUID results in error.
add_task(async function test_error_on_duplicate_syncguid_insert() {
  await promiseInstallAllFiles(xpis);

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));
  let initialGUID = addons[1].syncGUID;

  Assert.throws(
    () => {
      addons[1].syncGUID = addons[0].syncGUID;
    },
    /Addon sync GUID conflict/,
    "Assigning conflicting sync guids throws"
  );

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ADDONS[1].id);
  Assert.equal(initialGUID, addon.syncGUID);
});

add_task(async function test_fetch_by_guid_known_guid() {
  let addon = await AddonManager.getAddonByID(ADDONS[0].id);
  Assert.notEqual(null, addon);
  Assert.notEqual(null, addon.syncGUID);

  let syncGUID = addon.syncGUID;

  let newAddon = await XPIProvider.getAddonBySyncGUID(syncGUID);
  Assert.notEqual(null, newAddon);
  Assert.equal(syncGUID, newAddon.syncGUID);
});

add_task(async function test_addon_manager_get_by_sync_guid() {
  let addon = await AddonManager.getAddonByID(ADDONS[0].id);
  Assert.notEqual(null, addon.syncGUID);

  let syncGUID = addon.syncGUID;

  let newAddon = await AddonManager.getAddonBySyncGUID(syncGUID);
  Assert.notEqual(null, newAddon);
  Assert.equal(addon.id, newAddon.id);
  Assert.equal(syncGUID, newAddon.syncGUID);

  let missing = await AddonManager.getAddonBySyncGUID("DOES_NOT_EXIST");
  Assert.equal(undefined, missing);
});
