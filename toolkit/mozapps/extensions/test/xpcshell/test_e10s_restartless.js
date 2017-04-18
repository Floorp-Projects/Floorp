/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";
const ID2 = "bootstrap2@tests.mozilla.org";
const ID3 = "bootstrap3@tests.mozilla.org";

const APP_STARTUP   = 1;
const ADDON_INSTALL = 5;

function getStartupReason(id) {
  let info = BootstrapMonitor.started.get(id);
  return info ? info.reason : undefined;
}

BootstrapMonitor.init();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

startupManager();

function check_multi_disabled() {
  return Services.prefs.getBoolPref("extensions.e10sMultiBlockedByAddons", false);
}

function* check_normal(checkMultiDisabled) {
  let install = yield promiseInstallFile(do_get_addon("test_bootstrap1_1"));
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_false(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, install.addon);

  if (checkMultiDisabled) {
    do_check_false(check_multi_disabled());
  }

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

  let install = yield promiseInstallFile(do_get_addon("test_bootstrap1_1"));
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_true(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));
  do_check_false(check_multi_disabled());

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  yield promiseRestartManager();

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);
  do_check_eq(getStartupReason(ID), ADDON_INSTALL);

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_true(check_multi_disabled());

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE));
  addon.userDisabled = true;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_false(hasFlag(addon.pendingOperations, AddonManager.PENDING_DISABLE));
  do_check_false(check_multi_disabled());

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
  do_check_true(check_multi_disabled());
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();
});

add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);

  let install = yield promiseInstallFile(do_get_addon("test_bootstrap1_1"));
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_true(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));
  do_check_false(check_multi_disabled());

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  yield promiseRestartManager();

  // After install and restart we should block.
  let blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);
  do_check_true(check_multi_disabled());

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);
  do_check_eq(getStartupReason(ID), ADDON_INSTALL);

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
  do_check_false(check_multi_disabled());

  addon = yield promiseAddonByID(ID);
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_true(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  yield promiseRestartManager();

  // After re-enable and restart we should block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);
  do_check_true(check_multi_disabled());

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_true(addon.isActive);
  BootstrapMonitor.checkAddonStarted(ID);
  // This should probably be ADDON_ENABLE, but its not easy to make
  // that happen.  See bug 1304392 for discussion.
  do_check_eq(getStartupReason(ID), APP_STARTUP);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();

  // After uninstall and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);
  do_check_false(check_multi_disabled());

  restartManager();
});

add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);

  let [install1, install2] = yield Promise.all([
    promiseInstallFile(do_get_addon("test_bootstrap1_1")),
    promiseInstallFile(do_get_addon("test_bootstrap2_1")),
  ]);

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
  do_check_eq(getStartupReason(ID), ADDON_INSTALL);

  BootstrapMonitor.checkAddonInstalled(ID2);
  BootstrapMonitor.checkAddonStarted(ID2);
  do_check_eq(getStartupReason(ID2), ADDON_INSTALL);

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
  do_check_true(check_multi_disabled());

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
  do_check_false(check_multi_disabled());

  addon = yield promiseAddonByID(ID);
  addon.userDisabled = false;
  BootstrapMonitor.checkAddonNotStarted(ID);
  do_check_false(addon.isActive);
  do_check_true(hasFlag(addon.pendingOperations, AddonManager.PENDING_ENABLE));

  yield promiseRestartManager();

  // After re-enable one addon and restart we should block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);
  do_check_true(check_multi_disabled());

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  do_check_true(addon.isActive);
  BootstrapMonitor.checkAddonStarted(ID);
  // Bug 1304392 again (see comment above)
  do_check_eq(getStartupReason(ID), APP_STARTUP);

  do_check_false(hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_UNINSTALL));
  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  yield promiseRestartManager();

  // After uninstall the only enabled addon and restart we should not block.
  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_false(blocked);
  do_check_false(check_multi_disabled());

  addon2 = yield promiseAddonByID(ID2);
  addon2.uninstall();

  restartManager();
});

