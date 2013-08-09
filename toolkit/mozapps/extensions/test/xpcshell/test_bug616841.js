/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that string comparisons work correctly in callbacks

function test_string_compare() {
  do_check_true("C".localeCompare("D") < 0);
  do_check_true("D".localeCompare("C") > 0);
  do_check_true("\u010C".localeCompare("D") < 0);
  do_check_true("D".localeCompare("\u010C") > 0);
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  do_test_pending();

  test_string_compare();

  AddonManager.getAddonByID("foo", function(aAddon) {
    test_string_compare();
    do_test_finished();
  });
}
