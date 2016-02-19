/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";

Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_SYSTEM);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

BootstrapMonitor.init();

const globalDir = gProfD.clone();
globalDir.append("extensions2");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);
const profileDir = gProfD.clone();
profileDir.append("extensions");

// By default disable add-ons from the system
Services.prefs.setIntPref("extensions.autoDisableScopes", AddonManager.SCOPE_SYSTEM);

// When new add-ons already exist in a system location when starting with a new
// profile they should be marked as already seen.
add_task(function*() {
  manuallyInstall(do_get_addon("test_bootstrap1_1"), globalDir, ID);

  startupManager();

  let addon = yield promiseAddonByID(ID);
  do_check_true(addon.foreignInstall);
  do_check_true(addon.seen);
  do_check_true(addon.userDisabled);
  do_check_false(addon.isActive);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  yield promiseShutdownManager();
});
