/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies startup detection of added/removed/changed items and install
// location priorities

// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_USER);

function getID(n) { return `addon${n}@tests.mozilla.org`; }
function initialVersion(n) { return `${n}.0`; }

const ID1 = getID(1);
const ID2 = getID(2);
const ID3 = getID(3);
const ID4 = getID(4);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const globalDir = gProfD.clone();
globalDir.append("extensions2");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);

const userDir = gProfD.clone();
userDir.append("extensions3");
userDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userDir.parent);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function check_startup_changes(aType, aIds) {
  var ids = aIds.slice(0);
  ids.sort();
  var changes = AddonManager.getStartupChanges(aType);
  changes = changes.filter(aEl => /@tests.mozilla.org$/.test(aEl));
  changes.sort();

  Assert.equal(JSON.stringify(ids), JSON.stringify(changes));
}

function createWebExtension(id, version) {
  return createTempWebExtensionFile({
    manifest: {
      version,
      applications: {gecko: {id}},
    },
  });
}

// Set up the profile
add_task(async function setup() {
  await promiseStartupManager();
  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  Assert.ok(!gExtensionsJSON.exists());

  for (let n of [1, 2, 3]) {
    let addon = await promiseAddonByID(getID(n));
    Assert.equal(addon, null);
    do_check_not_in_crash_annotation(getID(n), initialVersion(n));
  }
});

// Try to install all the items into the profile
add_task(async function test_scan_profile() {
  let ids = [];
  for (let n of [1, 2, 3]) {
    let id = getID(n);
    ids.push(id);
    let xpi = await createWebExtension(id, initialVersion(n));
    xpi.copyTo(profileDir, `${id}.xpi`);
  }

  await Promise.all([
    promiseRestartManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, ids);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  info("Checking for " + gAddonStartup.path);
  Assert.ok(gAddonStartup.exists());

  for (let n of [1, 2, 3]) {
    let id = getID(n);
    let addon = await promiseAddonByID(id);
    Assert.notEqual(addon, null);
    Assert.equal(addon.id, id);
    Assert.notEqual(addon.syncGUID, null);
    Assert.ok(addon.syncGUID.length >= 9);
    Assert.equal(addon.version, initialVersion(n));
    Assert.ok(isExtensionInBootstrappedList(profileDir, id));
    Assert.ok(hasFlag(addon.permissions, AddonManager.PERM_CAN_UNINSTALL));
    Assert.ok(hasFlag(addon.permissions, AddonManager.PERM_CAN_UPGRADE));
    do_check_in_crash_annotation(id, initialVersion(n));
    Assert.equal(addon.scope, AddonManager.SCOPE_PROFILE);
    Assert.equal(addon.sourceURI, null);
    Assert.ok(addon.foreignInstall);
    Assert.ok(!addon.userDisabled);
    Assert.ok(addon.seen);
  }

  let extensionAddons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(extensionAddons.length, 3);
});

// Test that modified items are detected and items in other install locations
// are ignored
add_task(async function test_modify() {
  await promiseShutdownManager();

  let xpi = await createWebExtension(ID1, "1.1");
  xpi.copyTo(userDir, `${ID1}.xpi`);

  xpi = await createWebExtension(ID2, "2.1");
  xpi.copyTo(profileDir, `${ID2}.xpi`);

  xpi = await createWebExtension(ID2, "2.2");
  xpi.copyTo(globalDir, `${ID2}.xpi`);

  xpi = await createWebExtension(ID2, "2.3");
  xpi.copyTo(userDir, `${ID2}.xpi`);

  await OS.File.remove(OS.Path.join(profileDir.path, `${ID3}.xpi`));

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [ID3]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  Assert.ok(gAddonStartup.exists());

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);

  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, "1.0");
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, "1.0");
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(a1.foreignInstall);

  // The version in the profile should take precedence.
  const VERSION2 = "2.1";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(a2.foreignInstall);

  Assert.equal(a3, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID3));
  do_check_not_in_crash_annotation(ID3, "3.0");
});

