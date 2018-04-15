/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This just verifies that the EM can actually startup and shutdown a few times
// without any errors

// We have to look up how many add-ons are present since there will be plugins
// etc. detected
var gCount;

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  let list = await AddonManager.getAddonsByTypes(null);
  gCount = list.length;

  executeSoon(run_test_1);
}

async function run_test_1() {
  restartManager();

  let addons = await AddonManager.getAddonsByTypes(null);
  Assert.equal(gCount, addons.length);

  let pendingAddons = await AddonManager.getAddonsWithOperationsByTypes(null);
  Assert.equal(0, pendingAddons.length);

  executeSoon(run_test_2);
}

async function run_test_2() {
  shutdownManager();

  startupManager(false);

  let addons = await AddonManager.getAddonsByTypes(null);
  Assert.equal(gCount, addons.length);

  executeSoon(run_test_3);
}

async function run_test_3() {
  restartManager();

  let addons = await AddonManager.getAddonsByTypes(null);
  Assert.equal(gCount, addons.length);
  do_test_finished();
}
