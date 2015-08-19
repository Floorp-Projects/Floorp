/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

// Checks that permissions set in preferences are correctly imported but can
// be removed by the user.

const XPI_MIMETYPE = "application/x-xpinstall";

function newPrincipal(uri) {
  return Services.scriptSecurityManager.createCodebasePrincipal(NetUtil.newURI(uri), {});
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  Services.prefs.setCharPref("xpinstall.whitelist.add", "https://test1.com,https://test2.com");
  Services.prefs.setCharPref("xpinstall.whitelist.add.36", "https://test3.com,https://www.test4.com");
  Services.prefs.setCharPref("xpinstall.whitelist.add.test5", "https://test5.com");

  Services.perms.add(NetUtil.newURI("https://www.test9.com"), "install",
                     AM_Ci.nsIPermissionManager.ALLOW_ACTION);

  startupManager();

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("http://test1.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://test1.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://www.test2.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test4.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("http://www.test5.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://www.test5.com")));

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("http://www.test6.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test6.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test7.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test8.com")));

  // This should remain unaffected
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("http://www.test9.com")));
  do_check_true(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                              newPrincipal("https://www.test9.com")));

  Services.perms.removeAll();

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test1.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test2.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test5.com")));

  // Upgrade the application and verify that the permissions are still not there
  restartManager("2");

  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test1.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test2.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://test3.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test4.com")));
  do_check_false(AddonManager.isInstallAllowed(XPI_MIMETYPE,
                                               newPrincipal("https://www.test5.com")));
}
