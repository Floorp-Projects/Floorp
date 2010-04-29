/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons can be uninstalled.

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Sets up the profile by installing a couple of add-ons. One is dependent on
// the other.
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  do_test_pending();
  startupManager(1);

  AddonManager.getAddon("addon1@tests.mozilla.org", function(olda1) {
    do_check_eq(olda1, null);

    var dest = profileDir.clone();
    dest.append("addon1@tests.mozilla.org");
    writeInstallRDFToDir(addon1, dest);

    restartManager(1);

    AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
      do_check_neq(a1, null);
      do_check_true(a1.isActive);
      do_check_false(a1.userDisabled);
      do_check_true(isExtensionInAddonsList(profileDir, a1.id));
      do_check_eq(a1.pendingOperations, 0);

      run_test_1();
    });
  });
}

function end_test() {
  do_test_finished();
}

// Uninstalling an add-on should work.
function run_test_1() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_eq(a1.pendingOperations, 0);
    a1.uninstall();
    do_check_true(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));

    ensure_test_completed();

    AddonManager.getAddonsWithPendingOperations(null, function(list) {
      do_check_eq(list.length, 1);
      do_check_eq(list[0].id, "addon1@tests.mozilla.org");

      check_test_1();
    });
  });
}

function check_test_1() {
  restartManager(0);

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_eq(a1, null);
    do_check_false(isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));

    var dest = profileDir.clone();
    dest.append("addon1@tests.mozilla.org");
    do_check_false(dest.exists());
    writeInstallRDFToDir(addon1, dest);
    restartManager(1);

    run_test_2();
  });
}

// Cancelling the uninstall should send onOperationCancelled
function run_test_2() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_true(a1.isActive);
    do_check_false(a1.userDisabled);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));
    do_check_eq(a1.pendingOperations, 0);
    a1.uninstall();
    do_check_true(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));

    ensure_test_completed();

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.cancelUninstall();
    do_check_eq(a1.pendingOperations, 0);

    ensure_test_completed();

    check_test_2();
  });
}

function check_test_2() {
  restartManager(0);

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_true(a1.isActive);
    do_check_false(a1.userDisabled);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    run_test_3();
  });
}

// Uninstalling an item pending disable should still require a restart
function run_test_3() {
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    prepare_test({
      "addon1@tests.mozilla.org": [
        "onDisabling"
      ]
    });
    a1.userDisabled = true;
    ensure_test_completed();

    do_check_true(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));
    do_check_true(a1.isActive);

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onUninstalling"
      ]
    });
    a1.uninstall();

    check_test_3();
  });
}

function check_test_3() {
  ensure_test_completed();

  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_true(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));
    do_check_true(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.cancelUninstall();
    ensure_test_completed();

    restartManager(0);
    run_test_4();
  });
}

// Test that uninstalling an inactive item should happen without a restart
function run_test_4() {
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_true(a1.userDisabled);
    do_check_false(isExtensionInAddonsList(profileDir, a1.id));

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });
    a1.uninstall();

    check_test_4();
  });
}

function check_test_4() {
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_eq(a1, null);

    end_test();
  });
}
