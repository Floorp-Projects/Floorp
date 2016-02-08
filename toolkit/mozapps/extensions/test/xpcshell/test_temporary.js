/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

BootstrapMonitor.init();

// Install a temporary add-on with no existing add-on present.
// Restart and make sure it has gone away.
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

  yield AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));

  do_check_true(extInstallCalled);
  do_check_true(installingCalled);
  do_check_true(installedCalled);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = yield promiseAddonByID(ID);

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  yield promiseRestartManager();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  yield promiseRestartManager();
});

// Install a temporary add-on over the top of an existing add-on.
// Restart and make sure the existing add-on comes back.
add_task(function*() {
  yield promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = yield promiseAddonByID(ID);

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  // test that an unpacked add-on works too
  let tempdir = gTmpD.clone();

  writeInstallRDFToDir({
    id: ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (temporary)",
  }, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");

  yield AddonManager.installTemporaryAddon(unpacked_addon);

  BootstrapMonitor.checkAddonInstalled(ID, "2.0");
  BootstrapMonitor.checkAddonStarted(ID, "2.0");

  addon = yield promiseAddonByID(ID);

  // temporary add-on is installed and started
  do_check_neq(addon, null);
  do_check_eq(addon.version, "2.0");
  do_check_eq(addon.name, "Test Bootstrap 1 (temporary)");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  restartManager();

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  addon = yield promiseAddonByID(ID);

  // existing add-on is back
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  unpacked_addon.remove(true);
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  yield promiseRestartManager();
});

// Install a temporary add-on over the top of an existing add-on.
// Uninstall it and make sure the existing add-on comes back.
add_task(function*() {
  yield promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let tempdir = gTmpD.clone();
  writeInstallRDFToDir({
    id: ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (temporary)",
  }, tempdir);

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(ID);

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      do_check_eq(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  let installingCalled = false;
  let installedCalled = false;
  AddonManager.addAddonListener({
    onInstalling: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      if (!installingCalled)
        do_check_eq(aInstall.version, "2.0");
      installingCalled = true;
    },
    onInstalled: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      if (!installedCalled)
        do_check_eq(aInstall.version, "2.0");
      installedCalled = true;
    },
    onInstallStarted: (aInstall) => {
      do_throw("onInstallStarted called unexpectedly");
    }
  });

  yield AddonManager.installTemporaryAddon(unpacked_addon);

  do_check_true(extInstallCalled);
  do_check_true(installingCalled);
  do_check_true(installedCalled);

  let addon = yield promiseAddonByID(ID);

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  // temporary add-on is installed and started
  do_check_neq(addon, null);
  do_check_eq(addon.version, "2.0");
  do_check_eq(addon.name, "Test Bootstrap 1 (temporary)");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();

  addon = yield promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID);
  BootstrapMonitor.checkAddonStarted(ID);

  // existing add-on is back
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  unpacked_addon.remove(true);
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  yield promiseRestartManager();
});

// Install a temporary add-on over the top of an existing disabled add-on.
// After restart, the existing add-on should continue to be installed and disabled.
add_task(function*() {
  yield promiseInstallAllFiles([do_get_addon("test_bootstrap1_1")], true);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = yield promiseAddonByID(ID);

  addon.userDisabled = true;

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID);

  let tempdir = gTmpD.clone();
  writeInstallRDFToDir({
    id: ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (temporary)",
  }, tempdir, "bootstrap1@tests.mozilla.org", "bootstrap.js");

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(ID);
  do_get_file("data/test_temporary/bootstrap.js")
    .copyTo(unpacked_addon, "bootstrap.js");

  let extInstallCalled = false;
  AddonManager.addInstallListener({
    onExternalInstall: (aInstall) => {
      do_check_eq(aInstall.id, ID);
      do_check_eq(aInstall.version, "2.0");
      extInstallCalled = true;
    },
  });

  yield AddonManager.installTemporaryAddon(unpacked_addon);

  do_check_true(extInstallCalled);

  let tempAddon = yield promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "2.0");
  BootstrapMonitor.checkAddonStarted(ID);

  // temporary add-on is installed and started
  do_check_neq(tempAddon, null);
  do_check_eq(tempAddon.version, "2.0");
  do_check_eq(tempAddon.name, "Test Bootstrap 1 (temporary)");
  do_check_true(tempAddon.isCompatible);
  do_check_false(tempAddon.appDisabled);
  do_check_true(tempAddon.isActive);
  do_check_eq(tempAddon.type, "extension");
  do_check_eq(tempAddon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  tempAddon.uninstall();
  unpacked_addon.remove(true);

  addon.userDisabled = false;
  addon = yield promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID);

  // existing add-on is back
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  yield promiseRestartManager();
});

// Installing a temporary add-on over a non-restartless add-on should fail.
add_task(function*(){
  yield promiseInstallAllFiles([do_get_addon("test_install1")], true);

  let non_restartless_ID = "addon1@tests.mozilla.org";

  BootstrapMonitor.checkAddonNotInstalled(non_restartless_ID);
  BootstrapMonitor.checkAddonNotStarted(non_restartless_ID);

  restartManager();

  BootstrapMonitor.checkAddonNotInstalled(non_restartless_ID);
  BootstrapMonitor.checkAddonNotStarted(non_restartless_ID);

  let addon = yield promiseAddonByID(non_restartless_ID);

  // non-restartless add-on is installed and started
  do_check_neq(addon, null);
  do_check_eq(addon.id, non_restartless_ID);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let tempdir = gTmpD.clone();
  writeInstallRDFToDir({
    id: non_restartless_ID,
    version: "2.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test 1 (temporary)",
  }, tempdir);

  let unpacked_addon = tempdir.clone();
  unpacked_addon.append(non_restartless_ID);

  try {
    yield AddonManager.installTemporaryAddon(unpacked_addon);
    do_throw("Installing over a non-restartless add-on should return"
             + " a rejected promise");
  } catch (err) {
    do_check_eq(err.message,
        "Non-restartless add-on with ID addon1@tests.mozilla.org is"
        + " already installed");
  }

  unpacked_addon.remove(true);
  addon.uninstall();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);

  yield promiseRestartManager();
});

// Installing a temporary add-on when there is already a temporary
// add-on should fail.
add_task(function*() {
  yield AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));

  let addon = yield promiseAddonByID(ID);

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Test Bootstrap 1");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  try {
    yield AddonManager.installTemporaryAddon(do_get_addon("test_bootstrap1_1"));
    do_throw("Installing a temporary second temporary add-on should return"
             + " a rejected promise");
  } catch (err) {
    do_check_eq(err.message,
        "Add-on with ID bootstrap1@tests.mozilla.org is already temporarily"
        + " installed");
  }

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  yield promiseRestartManager();

  BootstrapMonitor.checkAddonNotInstalled(ID);
  BootstrapMonitor.checkAddonNotStarted(ID);
});
