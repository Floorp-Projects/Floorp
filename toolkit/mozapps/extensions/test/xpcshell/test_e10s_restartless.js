/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";

BootstrapMonitor.init();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

startupManager();

function* check_normal() {
  let install = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  yield promiseCompleteAllInstalls([install]);
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_false(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, install.addon);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE));
  addon.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_false(hasFlag(addon.pendingOperations, AddonManager.PENDING_DISABLE));

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE));
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonStarted(ID);
  do_check_true(addon.isActive);
  do_check_false(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  restartManager();
}

// Installing the add-on normally doesn't require a restart
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = false;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", false);

  yield check_normal();
});

// Enabling the pref doesn't change anything
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = false;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);

  yield check_normal();
});

// Default e10s doesn't change anything
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", false);

  yield check_normal();
});

// Pref and e10s blocks install
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);

  let install = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  yield promiseCompleteAllInstalls([install]);
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_true(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  yield promiseRestartManager();

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE));
  addon.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_false(hasFlag(addon.pendingOperations, AddonManager.PENDING_DISABLE));

  do_check_true(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE));
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_true(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  yield promiseRestartManager();

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_true(addon.isActive);
  BootstrapMonitor.checkAddonStarted(ID);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  restartManager();
});

// The hotfix is unaffected
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);
  Services.prefs.setCharPref("extensions.hotfix.id", ID);

  yield check_normal();
});
