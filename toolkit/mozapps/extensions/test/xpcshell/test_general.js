/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This just verifies that the EM can actually startup and shutdown a few times
// without any errors

// We have to look up how many add-ons are present since there will be plugins
// etc. detected
var gCount;

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  AddonManager.getAddonsByTypes(null, function(list) {
    gCount = list.length;

    executeSoon(run_test_1);
  });
}

function run_test_1() {
  restartManager();

  AddonManager.getAddonsByTypes(null, function(addons) {
    Assert.equal(gCount, addons.length);

    AddonManager.getAddonsWithOperationsByTypes(null, function(pendingAddons) {
      Assert.equal(0, pendingAddons.length);

      executeSoon(run_test_2);
    });
  });
}

function run_test_2() {
  shutdownManager();

  startupManager(false);

  AddonManager.getAddonsByTypes(null, function(addons) {
    Assert.equal(gCount, addons.length);

    executeSoon(run_test_3);
  });
}

function run_test_3() {
  restartManager();

  AddonManager.getAddonsByTypes(null, callback_soon(function(addons) {
    Assert.equal(gCount, addons.length);
    do_test_finished();
  }));
}
