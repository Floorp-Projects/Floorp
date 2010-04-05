/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that plugins exist and can be enabled and disabled.
var gID = null;

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager(1);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  run_test_1();
}

// Tests that the test plugin exists
function run_test_1() {
  AddonManager.getAddonsByTypes("plugin", function(addons) {
    do_check_true(addons.length > 0);

    addons.forEach(function(p) {
      if (p.name == "Test Plug-in")
        gID = p.id;
    });

    do_check_neq(gID, null);

    AddonManager.getAddon(gID, function(p) {
      do_check_neq(p, null)
      do_check_false(p.userDisabled);
      do_check_false(p.appDisabled);
      do_check_true(p.isActive);
      do_check_eq(p.name, "Test Plug-in");

      run_test_2(p);
    });
  });
}

// Tests that disabling a plugin works
function run_test_2(p) {
  let test = {};
  test[gID] = [
    ["onDisabling", false],
    "onDisabled"
  ];
  prepare_test(test);

  p.userDisabled = true;

  ensure_test_completed();

  do_check_true(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_false(p.isActive);

  AddonManager.getAddon(gID, function(p) {
    do_check_neq(p, null)
    do_check_true(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_false(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    run_test_3(p);
  });
}

// Tests that enabling a plugin works
function run_test_3(p) {
  let test = {};
  test[gID] = [
    ["onEnabling", false],
    "onEnabled"
  ];
  prepare_test(test);

  p.userDisabled = false;

  ensure_test_completed();

  do_check_false(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_true(p.isActive);

  AddonManager.getAddon(gID, function(p) {
    do_check_neq(p, null)
    do_check_false(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_true(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    do_test_finished();
  });
}
