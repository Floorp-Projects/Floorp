/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

// Checks that permissions set in preferences are correctly imported but can
// be removed by the user.

const XPI_MIMETYPE = "application/x-xpinstall";

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  Services.prefs.setCharPref("xpinstall.whitelist.add", "test1.com,test2.com");
  Services.prefs.setCharPref("xpinstall.whitelist.add.36", "test3.com,www.test4.com");
  Services.prefs.setCharPref("xpinstall.whitelist.add.test5", "test5.com");

  Services.perms.add(NetUtil.newURI("https://www.test9.com"), "install",
                     AM_Ci.nsIPermissionManager.ALLOW_ACTION);

  startupManager();

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("http://test1.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://test1.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://www.test2.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test4.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("http://www.test5.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://www.test5.com")));

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("http://www.test6.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test6.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test7.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test8.com")));

  // This should remain unaffected
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("http://www.test9.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              NetUtil.newURI("https://www.test9.com")));

  Services.perms.removeAll();

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test1.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test2.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test5.com")));

  // Upgrade the application and verify that the permissions are still not there
  restartManager("2");

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test1.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test2.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               NetUtil.newURI("https://www.test5.com")));
}