// Check that removing items from the profile reveals their hidden versions.
add_task(async function test_reveal() {
  await promiseShutdownManager();

  await OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID2}.xpi`));

  // XPI with wrong name (basename doesn't match the id)
  let xpi = await createWebExtension(ID3, "3.0");
  xpi.copyTo(profileDir, `${ID4}.xpi`);

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID1, ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2, a3, a4] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3, ID4]);

  // Copy of addon1 in the per-user directory is now revealed.
  const VERSION1 = "1.1";
  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, VERSION1);
  Assert.equal(a1.scope, AddonManager.SCOPE_USER);

  // Likewise with addon2
  const VERSION2 = "2.3";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_USER);

  Assert.equal(a3, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID3));

  Assert.equal(a4, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID4));

  let addon4Exists = await OS.File.exists(OS.Path.join(profileDir.path, `${ID4}.xpi`));
  Assert.ok(!addon4Exists, "Misnamed xpi should be removed from profile");
});

// Test that disabling an install location works
add_task(async function test_disable_location() {
  Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_SYSTEM);

  await Promise.all([
    promiseRestartManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [ID1]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  Assert.equal(a1, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));

  // System-wide copy of addon2 is now revealed
  const VERSION2 = "2.2";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_SYSTEM);
});

// Switching disabled locations works
add_task(async function test_disable_location2() {
  Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_USER);

  await Promise.all([
    promiseRestartManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, [ID1]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);

  const VERSION1 = "1.1";
  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, VERSION1);
  Assert.equal(a1.scope, AddonManager.SCOPE_USER);

  const VERSION2 = "2.3";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_USER);
});

// Resetting the pref makes everything visible again
add_task(async function test_enable_location() {
  Services.prefs.clearUserPref("extensions.enabledScopes");

  await Promise.all([
    promiseRestartManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);

  const VERSION1 = "1.1";
  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, VERSION1);
  Assert.equal(a1.scope, AddonManager.SCOPE_USER);

  const VERSION2 = "2.3";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_USER);
});

// Check that items in the profile hide the others again.
add_task(async function test_profile_hiding() {
  await promiseShutdownManager();

  const VERSION1 = "1.2";
  let xpi = await createWebExtension(ID1, VERSION1);
  xpi.copyTo(profileDir, `${ID1}.xpi`);

  await OS.File.remove(OS.Path.join(userDir.path, `${ID2}.xpi`));

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID1, ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);

  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, VERSION1);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  const VERSION2 = "2.2";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID2, VERSION2);
  Assert.equal(a2.scope, AddonManager.SCOPE_SYSTEM);

  Assert.equal(a3, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID3));
});

// Disabling all locations still leaves the profile working
add_task(async function test_disable3() {
  Services.prefs.setIntPref("extensions.enabledScopes", 0);

  await Promise.all([
    await promiseRestartManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, ["addon2@tests.mozilla.org"]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);

  const VERSION1 = "1.2";
  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  do_check_in_crash_annotation(ID1, VERSION1);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  Assert.equal(a2, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
});

// More hiding and revealing
add_task(async function test_reval() {
  Services.prefs.clearUserPref("extensions.enabledScopes");

  await promiseShutdownManager();

  await OS.File.remove(OS.Path.join(userDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(globalDir.path, `${ID2}.xpi`));

  const VERSION2 = "2.4";
  let xpi = createWebExtension(ID2, VERSION2);
  xpi.copyTo(profileDir, `${ID2}.xpi`);

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, ["addon2@tests.mozilla.org"]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);

  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, "1.2");
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  Assert.equal(a2.scope, AddonManager.SCOPE_PROFILE);

  Assert.equal(a3, null);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID3));
});

// Checks that a removal from one location and an addition in another location
// for the same item is handled
add_task(async function test_move() {
  await promiseShutdownManager();

  await OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`));
  const VERSION1 = "1.3";
  let xpi = createWebExtension(ID1, VERSION1);
  xpi.copyTo(userDir, `${ID1}.xpi`);

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID1]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);

  Assert.notEqual(a1, null);
  Assert.equal(a1.id, ID1);
  Assert.equal(a1.version, VERSION1);
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UPGRADE));
  Assert.equal(a1.scope, AddonManager.SCOPE_USER);

  const VERSION2 = "2.4";
  Assert.notEqual(a2, null);
  Assert.equal(a2.id, ID2);
  Assert.equal(a2.version, VERSION2);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  Assert.ok(hasFlag(a2.permissions, AddonManager.PERM_CAN_UPGRADE));
  Assert.equal(a2.scope, AddonManager.SCOPE_PROFILE);
});

