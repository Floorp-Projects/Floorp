/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

BootstrapMonitor.init();

// Partial list of bootstrap reasons from XPIProvider.jsm
const BOOTSTRAP_REASONS = {
  ADDON_INSTALL: 5,
  ADDON_UPGRADE: 7,
  ADDON_DOWNGRADE: 8,
};

// Install an unsigned add-on with no existing add-on present.
// Restart and make sure it is still around.
add_task(async function() {
  await promiseStartupManager();

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installingCalled = true;
    },
    onInstalled: (aInstall) => {
      Assert.equal(aInstall.id, ID);
      Assert.equal(aInstall.version, "1.0");
      installedCalled = true;
    },
    onInstallStarted: (aInstall) => {
      do_throw("onInstallStarted called unexpectedly");
    }
  });

  await AddonManager.installAddonFromSources(do_get_file("data/from_sources/"));

  Assert.ok(extInstallCalled);
  Assert.ok(installingCalled);
  Assert.ok(installedCalled);

  let install = BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_PRIVILEGED : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  await promiseRestartManager();

  install = BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  equal(install.reason, BOOTSTRAP_REASONS.ADDON_INSTALL);
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);

  await promiseRestartManager();
});

