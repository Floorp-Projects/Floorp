/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0.1",
      maxVersion: "0.2"
    }]
  }, profileDir);

  startupManager();

  AddonManager.strictCompatibility = false;

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
    Assert.notEqual(aAddon, null);
    aAddon.userDisabled = true;
    do_execute_soon(run_test_1);
  });
}

function run_test_1() {
  restartManager();
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
    Assert.notEqual(aAddon, null);
    Assert.ok(aAddon.userDisabled);
    Assert.ok(!aAddon.isActive);
    Assert.ok(!aAddon.appDisabled);

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onPropertyChanged", ["appDisabled"]]
      ]
    }, [], run_test_2);

    AddonManager.strictCompatibility = true;
  });
}

function run_test_2() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
    Assert.notEqual(aAddon, null);
    Assert.ok(aAddon.userDisabled);
    Assert.ok(!aAddon.isActive);
    Assert.ok(aAddon.appDisabled);

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onPropertyChanged", ["appDisabled"]]
      ]
    }, [], callback_soon(do_test_finished));

    AddonManager.strictCompatibility = false;
  });
}
