/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";
const ID2 = "bootstrap2@tests.mozilla.org";

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

  yield promiseRestartManager();
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

  yield promiseRestartManager();
});

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

  // After install and restart we should block.
  let blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

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

  yield promiseRestartManager();

  // After disable and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);

  addon = yield promiseAddonByID(ID);
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_true(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  yield promiseRestartManager();

  // After re-enable and restart we should block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_true(addon.isActive);
  BootstrapMonitor.checkAddonStarted(ID);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();

  // After uninstall and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);

  restartManager();
});

add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);

  let install1 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  let install2 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap2_1"), resolve));
  yield promiseCompleteAllInstalls([install1, install2]);
  do_check_eq(install1.state, AddonManager.STATE_INSTALLED);
  do_check_eq(install2.state, AddonManager.STATE_INSTALLED);
  do_check_true(hasFlag(install1.addon.pendingOperations, AddonManager.PENDING_INSTALL));
  do_check_true(hasFlag(install2.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  let addon = yield promiseAddonByID(ID);
  let addon2 = yield promiseAddonByID(ID2);

  do_check_eq(addon, null);
  do_check_eq(addon2, null);

  yield promiseRestartManager();

  // After install and restart we should block.
  let blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  addon2 = yield promiseAddonByID(ID2);
  do_check_neq(addon2, null);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE));
  addon.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_false(hasFlag(addon.pendingOperations, AddonManager.PENDING_DISABLE));
  do_check_true(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE));

  yield promiseRestartManager();

  // After disable one addon and restart we should block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  addon2 = yield promiseAddonByID(ID2);

  do_check_false(hasFlag(addon2.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE));
  addon2.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID2);
  do_check_false(addon2.isActive);
  do_check_false(hasFlag(addon2.pendingOperations, AddonManager.PENDING_DISABLE));
  do_check_true(hasFlag(addon2.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE));

  yield promiseRestartManager();

  // After disable both addons and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);

  addon = yield promiseAddonByID(ID);
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_true(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  yield promiseRestartManager();

  // After re-enable one addon and restart we should block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_true(addon.isActive);
  BootstrapMonitor.checkAddonStarted(ID);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();

  // After uninstall the only enabled addon and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);

  addon2 = yield promiseAddonByID(ID2);
  addon2.uninstall();

  restartManager();
});

// Check that the rollout policy sets work as expected
add_task(function*(){
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);
  Services.prefs.setCharPref("extensions.e10s.rollout.policy", "xpcshell-test");

  // Both 'bootstrap1' and 'bootstrap2' addons are listed in the allowed policy
  // set, so they should install and start normally.
  yield check_normal();

  // Check that the two add-ons can be installed together correctly as
  // check_normal() only perform checks on bootstrap1.
  let install1 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  let install2 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap2_1"), resolve));
  yield promiseCompleteAllInstalls([install1, install2]);

  do_check_eq(install1.state, AddonManager.STATE_INSTALLED);
  do_check_eq(install2.state, AddonManager.STATE_INSTALLED);
  do_check_false(hasFlag(install1.addon.pendingOperations, AddonManager.PENDING_INSTALL));
  do_check_false(hasFlag(install2.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  let addon = yield promiseAddonByID(ID);
  let addon2 = yield promiseAddonByID(ID2);

  do_check_neq(addon, null);
  do_check_neq(addon2, null);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  BootstrapMonitor.checkAddonInstalled(ID2);
  BootstrapMonitor.checkAddonStarted(ID2);

  yield promiseRestartManager();

  // After install and restart e10s should not be blocked.
  let blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);

  // Check that adding bootstrap2 to the blocklist will trigger a disable of e10s.
  Services.prefs.setCharPref("extensions.e10s.rollout.blocklist", ID2);
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  yield promiseRestartManager();

  // Check that after restarting, e10s continues to be blocked.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  // Check that uninstalling bootstrap2 (which is in the blocklist) will
  // cause e10s to be re-enabled.
  addon2 = yield promiseAddonByID(ID2);
  do_check_false(hasFlag(addon2.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon2.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID2);
  BootstrapMonitor.checkAddonNotInstalled(ID2);

  yield promiseRestartManager();

  // After uninstall the blocklisted addon and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);


  // Let's perform similar checks again, now that bootstrap2 is in the blocklist.
  // The bootstrap1 add-on should install and start correctly, but bootstrap2 should not.
  addon = yield promiseAddonByID(ID);
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();

  install1 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  install2 = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap2_1"), resolve));
  yield promiseCompleteAllInstalls([install1, install2]);

  do_check_eq(install1.state, AddonManager.STATE_INSTALLED);
  do_check_eq(install2.state, AddonManager.STATE_INSTALLED);
  do_check_false(hasFlag(install1.addon.pendingOperations, AddonManager.PENDING_INSTALL));
  do_check_true(hasFlag(install2.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  addon = yield promiseAddonByID(ID);
  addon2 = yield promiseAddonByID(ID2);

  do_check_neq(addon, null);
  do_check_eq(addon2, null);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  BootstrapMonitor.checkAddonNotInstalled(ID2);
  BootstrapMonitor.checkAddonNotStarted(ID2);

  yield promiseRestartManager();

  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  // Clean-up
  addon = yield promiseAddonByID(ID);
  addon2 = yield promiseAddonByID(ID2);

  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  addon2.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID2);
  BootstrapMonitor.checkAddonNotInstalled(ID2);

  Services.prefs.clearUserPref("extensions.e10s.rollout.policy");
  Services.prefs.clearUserPref("extensions.e10s.rollout.blocklist");

  yield promiseRestartManager();
});

// The hotfix is unaffected
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);
  Services.prefs.setCharPref("extensions.hotfix.id", ID);
  Services.prefs.setBoolPref("extensions.hotfix.cert.checkAttributes", false);

  yield check_normal();
});
