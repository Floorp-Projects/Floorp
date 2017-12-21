/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "bootstrap1@tests.mozilla.org";

let profileDir = gProfD.clone();
profileDir.append("extensions");

// By default disable add-ons from the profile and the system-wide scope
const SCOPES = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_SYSTEM;
Services.prefs.setIntPref("extensions.enabledScopes", SCOPES);
Services.prefs.setIntPref("extensions.autoDisableScopes", SCOPES);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Installing an add-on through the API should mark it as seen
add_task(async function() {
  await promiseStartupManager();

  let install = await promiseInstallFile(do_get_addon("test_bootstrap1_1"));
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.ok(!hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  let addon = install.addon;
  Assert.equal(addon.version, "1.0");
  Assert.ok(!addon.foreignInstall);
  Assert.ok(addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.foreignInstall);
  Assert.ok(addon.seen);

  // Installing an update should retain that
  install = await promiseInstallFile(do_get_addon("test_bootstrap1_2"));
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.ok(!hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  addon = install.addon;
  Assert.equal(addon.version, "2.0");
  Assert.ok(!addon.foreignInstall);
  Assert.ok(addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(!addon.foreignInstall);
  Assert.ok(addon.seen);

  addon.uninstall();

  await promiseShutdownManager();
});

// Sideloading an add-on in the systemwide location should mark it as unseen
add_task(async function() {
  let savedStartupScanScopes = Services.prefs.getIntPref("extensions.startupScanScopes");
  Services.prefs.setIntPref("extensions.startupScanScopes", 0);

  let systemParentDir = gTmpD.clone();
  systemParentDir.append("systemwide-extensions");
  registerDirectory("XRESysSExtPD", systemParentDir.clone());
  do_register_cleanup(() => {
    systemParentDir.remove(true);
  });

  let systemDir = systemParentDir.clone();
  systemDir.append(Services.appinfo.ID);

  let path = manuallyInstall(do_get_addon("test_bootstrap1_1"), systemDir, ID);
  // Make sure the startup code will detect sideloaded updates
  setExtensionModifiedTime(path, Date.now() - 10000);

  await promiseStartupManager();
  await AddonManagerPrivate.getNewSideloads();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "1.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseShutdownManager();
  Services.obs.notifyObservers(path, "flush-cache-entry");
  path.remove(true);

  Services.prefs.setIntPref("extensions.startupScanScopes", savedStartupScanScopes);
});

// Sideloading an add-on in the profile should mark it as unseen and it should
// remain unseen after an update is sideloaded.
add_task(async function() {
  let path = manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir, ID);
  // Make sure the startup code will detect sideloaded updates
  setExtensionModifiedTime(path, Date.now() - 10000);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "1.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseShutdownManager();

  // Sideloading an update shouldn't change the state
  manuallyUninstall(profileDir, ID);
  manuallyInstall(do_get_addon("test_bootstrap1_2"), profileDir, ID);
  setExtensionModifiedTime(path, Date.now());

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "2.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  addon.uninstall();
  await promiseShutdownManager();
});

// Sideloading an add-on in the profile should mark it as unseen and it should
// remain unseen after a regular update.
add_task(async function() {
  let path = manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir, ID);
  // Make sure the startup code will detect sideloaded updates
  setExtensionModifiedTime(path, Date.now() - 10000);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "1.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  // Updating through the API shouldn't change the state
  let install = await promiseInstallFile(do_get_addon("test_bootstrap1_2"));
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.ok(!hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  addon = install.addon;
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "2.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);

  addon.uninstall();
  await promiseShutdownManager();
});

// After a sideloaded addon has been seen, sideloading an update should
// not reset it to unseen.
add_task(async function() {
  let path = manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir, ID);
  // Make sure the startup code will detect sideloaded updates
  setExtensionModifiedTime(path, Date.now() - 10000);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "1.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);
  addon.markAsSeen();
  Assert.ok(addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.foreignInstall);
  Assert.ok(addon.seen);

  await promiseShutdownManager();

  // Sideloading an update shouldn't change the state
  manuallyUninstall(profileDir, ID);
  manuallyInstall(do_get_addon("test_bootstrap1_2"), profileDir, ID);
  setExtensionModifiedTime(path, Date.now());

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "2.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(addon.seen);

  addon.uninstall();
  await promiseShutdownManager();
});

// After a sideloaded addon has been seen, manually applying an update should
// not reset it to unseen.
add_task(async function() {
  let path = manuallyInstall(do_get_addon("test_bootstrap1_1"), profileDir, ID);
  // Make sure the startup code will detect sideloaded updates
  setExtensionModifiedTime(path, Date.now() - 10000);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "1.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(!addon.seen);
  addon.markAsSeen();
  Assert.ok(addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.ok(addon.foreignInstall);
  Assert.ok(addon.seen);

  // Updating through the API shouldn't change the state
  let install = await promiseInstallFile(do_get_addon("test_bootstrap1_2"));
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.ok(!hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

  addon = install.addon;
  Assert.ok(addon.foreignInstall);
  Assert.ok(addon.seen);

  await promiseRestartManager();

  addon = await promiseAddonByID(ID);
  Assert.equal(addon.version, "2.0");
  Assert.ok(addon.foreignInstall);
  Assert.ok(addon.seen);

  addon.uninstall();
  await promiseShutdownManager();
});
