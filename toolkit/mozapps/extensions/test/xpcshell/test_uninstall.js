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

// Sets up the profile by installing an add-on.
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  do_test_pending();
  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(olda1) {
    Assert.equal(olda1, null);

    writeInstallRDFForExtension(addon1, profileDir);

    await promiseRestartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      Assert.notEqual(a1, null);
      Assert.ok(a1.isActive);
      Assert.ok(!a1.userDisabled);
      Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
      Assert.equal(a1.pendingOperations, 0);
      do_check_in_crash_annotation(addon1.id, addon1.version);

      do_execute_soon(run_test_1);
    });
  }));
}

function end_test() {
  do_execute_soon(do_test_finished);
}

// Uninstalling an add-on should work.
function run_test_1() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.equal(a1.pendingOperations, 0);
    Assert.notEqual(a1.operationsRequiringRestart &
                    AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
    a1.uninstall();
    Assert.ok(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));
    do_check_in_crash_annotation(addon1.id, addon1.version);

    ensure_test_completed();

    AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
      Assert.equal(list.length, 1);
      Assert.equal(list[0].id, "addon1@tests.mozilla.org");

      do_execute_soon(check_test_1);
    });
  });
}

function check_test_1() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.equal(a1, null);
    Assert.ok(!isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    var dest = profileDir.clone();
    dest.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
    Assert.ok(!dest.exists());
    writeInstallRDFForExtension(addon1, profileDir);
    do_execute_soon(run_test_2);
  });
}

// Cancelling the uninstall should send onOperationCancelled
async function run_test_2() {
  await promiseRestartManager();

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.ok(a1.isActive);
    Assert.ok(!a1.userDisabled);
    Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
    Assert.equal(a1.pendingOperations, 0);
    a1.uninstall();
    Assert.ok(hasFlag(a1.pendingOperations, AddonManager.PENDING_UNINSTALL));

    ensure_test_completed();

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.cancelUninstall();
    Assert.equal(a1.pendingOperations, 0);

    ensure_test_completed();

    do_execute_soon(check_test_2);
  });
}

async function check_test_2() {
  await promiseRestartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.ok(a1.isActive);
    Assert.ok(!a1.userDisabled);
    Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

    run_test_3();
  });
}

// Uninstalling an item pending disable should still require a restart
function run_test_3() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    prepare_test({
      "addon1@tests.mozilla.org": [
        "onDisabling"
      ]
    });
    a1.userDisabled = true;
    ensure_test_completed();

    Assert.ok(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));
    Assert.ok(a1.isActive);

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

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.cancelUninstall();
    ensure_test_completed();
    Assert.ok(hasFlag(AddonManager.PENDING_DISABLE, a1.pendingOperations));

    do_execute_soon(run_test_4);
  });
}

// Test that uninstalling an inactive item should happen without a restart
function run_test_4() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.ok(!a1.isActive);
    Assert.ok(a1.userDisabled);
    Assert.ok(!isExtensionInAddonsList(profileDir, a1.id));

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });
    a1.uninstall();
    ensure_test_completed();

    check_test_4();
  });
}

function check_test_4() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.equal(a1, null);

    end_test();
  });
}
