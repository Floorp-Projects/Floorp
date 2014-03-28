/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

        Assert.ok(addon.isActive, "Add-on is active.");
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
