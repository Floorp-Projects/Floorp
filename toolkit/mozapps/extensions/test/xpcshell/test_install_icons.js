/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var addon_url = "http://localhost:4444/test.xpi";
var icon32_url = "http://localhost:4444/icon.png";
var icon64_url = "http://localhost:4444/icon64.png";

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  test_1();
}

function test_1() {
  AddonManager.getInstallForURL(addon_url, function(aInstall) {
    do_check_eq(aInstall.iconURL, null);
    do_check_neq(aInstall.icons, null);
    do_check_eq(aInstall.icons[32], undefined);
    do_check_eq(aInstall.icons[64], undefined);
    test_2();
  }, "application/x-xpinstall", null, null, null, null, null);
}

function test_2() {
  AddonManager.getInstallForURL(addon_url, function(aInstall) {
    do_check_eq(aInstall.iconURL, icon32_url);
    do_check_neq(aInstall.icons, null);
    do_check_eq(aInstall.icons[32], icon32_url);
    do_check_eq(aInstall.icons[64], undefined);
    test_3();
  }, "application/x-xpinstall", null, null, icon32_url, null, null);
}

function test_3() {
  AddonManager.getInstallForURL(addon_url, function(aInstall) {
    do_check_eq(aInstall.iconURL, icon32_url);
    do_check_neq(aInstall.icons, null);
    do_check_eq(aInstall.icons[32], icon32_url);
    do_check_eq(aInstall.icons[64], undefined);
    test_4();
  }, "application/x-xpinstall", null, null, { "32": icon32_url }, null, null);
}

function test_4() {
  AddonManager.getInstallForURL(addon_url, function(aInstall) {
    do_check_eq(aInstall.iconURL, icon32_url);
    do_check_neq(aInstall.icons, null);
    do_check_eq(aInstall.icons[32], icon32_url);
    do_check_eq(aInstall.icons[64], icon64_url);
    do_test_finished();
  }, "application/x-xpinstall", null, null, { "32": icon32_url, "64": icon64_url }, null, null);
}
