// Enable signature checks for these tests
gUseRealCertChecks = true;
// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const DATA = "data/signing_checks/";
const ADDONS = {
  bootstrap: {
    unsigned: "unsigned_bootstrap_2.xpi",
    badid: "signed_bootstrap_badid_2.xpi",
    signed: "signed_bootstrap_2.xpi",
    preliminary: "preliminary_bootstrap_2.xpi",
  },
  nonbootstrap: {
    unsigned: "unsigned_nonbootstrap_2.xpi",
    badid: "signed_nonbootstrap_badid_2.xpi",
    signed: "signed_nonbootstrap_2.xpi",
  }
};
const ID = "test@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Deletes a file from the test add-on in the profile
function breakAddon(file) {
  if (TEST_UNPACKED) {
    let f = file.clone();
    f.append("test.txt");
    f.remove(true);

    f = file.clone();
    f.append("install.rdf");
    f.lastModifiedTime = Date.now();
  } else {
    var zipW = Cc["@mozilla.org/zipwriter;1"].
               createInstance(Ci.nsIZipWriter);
    zipW.open(file, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
    zipW.removeEntry("test.txt", false);
    zipW.close();
  }
}

function resetPrefs() {
  Services.prefs.setIntPref("bootstraptest.active_version", -1);
  Services.prefs.setIntPref("bootstraptest.installed_version", -1);
  Services.prefs.setIntPref("bootstraptest.startup_reason", -1);
  Services.prefs.setIntPref("bootstraptest.shutdown_reason", -1);
  Services.prefs.setIntPref("bootstraptest.install_reason", -1);
  Services.prefs.setIntPref("bootstraptest.uninstall_reason", -1);
  Services.prefs.setIntPref("bootstraptest.startup_oldversion", -1);
  Services.prefs.setIntPref("bootstraptest.shutdown_newversion", -1);
  Services.prefs.setIntPref("bootstraptest.install_oldversion", -1);
  Services.prefs.setIntPref("bootstraptest.uninstall_newversion", -1);
}

function clearCache(file) {
  if (TEST_UNPACKED)
    return;

  Services.obs.notifyObservers(file, "flush-cache-entry");
}

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

  // Start and stop the manager to initialise everything in the profile before
  // actual testing
  await promiseStartupManager();
  await promiseShutdownManager();
  resetPrefs();
});

// Injecting into profile (bootstrap)
add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.unsigned), profileDir, ID);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);
  Assert.equal(getActiveVersion(), -1);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});

add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.signed), profileDir, ID);
  breakAddon(file);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.equal(getActiveVersion(), -1);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});

add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.badid), profileDir, ID);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.equal(getActiveVersion(), -1);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});

// Installs a signed add-on then modifies it in place breaking its signing
add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.signed), profileDir, ID);

  // Make it appear to come from the past so when we modify it later it is
  // detected during startup. Obviously malware can bypass this method of
  // detection but the periodic scan will catch that
  await promiseSetExtensionModifiedTime(file.path, Date.now() - 600000);

  await promiseStartupManager();
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);
  Assert.equal(getActiveVersion(), 2);

  await promiseShutdownManager();
  Assert.equal(getActiveVersion(), 0);

  clearCache(file);
  breakAddon(file);
  resetPrefs();

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.equal(getActiveVersion(), -1);

  let ids = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
  Assert.equal(ids.length, 1);
  Assert.equal(ids[0], ID);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});

// Injecting into profile (non-bootstrap)
add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.nonbootstrap.unsigned), profileDir, ID);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);
  Assert.ok(!isExtensionInAddonsList(profileDir, ID));

  addon.uninstall();
  await promiseRestartManager();
  await promiseShutdownManager();

  Assert.ok(!file.exists());
  clearCache(file);
});

add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.nonbootstrap.signed), profileDir, ID);
  breakAddon(file);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.ok(!isExtensionInAddonsList(profileDir, ID));

  addon.uninstall();
  await promiseRestartManager();
  await promiseShutdownManager();

  Assert.ok(!file.exists());
  clearCache(file);
});

add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.nonbootstrap.badid), profileDir, ID);

  await promiseStartupManager();

  // Currently we leave the sideloaded add-on there but just don't run it
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.ok(!isExtensionInAddonsList(profileDir, ID));

  addon.uninstall();
  await promiseRestartManager();
  await promiseShutdownManager();

  Assert.ok(!file.exists());
  clearCache(file);
});

// Installs a signed add-on then modifies it in place breaking its signing
add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.nonbootstrap.signed), profileDir, ID);

  // Make it appear to come from the past so when we modify it later it is
  // detected during startup. Obviously malware can bypass this method of
  // detection but the periodic scan will catch that
  await promiseSetExtensionModifiedTime(file.path, Date.now() - 60000);

  await promiseStartupManager();
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);
  Assert.ok(isExtensionInAddonsList(profileDir, ID));

  await promiseShutdownManager();

  clearCache(file);
  breakAddon(file);

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);
  Assert.ok(!isExtensionInAddonsList(profileDir, ID));

  let ids = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
  Assert.equal(ids.length, 1);
  Assert.equal(ids[0], ID);

  addon.uninstall();
  await promiseRestartManager();
  await promiseShutdownManager();

  Assert.ok(!file.exists());
  clearCache(file);
});

// Stage install then modify before startup (non-bootstrap)
add_task(async function() {
  await promiseStartupManager();
  await promiseInstallAllFiles([do_get_file(DATA + ADDONS.nonbootstrap.signed)]);
  await promiseShutdownManager();

  let staged = profileDir.clone();
  staged.append("staged");
  staged.append(do_get_expected_addon_name(ID));
  Assert.ok(staged.exists());

  breakAddon(staged);
  await promiseStartupManager();

  // Should have refused to install the broken staged version
  let addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  clearCache(staged);

  await promiseShutdownManager();
});

// Manufacture staged install (bootstrap)
add_task(async function() {
  let stage = profileDir.clone();
  stage.append("staged");

  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.signed), stage, ID);
  breakAddon(file);

  await promiseStartupManager();

  // Should have refused to install the broken staged version
  let addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);
  Assert.equal(getActiveVersion(), -1);

  Assert.ok(!file.exists());
  clearCache(file);

  await promiseShutdownManager();
  resetPrefs();
});

// Preliminarily-signed sideloaded add-ons should work
add_task(async function() {
  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.preliminary), profileDir, ID);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_PRELIMINARY);
  Assert.equal(getActiveVersion(), 2);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});

// Preliminarily-signed sideloaded add-ons should work via staged install
add_task(async function() {
  let stage = profileDir.clone();
  stage.append("staged");

  let file = await manuallyInstall(do_get_file(DATA + ADDONS.bootstrap.preliminary), stage, ID);

  await promiseStartupManager();

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_PRELIMINARY);
  Assert.equal(getActiveVersion(), 2);

  addon.uninstall();
  await promiseShutdownManager();
  resetPrefs();

  Assert.ok(!file.exists());
  clearCache(file);
});
