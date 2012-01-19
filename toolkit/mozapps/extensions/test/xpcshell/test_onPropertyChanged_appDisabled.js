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
    do_check_neq(aAddon, null);
    aAddon.userDisabled = true;
    restartManager();
    run_test_1();
  });
}

function run_test_1() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
    do_check_neq(aAddon, null);
    do_check_true(aAddon.userDisabled);
    do_check_false(aAddon.isActive);
    do_check_false(aAddon.appDisabled);

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
    do_check_neq(aAddon, null);
    do_check_true(aAddon.userDisabled);
    do_check_false(aAddon.isActive);
    do_check_true(aAddon.appDisabled);

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onPropertyChanged", ["appDisabled"]]
      ]
    }, [], do_test_finished);

    AddonManager.strictCompatibility = false;
  });
}
