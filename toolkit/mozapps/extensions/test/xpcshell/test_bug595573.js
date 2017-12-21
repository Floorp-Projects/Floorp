/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests if addons with UUID based ids install and stay installed

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  run_test_1();
}

function run_test_1() {
  installAllFiles([do_get_addon("test_bug595573")], async function() {
    await promiseRestartManager();

    AddonManager.getAddonByID("{2f69dacd-03df-4150-a9f1-e8a7b2748829}", function(a1) {
      Assert.notEqual(a1, null);
      Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

      executeSoon(run_test_2);
    });
  });
}

function run_test_2() {
  restartManager();

  AddonManager.getAddonByID("{2f69dacd-03df-4150-a9f1-e8a7b2748829}", function(a1) {
    Assert.notEqual(a1, null);
    Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

    executeSoon(do_test_finished);
  });
}