// This should remove any remaining items
add_task(async function test_remove() {
  await promiseShutdownManager();

  await OS.File.remove(OS.Path.join(userDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID2}.xpi`));

  await Promise.all([
    promiseStartupManager(),
  ]);

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [ID1, ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);
  Assert.equal(a1, null);
  Assert.equal(a2, null);
  Assert.equal(a3, null);

  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID3));
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID4));
  Assert.ok(!isExtensionInBootstrappedList(profileDir, ID4));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID3));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID4));
  Assert.ok(!isExtensionInBootstrappedList(userDir, ID4));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID1));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID2));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID3));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID4));
  Assert.ok(!isExtensionInBootstrappedList(globalDir, ID4));
});

// Test that auto-disabling for specific scopes works
add_task(async function test_autoDisable() {
  Services.prefs.setIntPref("extensions.autoDisableScopes", AddonManager.SCOPE_USER);

  await promiseShutdownManager();

  async function writeAll() {
    let xpi = await createWebExtension(ID1, "1.0");
    xpi.copyTo(profileDir, `${ID1}.xpi`);

    xpi = await createWebExtension(ID2, "2.0");
    xpi.copyTo(userDir, `${ID2}.xpi`);

    xpi = await createWebExtension(ID3, "3.0");
    xpi.copyTo(globalDir, `${ID3}.xpi`);
  }

  async function removeAll() {
    await OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`));
    await OS.File.remove(OS.Path.join(userDir.path, `${ID2}.xpi`));
    await OS.File.remove(OS.Path.join(globalDir.path, `${ID3}.xpi`));
  }

  await writeAll();

  await promiseStartupManager();

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);
  Assert.notEqual(a1, null);
  Assert.ok(!a1.userDisabled);
  Assert.ok(a1.seen);
  Assert.ok(a1.isActive);

  Assert.notEqual(a2, null);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.seen);
  Assert.ok(!a2.isActive);

  Assert.notEqual(a3, null);
  Assert.ok(!a3.userDisabled);
  Assert.ok(a3.seen);
  Assert.ok(a3.isActive);

  await promiseShutdownManager();

  await removeAll();

  await promiseStartupManager();
  await promiseShutdownManager();

  Services.prefs.setIntPref("extensions.autoDisableScopes", AddonManager.SCOPE_SYSTEM);

  await writeAll();

  await promiseStartupManager();

  [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);
  Assert.notEqual(a1, null);
  Assert.ok(!a1.userDisabled);
  Assert.ok(a1.seen);
  Assert.ok(a1.isActive);

  Assert.notEqual(a2, null);
  Assert.ok(!a2.userDisabled);
  Assert.ok(a2.seen);
  Assert.ok(a2.isActive);

  Assert.notEqual(a3, null);
  Assert.ok(a3.userDisabled);
  Assert.ok(!a3.seen);
  Assert.ok(!a3.isActive);

  await promiseShutdownManager();

  await removeAll();

  await promiseStartupManager();
  await promiseShutdownManager();

  Services.prefs.setIntPref("extensions.autoDisableScopes", AddonManager.SCOPE_USER + AddonManager.SCOPE_SYSTEM);

  await writeAll();

  await promiseStartupManager();

  [a1, a2, a3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);
  Assert.notEqual(a1, null);
  Assert.ok(!a1.userDisabled);
  Assert.ok(a1.seen);
  Assert.ok(a1.isActive);

  Assert.notEqual(a2, null);
  Assert.ok(a2.userDisabled);
  Assert.ok(!a2.seen);
  Assert.ok(!a2.isActive);

  Assert.notEqual(a3, null);
  Assert.ok(a3.userDisabled);
  Assert.ok(!a3.seen);
  Assert.ok(!a3.isActive);

  await promiseShutdownManager();
});
