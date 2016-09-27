/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

BootstrapMonitor.init();

// Partial list of bootstrap reasons from XPIProvider.jsm
const BOOTSTRAP_REASONS = {
  ADDON_INSTALL: 5,
  ADDON_UPGRADE: 7,
  ADDON_DOWNGRADE: 8,
};

// Install an unsigned add-on with no existing add-on present.
// Restart and make sure it is still around.
add_task(function*() {
  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      do_check_eq(aInstall.version, "1.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      do_check_eq(aInstall.version, "1.0");
      installingCalled = true;
    },
    onInstalled: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      do_check_eq(aInstall.version, "1.0");
      installedCalled = true;
    },
    onInstallStarted: (aInstall) => {
      do_throw("onInstallStarted called unexpectedly");
    }
  });

  yield AddonManager.installAddonFromSources(do_get_file("data/from_sources/"));

  do_check_true(extInstallCalled);
  do_check_true(installingCalled);
  do_check_true(installedCalled);

  let install = BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = yield promiseAddonByID(ID);

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  yield promiseRestartManager();

  install = BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  yield promiseRestartManager();
});

