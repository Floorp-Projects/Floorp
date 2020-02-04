/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies startup detection of added/removed/changed items and install
// location priorities

Services.prefs.setIntPref("extensions.autoDisableScopes", 0);

const ID1 = getID(1);
const ID2 = getID(2);
const ID3 = getID(3);
const ID4 = getID(4);

function createWebExtensionXPI(id, version) {
  return createTempWebExtensionFile({
    manifest: {
      version,
      applications: { gecko: { id } },
    },
  });
}

// Try to install all the items into the profile
add_task(async function test_scan_profile() {
  await promiseStartupManager();

  let ids = [];
  for (let n of [1, 2, 3]) {
    let id = getID(n);
    ids.push(id);
    await createWebExtension(id, initialVersion(n), profileDir);
  }

  await promiseRestartManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 3, "addons installed");

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

  await promiseShutdownManager();
});

// Test that modified items are detected and items in other install locations
// are ignored
add_task(async function test_modify() {
  await createWebExtension(ID1, "1.1", userDir);
  await createWebExtension(ID2, "2.1", profileDir);
  await createWebExtension(ID2, "2.2", globalDir);
  await createWebExtension(ID2, "2.3", userDir);

  await OS.File.remove(OS.Path.join(profileDir.path, `${ID3}.xpi`));

  await promiseStartupManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 2, "addons installed");

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

  await promiseShutdownManager();
});

// Check that removing items from the profile reveals their hidden versions.
add_task(async function test_reveal() {
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID2}.xpi`));

  // XPI with wrong name (basename doesn't match the id)
  let xpi = await createWebExtensionXPI(ID3, "3.0");
  xpi.copyTo(profileDir, `${ID4}.xpi`);

  await promiseStartupManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 2, "addons installed");

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [ID1, ID2]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_DISABLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_ENABLED, []);

  let [a1, a2, a3, a4] = await AddonManager.getAddonsByIDs([
    ID1,
    ID2,
    ID3,
    ID4,
  ]);

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

  let addon4Exists = await OS.File.exists(
    OS.Path.join(profileDir.path, `${ID4}.xpi`)
  );
  Assert.ok(!addon4Exists, "Misnamed xpi should be removed from profile");

  await promiseShutdownManager();
});

// Test that disabling an install location works
add_task(async function test_disable_location() {
  Services.prefs.setIntPref(
    "extensions.enabledScopes",
    AddonManager.SCOPE_SYSTEM
  );

  await promiseStartupManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "addons installed");

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

  await promiseShutdownManager();
});

// Switching disabled locations works
add_task(async function test_disable_location2() {
  Services.prefs.setIntPref(
    "extensions.enabledScopes",
    AddonManager.SCOPE_USER
  );

  await promiseStartupManager();

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

  await promiseShutdownManager();
});

// Resetting the pref makes everything visible again
add_task(async function test_enable_location() {
  Services.prefs.clearUserPref("extensions.enabledScopes");

  await promiseStartupManager();

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

  await promiseShutdownManager();
});

// Check that items in the profile hide the others again.
add_task(async function test_profile_hiding() {
  const VERSION1 = "1.2";
  await createWebExtension(ID1, VERSION1, profileDir);

  await OS.File.remove(OS.Path.join(userDir.path, `${ID2}.xpi`));

  await promiseStartupManager();

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

  await promiseShutdownManager();
});

// Disabling all locations still leaves the profile working
add_task(async function test_disable3() {
  Services.prefs.setIntPref("extensions.enabledScopes", 0);

  await promiseStartupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [
    "2@tests.mozilla.org",
  ]);
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

  await promiseShutdownManager();
});

// More hiding and revealing
add_task(async function test_reval() {
  Services.prefs.clearUserPref("extensions.enabledScopes");

  await OS.File.remove(OS.Path.join(userDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(globalDir.path, `${ID2}.xpi`));

  const VERSION2 = "2.4";
  await createWebExtension(ID2, VERSION2, profileDir);

  await promiseStartupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, [
    "2@tests.mozilla.org",
  ]);
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

  await promiseShutdownManager();
});

// Checks that a removal from one location and an addition in another location
// for the same item is handled
add_task(async function test_move() {
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`));
  const VERSION1 = "1.3";
  await createWebExtension(ID1, VERSION1, userDir);

  await promiseStartupManager();

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

  await promiseShutdownManager();
});

// This should remove any remaining items
add_task(async function test_remove() {
  await OS.File.remove(OS.Path.join(userDir.path, `${ID1}.xpi`));
  await OS.File.remove(OS.Path.join(profileDir.path, `${ID2}.xpi`));

  await promiseStartupManager();

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

  await promiseShutdownManager();
});

// Test that auto-disabling for specific scopes works
add_task(async function test_autoDisable() {
  Services.prefs.setIntPref(
    "extensions.autoDisableScopes",
    AddonManager.SCOPE_USER
  );

  async function writeAll() {
    return Promise.all([
      createWebExtension(ID1, "1.0", profileDir),
      createWebExtension(ID2, "2.0", userDir),
      createWebExtension(ID3, "3.0", globalDir),
    ]);
  }

  async function removeAll() {
    return Promise.all([
      OS.File.remove(OS.Path.join(profileDir.path, `${ID1}.xpi`)),
      OS.File.remove(OS.Path.join(userDir.path, `${ID2}.xpi`)),
      OS.File.remove(OS.Path.join(globalDir.path, `${ID3}.xpi`)),
    ]);
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

  Services.prefs.setIntPref(
    "extensions.autoDisableScopes",
    AddonManager.SCOPE_SYSTEM
  );

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

  Services.prefs.setIntPref(
    "extensions.autoDisableScopes",
    AddonManager.SCOPE_USER + AddonManager.SCOPE_SYSTEM
  );

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