// Check that the rollout policy sets work as expected
add_task(function*() {
  gAppInfo.browserTabsRemoteAutostart = true;
  Services.prefs.setBoolPref("extensions.e10sBlocksEnabling", true);
  Services.prefs.setCharPref("extensions.e10s.rollout.policy", "xpcshell-test");

  // Both 'bootstrap1' and 'bootstrap2' addons are part of the allowed policy
  // set, because they are marked as mpc=true.
  yield check_normal();

  // Check that the two add-ons can be installed together correctly as
  // check_normal() only perform checks on bootstrap1.
  let [install1, install2] = yield Promise.all([
    promiseInstallFile(do_get_addon("test_bootstrap1_1")),
    promiseInstallFile(do_get_addon("test_bootstrap2_1")),
  ]);

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

  [install1, install2] = yield Promise.all([
    promiseInstallFile(do_get_addon("test_bootstrap1_1")),
    promiseInstallFile(do_get_addon("test_bootstrap2_1")),
  ]);

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

  // Now let's test that an add-on that is not mpc=true doesn't get installed,
  // since the rollout policy is restricted to only mpc=true addons.
  let install3 = yield promiseInstallFile(do_get_addon("test_bootstrap3_1"));

  do_check_eq(install3.state, AddonManager.STATE_INSTALLED);
  do_check_true(hasFlag(install3.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  let addon3 = yield promiseAddonByID(ID3);

  do_check_eq(addon3, null);

  BootstrapMonitor.checkAddonNotInstalled(ID3);
  BootstrapMonitor.checkAddonNotStarted(ID3);

  yield promiseRestartManager();

  blocked = Services.prefs.getBoolPref("extensions.e10sBlockedByAddons");
  do_check_true(blocked);

  // Clean-up
  addon = yield promiseAddonByID(ID);
  addon2 = yield promiseAddonByID(ID2);
  addon3 = yield promiseAddonByID(ID3);

  addon.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID);
  BootstrapMonitor.checkAddonNotInstalled(ID);

  addon2.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID2);
  BootstrapMonitor.checkAddonNotInstalled(ID2);

  addon3.uninstall();
  BootstrapMonitor.checkAddonNotStarted(ID3);
  BootstrapMonitor.checkAddonNotInstalled(ID3);

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

  yield check_normal(true);
});

// Test non-restarless add-on's should not block multi
add_task(function*() {
  yield promiseInstallAllFiles([do_get_addon("test_install1")], true);

  let non_restartless_ID = "addon1@tests.mozilla.org";

  restartManager();

  let addon = yield promiseAddonByID(non_restartless_ID);

  // non-restartless add-on is installed and started
  do_check_neq(addon, null);

  do_check_false(check_multi_disabled());

  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(non_restartless_ID);
  BootstrapMonitor.checkAddonNotStarted(non_restartless_ID);

  yield promiseRestartManager();
});

// Test experiment add-on should not block multi
add_task(function*() {
  yield promiseInstallAllFiles([do_get_addon("test_experiment1")], true);

  let experiment_ID = "experiment1@tests.mozilla.org";

  BootstrapMonitor.checkAddonInstalled(experiment_ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(experiment_ID);

  let addon = yield promiseAddonByID(experiment_ID);

  // non-restartless add-on is installed and started
  do_check_neq(addon, null);

  do_check_false(check_multi_disabled());

  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(experiment_ID);
  BootstrapMonitor.checkAddonNotStarted(experiment_ID);

  yield promiseRestartManager();
});

const { GlobalManager } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

// Test web extension add-on's should not block multi
add_task(function*() {

  yield promiseInstallAllFiles([do_get_addon("webextension_1")], true),

  restartManager();

  yield promiseWebExtensionStartup();

  let we_ID = "webextension1@tests.mozilla.org";

  do_check_eq(GlobalManager.extensionMap.size, 1);

  let addon = yield promiseAddonByID(we_ID);

  do_check_neq(addon, null);

  do_check_false(check_multi_disabled());

  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(we_ID);
  BootstrapMonitor.checkAddonNotStarted(we_ID);

  yield promiseRestartManager();
});
