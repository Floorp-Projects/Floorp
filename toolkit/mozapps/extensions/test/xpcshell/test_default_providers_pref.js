/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the extensions.defaultProviders.enabled pref which turns
// off the default XPIProvider and LightweightThemeManager.

async function run_test() {
  Services.prefs.setBoolPref("extensions.defaultProviders.enabled", false);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
  do_check_false(AddonManager.isInstallEnabled("application/x-xpinstall"));
  Services.prefs.clearUserPref("extensions.defaultProviders.enabled");
}
