/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");

// restartManager() mucks with XPIProvider.jsm importing, so we hack around.
this.__defineGetter__("XPIProvider", function () {
  let scope = {};
  return Components.utils.import("resource://gre/modules/XPIProvider.jsm", scope)
                   .XPIProvider;
});

const addonId = "addon1@tests.mozilla.org";

function run_test() {
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  run_next_test();
}

add_test(function test_getter_and_setter() {
  // Our test add-on requires a restart.
  let listener = {
    onInstallEnded: function onInstallEnded() {
      restartManager();

      AddonManager.getAddonByID(addonId, function(addon) {

        do_check_neq(addon, null);
        do_check_neq(addon.syncGUID, null);
        do_check_true(addon.syncGUID.length >= 9);

        let oldGUID = addon.SyncGUID;
        let newGUID = "foo";

        addon.syncGUID = newGUID;
        do_check_eq(newGUID, addon.syncGUID);

        // Verify change made it to DB.
        AddonManager.getAddonByID(addonId, function(newAddon) {
          do_check_neq(newAddon, null);
          do_check_eq(newGUID, newAddon.syncGUID);
        });

        AddonManager.removeInstallListener(listener);

        run_next_test();
      });
    }
  };

  AddonManager.addInstallListener(listener);

  AddonManager.getInstallForFile(do_get_addon("test_install1"),
                                 function(install) {
    install.install();
  });
});

add_test(function test_fetch_by_guid_unknown_guid() {
  XPIProvider.getAddonBySyncGUID("XXXX", function(addon) {
    do_check_eq(null, addon);
    run_next_test();
  });
});

// Ensure setting an extension to an existing syncGUID results in error.
add_test(function test_error_on_duplicate_syncguid_insert() {
  const installNames = ["test_install1", "test_install2_1"];
  const installIDs = ["addon1@tests.mozilla.org", "addon2@tests.mozilla.org"];

  let installCount = 0;

  let listener = {
    onInstallEnded: function onInstallEnded() {
      installCount++;

      if (installCount == installNames.length) {
        AddonManager.removeInstallListener(listener);
        restartManager();

        AddonManager.getAddonsByIDs(installIDs, function(addons) {
          let initialGUID = addons[1].syncGUID;

          try {
            addons[1].syncGUID = addons[0].syncGUID;
            do_throw("Should not get here.");
          }
          catch (e) {
            do_check_eq(e.result,
                        Components.results.NS_ERROR_STORAGE_CONSTRAINT);
            restartManager();

            AddonManager.getAddonByID(installIDs[1], function(addon) {
              do_check_eq(initialGUID, addon.syncGUID);
              run_next_test();
            });
          }
        });
      }
    }
  };

  AddonManager.addInstallListener(listener);
  let getInstallCB = function(install) { install.install(); };

  for each (let name in installNames) {
    AddonManager.getInstallForFile(do_get_addon(name), getInstallCB);
  }
});

add_test(function test_fetch_by_guid_known_guid() {
  AddonManager.getAddonByID(addonId, function(addon) {
    do_check_neq(null, addon);
    do_check_neq(null, addon.syncGUID);

    let syncGUID = addon.syncGUID;

    XPIProvider.getAddonBySyncGUID(syncGUID, function(newAddon) {
      do_check_neq(null, newAddon);
      do_check_eq(syncGUID, newAddon.syncGUID);

      run_next_test();
    });
  });
});

add_test(function test_addon_manager_get_by_sync_guid() {
  AddonManager.getAddonByID(addonId, function(addon) {
    do_check_neq(null, addon.syncGUID);

    let syncGUID = addon.syncGUID;

    AddonManager.getAddonBySyncGUID(syncGUID, function(newAddon) {
      do_check_neq(null, newAddon);
      do_check_eq(addon.id, newAddon.id);
      do_check_eq(syncGUID, newAddon.syncGUID);

      AddonManager.getAddonBySyncGUID("DOES_NOT_EXIST", function(missing) {
        do_check_eq(undefined, missing);

        run_next_test();
      });
    });
  });
});

