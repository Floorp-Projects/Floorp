/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons can be disabled and enabled.

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  iconURL: "chrome://foo/content/icon.png",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Sets up the profile by installing an add-on.
function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager(1);

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_eq(a1, null);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    var dest = profileDir.clone();
    dest.append("addon1@tests.mozilla.org");
    writeInstallRDFToDir(addon1, dest);

    restartManager(1);

    AddonManager.getAddon("addon1@tests.mozilla.org", function(newa1) {
      do_check_neq(newa1, null);
      do_check_true(newa1.isActive);
      do_check_false(newa1.userDisabled);
      do_check_eq(newa1.iconURL, "chrome://foo/content/icon.png");
      do_check_true(isExtensionInAddonsList(profileDir, newa1.id));
      do_check_true(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
      do_check_false(hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
      do_check_in_crash_annotation(addon1.id, addon1.version);

      run_test_1();
    });
  });
}

// Disabling an add-on should work
function run_test_1() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onDisabling"
    ]
  });

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    a1.userDisabled = true;
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
    do_check_in_crash_annotation(addon1.id, addon1.version);

    ensure_test_completed();

    AddonManager.getAddonsWithPendingOperations(null, function(list) {
      do_check_eq(list.length, 1);
      do_check_eq(list[0].id, "addon1@tests.mozilla.org");

      restartManager(0);

      AddonManager.getAddon("addon1@tests.mozilla.org", function(newa1) {
        do_check_neq(newa1, null);
        do_check_false(newa1.isActive);
        do_check_true(newa1.userDisabled);
        do_check_eq(newa1.iconURL, null);
        do_check_false(isExtensionInAddonsList(profileDir, newa1.id));
        do_check_false(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
        do_check_true(hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
        do_check_not_in_crash_annotation(addon1.id, addon1.version);

        run_test_2();
      });
    });
  });
}

// Enabling an add-on should work.
function run_test_2() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onEnabling"
    ]
  });

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    a1.userDisabled = false;
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));

    ensure_test_completed();

    AddonManager.getAddonsWithPendingOperations(null, function(list) {
      do_check_eq(list.length, 1);
      do_check_eq(list[0].id, "addon1@tests.mozilla.org");

      restartManager(0);

      AddonManager.getAddon("addon1@tests.mozilla.org", function(newa1) {
        do_check_neq(newa1, null);
        do_check_true(newa1.isActive);
        do_check_false(newa1.userDisabled);
        do_check_true(isExtensionInAddonsList(profileDir, newa1.id));
        do_check_true(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
        do_check_false(hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
        do_check_in_crash_annotation(addon1.id, addon1.version);

        run_test_3();
      });
    });
  });
}

// Disabling then enabling withut restart should fire onOperationCancelled.
function run_test_3() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onDisabling"
    ]
  });

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    a1.userDisabled = true;
    ensure_test_completed();
    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.userDisabled = false;
    do_check_true(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));

    ensure_test_completed();

    restartManager(0);

    AddonManager.getAddon("addon1@tests.mozilla.org", function(newa1) {
      do_check_neq(newa1, null);
      do_check_true(newa1.isActive);
      do_check_false(newa1.userDisabled);
      do_check_true(isExtensionInAddonsList(profileDir, newa1.id));
      do_check_true(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
      do_check_false(hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));

      do_test_finished();
    });
  });
}
