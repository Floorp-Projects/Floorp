/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bug567184"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);

    prepare_test({
      "bug567184@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_1);
    install.install();
  });
}

function check_test_1() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("bug567184@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_true(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_false(b1.isActive);

      do_execute_soon(do_test_finished);
    });
  });
}
