/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");

// restartManager() mucks with XPIProvider.jsm importing, so we hack around.
this.__defineGetter__("XPIProvider", function() {
  let scope = {};
  return Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", scope)
                   .XPIProvider;
});

const addonId = "addon1@tests.mozilla.org";

function run_test() {
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  run_next_test();
}

const UUID_PATTERN = /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i;

add_test(function test_getter_and_setter() {
  // Our test add-on requires a restart.
  let listener = {
    onInstallEnded: function onInstallEnded() {
     AddonManager.removeInstallListener(listener);
     // never restart directly inside an onInstallEnded handler!
     do_execute_soon(function getter_setter_install_ended() {
      restartManager();

      AddonManager.getAddonByID(addonId, function(addon) {

        Assert.notEqual(addon, null);
        Assert.notEqual(addon.syncGUID, null);
        Assert.ok(UUID_PATTERN.test(addon.syncGUID));

        let newGUID = "foo";

        addon.syncGUID = newGUID;
        Assert.equal(newGUID, addon.syncGUID);

        // Verify change made it to DB.
        AddonManager.getAddonByID(addonId, function(newAddon) {
          Assert.notEqual(newAddon, null);
          Assert.equal(newGUID, newAddon.syncGUID);
        });

        run_next_test();
      });
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
    Assert.equal(null, addon);
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
       do_execute_soon(function duplicate_syncguid_install_ended() {
        restartManager();

        AddonManager.getAddonsByIDs(installIDs, callback_soon(function(addons) {
          let initialGUID = addons[1].syncGUID;

          try {
            addons[1].syncGUID = addons[0].syncGUID;
            do_throw("Should not get here.");
          } catch (e) {
            Assert.ok(e.message.startsWith("Addon sync GUID conflict"));
            restartManager();

            AddonManager.getAddonByID(installIDs[1], function(addon) {
              Assert.equal(initialGUID, addon.syncGUID);
              run_next_test();
            });
          }
        }));
       });
      }
    }
  };

  AddonManager.addInstallListener(listener);
  let getInstallCB = function(install) { install.install(); };

  for (let name of installNames) {
    AddonManager.getInstallForFile(do_get_addon(name), getInstallCB);
  }
});

add_test(function test_fetch_by_guid_known_guid() {
  AddonManager.getAddonByID(addonId, function(addon) {
    Assert.notEqual(null, addon);
    Assert.notEqual(null, addon.syncGUID);

    let syncGUID = addon.syncGUID;

    XPIProvider.getAddonBySyncGUID(syncGUID, function(newAddon) {
      Assert.notEqual(null, newAddon);
      Assert.equal(syncGUID, newAddon.syncGUID);

      run_next_test();
    });
  });
});

add_test(function test_addon_manager_get_by_sync_guid() {
  AddonManager.getAddonByID(addonId, function(addon) {
    Assert.notEqual(null, addon.syncGUID);

    let syncGUID = addon.syncGUID;

    AddonManager.getAddonBySyncGUID(syncGUID, function(newAddon) {
      Assert.notEqual(null, newAddon);
      Assert.equal(addon.id, newAddon.id);
      Assert.equal(syncGUID, newAddon.syncGUID);

      AddonManager.getAddonBySyncGUID("DOES_NOT_EXIST", function(missing) {
        Assert.equal(undefined, missing);

        run_next_test();
      });
    });
  });
});
