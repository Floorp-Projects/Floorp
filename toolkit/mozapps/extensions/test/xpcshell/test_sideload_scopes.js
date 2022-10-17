/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

// We refer to addons that were sideloaded prior to disabling sideloading as legacy.  We
// determine that they are legacy because they are in a SCOPE that is not included
// in AddonSettings.SCOPES_SIDELOAD.
//
// test_startup.js tests the legacy sideloading functionality still works as it should
// for ESR and some 3rd party distributions, which is to allow sideloading in all scopes.
//
// This file tests that locking down the sideload functionality works as we expect it to, which
// is to allow sideloading only in SCOPE_APPLICATION and SCOPE_PROFILE.  We also allow the legacy
// sideloaded addons to be updated or removed.
//
// We first change the sideload scope so we can sideload some addons into locations outside
// the profile (ie. we create some legacy sideloads).  We then reset it to test new sideloads.
//
// We expect new sideloads to only work in profile.
// We expect new sideloads to fail elsewhere.
// We expect to be able to change/uninstall legacy sideloads.

// IDs for scopes that should sideload when sideloading
// is not disabled.
let legacyIDs = [
  getID(`legacy-global`),
  getID(`legacy-user`),
  getID(`legacy-app`),
  getID(`legacy-profile`),
];

add_task(async function test_sideloads_legacy() {
  let IDs = [];

  // Create a "legacy" addon for each scope.
  for (let [name, dir] of Object.entries(scopeDirectories)) {
    let id = getID(`legacy-${name}`);
    IDs.push(id);
    await createWebExtension(id, initialVersion(name), dir);
  }

  await promiseStartupManager();

  // SCOPE_APPLICATION will never sideload, so we expect 3 addons.
  let sideloaded = await AddonManagerPrivate.getNewSideloads();
  Assert.equal(sideloaded.length, 4, "four sideloaded addons");
  let sideloadedIds = sideloaded.map(a => a.id);
  for (let id of legacyIDs) {
    Assert.ok(sideloadedIds.includes(id), `${id} is sideloaded`);
  }

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  await promiseShutdownManager();
});

// Test that a sideload install in SCOPE_PROFILE is allowed, all others are
// disallowed.
add_task(async function test_sideloads_disabled() {
  // First, reset our scope pref to disable sideloading.  head_sideload.js set this to ALL.
  Services.prefs.setIntPref(
    "extensions.sideloadScopes",
    AddonManager.SCOPE_PROFILE
  );

  // Create 4 new addons, only one of these, "profile" should
  // sideload.
  for (let [name, dir] of Object.entries(scopeDirectories)) {
    await createWebExtension(getID(name), initialVersion(name), dir);
  }

  await promiseStartupManager();

  // Test that the "profile" addon has been sideloaded.
  let sideloaded = await AddonManagerPrivate.getNewSideloads();
  Assert.equal(sideloaded.length, 1, "one sideloaded addon");

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, [
    getID("profile"),
  ]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  for (let [name] of Object.entries(scopeDirectories)) {
    let id = getID(name);
    let addon = await promiseAddonByID(id);
    if (name === "profile") {
      Assert.notEqual(addon, null);
      Assert.equal(addon.id, id);
      Assert.ok(addon.foreignInstall);
      Assert.equal(addon.scope, AddonManager.SCOPE_PROFILE);
      Assert.ok(addon.userDisabled);
      Assert.ok(!addon.seen);
    } else {
      Assert.equal(addon, null, `addon ${id} is not installed`);
    }
  }

  // Test that we still have the 3 legacy addons from the prior test, plus
  // the new "profile" addon from this test.
  let extensionAddons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(
    extensionAddons.length,
    5,
    "five addons expected to be installed"
  );
  let IDs = extensionAddons.map(ext => ext.id);
  for (let id of [getID("profile"), ...legacyIDs]) {
    Assert.ok(IDs.includes(id), `${id} is installed`);
  }

  await promiseShutdownManager();
});

add_task(async function test_sideloads_changed() {
  // Upgrade the manifest version
  for (let [name, dir] of Object.entries(scopeDirectories)) {
    let id = getID(name);
    await createWebExtension(id, `${name}.1`, dir);

    id = getID(`legacy-${name}`);
    await createWebExtension(id, `${name}.1`, dir);
  }

  await promiseStartupManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 5, "addons installed");

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [
    getID("profile"),
    ...legacyIDs,
  ]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  await promiseShutdownManager();
});

// Remove one just to test the startup changes
add_task(async function test_sideload_removal() {
  let id = getID(`legacy-profile`);
  let file = AddonTestUtils.getFileForAddon(profileDir, id);
  file.remove(false);
  Assert.ok(!file.exists());

  await promiseStartupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [id]);

  await promiseShutdownManager();
});

add_task(async function test_sideload_uninstall() {
  await promiseStartupManager();
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 4, "addons installed");
  for (let addon of addons) {
    let file = AddonTestUtils.getFileForAddon(
      scopeToDir.get(addon.scope),
      addon.id
    );
    await addon.uninstall();
    // Addon file should still exist in non-profile directories.
    Assert.equal(
      addon.scope !== AddonManager.SCOPE_PROFILE,
      file.exists(),
      `file remains after uninstall for non-profile sideloads, scope ${addon.scope}`
    );
  }
  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 0, "addons left");

  await promiseShutdownManager();
});
