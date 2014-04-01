/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let scope = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm");
const XPIProvider = scope.XPIProvider;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  run_next_test();
}

add_test(function test_experiment() {
  AddonManager.getInstallForFile(do_get_addon("test_experiment1"), (install) => {
    completeAllInstalls([install], () => {
      AddonManager.getAddonByID("experiment1@tests.mozilla.org", (addon) => {
        Assert.ok(addon, "Addon is found.");

        Assert.ok(addon.userDisabled, "Experiments are userDisabled by default.");
        Assert.equal(addon.isActive, false, "Add-on is not active.");
        Assert.equal(addon.updateURL, null, "No updateURL for experiments.");
        Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE,
                     "Background updates are disabled.");
        Assert.equal(addon.permissions, AddonManager.PERM_CAN_UNINSTALL,
                     "Permissions are minimal.");

        // Setting applyBackgroundUpdates should not work.
        addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
        Assert.equal(addon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE,
                     "Setting applyBackgroundUpdates shouldn't do anything.");

        let noCompatibleCalled = false;
        let noUpdateCalled = false;
        let finishedCalled = false;

        let listener = {
          onNoCompatibilityUpdateAvailable: () => { noCompatibleCalled = true; },
          onNoUpdateAvailable: () => { noUpdateCalled = true; },
          onUpdateFinished: () => { finishedCalled = true; },
        };

        addon.findUpdates(listener, "testing", null, null);
        Assert.ok(noCompatibleCalled, "Listener called.");
        Assert.ok(noUpdateCalled, "Listener called.");
        Assert.ok(finishedCalled, "Listener called.");

        run_next_test();
      });
    });
  });
});

// Changes to userDisabled should not be persisted to the database.
add_test(function test_userDisabledNotPersisted() {
  AddonManager.getAddonByID("experiment1@tests.mozilla.org", (addon) => {
    Assert.ok(addon, "Add-on is found.");
    Assert.ok(addon.userDisabled, "Add-on is user disabled.");

    let listener = {
      onEnabled: (addon2) => {
        AddonManager.removeAddonListener(listener);

        Assert.equal(addon2.id, addon.id, "Changed add-on matches expected.");
        Assert.equal(addon2.userDisabled, false, "Add-on is no longer user disabled.");
        Assert.ok(addon2.isActive, "Add-on is active.");

        Assert.ok("experiment1@tests.mozilla.org" in XPIProvider.bootstrappedAddons,
                  "Experiment add-on listed in XPIProvider bootstrapped list.");

        AddonManager.getAddonByID("experiment1@tests.mozilla.org", (addon) => {
          Assert.ok(addon, "Add-on retrieved.");
          Assert.equal(addon.userDisabled, false, "Add-on is still enabled after API retrieve.");
          Assert.ok(addon.isActive, "Add-on is still active.");

          // Now when we restart the manager the add-on should revert state.
          restartManager();
          let persisted = JSON.parse(Services.prefs.getCharPref("extensions.bootstrappedAddons"));
          Assert.ok(!("experiment1@tests.mozilla.org" in persisted),
                    "Experiment add-on not persisted to bootstrappedAddons.");

          AddonManager.getAddonByID("experiment1@tests.mozilla.org", (addon) => {
            Assert.ok(addon, "Add-on retrieved.");
            Assert.ok(addon.userDisabled, "Add-on is disabled after restart.");
            Assert.equal(addon.isActive, false, "Add-on is not active after restart.");

            run_next_test();
          });
        });
      },
    };

    AddonManager.addAddonListener(listener);
    addon.userDisabled = false;
  });
});
