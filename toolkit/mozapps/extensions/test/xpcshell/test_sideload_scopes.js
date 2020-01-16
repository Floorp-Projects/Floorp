/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

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

// Just enable all scopes for this test.
Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_ALL);
// Setting this to all enables the same behavior as before disabling sideloading.
// We reset this later after doing some legacy sideloading.
Services.prefs.setIntPref("extensions.sideloadScopes", AddonManager.SCOPE_ALL);
// AddonTestUtils sets this to zero, we need the default value.
Services.prefs.clearUserPref("extensions.autoDisableScopes");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

function getID(n) {
  return `${n}@tests.mozilla.org`;
}
function initialVersion(n) {
  return `${n}.0`;
}

// Setup some common extension locations, at least one in each scope.

// SCOPE_SYSTEM
const globalDir = gProfD.clone();
globalDir.append("app-system-share");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);

// SCOPE_USER
const userDir = gProfD.clone();
userDir.append("app-system-user");
userDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userDir.parent);

// SCOPE_APPLICATION
const addonAppDir = gProfD.clone();
addonAppDir.append("app-global");
addonAppDir.append(gAppInfo.ID);
registerDirectory("XREAddonAppDir", addonAppDir.parent);

// SCOPE_PROFILE
const profileDir = gProfD.clone();
profileDir.append("extensions");

const scopeDirectories = Object.entries({
  global: globalDir,
  user: userDir,
  app: addonAppDir,
  profile: profileDir,
});

function check_startup_changes(aType, aIds) {
  var ids = aIds.slice(0);
  ids.sort();
  var changes = AddonManager.getStartupChanges(aType);
  changes = changes.filter(aEl => /@tests.mozilla.org$/.test(aEl));
  changes.sort();

  Assert.equal(JSON.stringify(ids), JSON.stringify(changes));
}

async function createWebExtension(id, version, dir) {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version,
      applications: { gecko: { id } },
    },
  });
  await AddonTestUtils.manuallyInstall(xpi, dir);
}

// IDs for scopes that should sideload when sideloading
// is not disabled.
let legacyIDs = [
  getID(`legacy-global`),
  getID(`legacy-user`),
  getID(`legacy-profile`),
];

add_task(async function test_sideloads_legacy() {
  let IDs = [];

  // Create a "legacy" addon for each scope.
  for (let [name, dir] of scopeDirectories) {
    let id = getID(`legacy-${name}`);
    IDs.push(id);
    await createWebExtension(id, initialVersion(name), dir);
  }

  await promiseStartupManager();

  // SCOPE_APPLICATION will never sideload, so we expect 3 addons.
  let sideloaded = await AddonManagerPrivate.getNewSideloads();
  Assert.equal(sideloaded.length, 3, "three sideloaded addon");
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
  // First, reset our scope pref to disable sideloading.
  Services.prefs.clearUserPref("extensions.sideloadScopes");

  // Create 4 new addons, only one of these, "profile" should
  // sideload.
  for (let [name, dir] of scopeDirectories) {
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

  for (let [name] of scopeDirectories) {
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
  Assert.equal(extensionAddons.length, 4);
  let IDs = extensionAddons.map(ext => ext.id);
  for (let id of [getID("profile"), ...legacyIDs]) {
    Assert.ok(IDs.includes(id), `${id} is installed`);
  }

  await promiseShutdownManager();
});

add_task(async function test_sideloads_changed() {
  // Upgrade the manifest version
  for (let [name, dir] of scopeDirectories) {
    let id = getID(name);
    await createWebExtension(id, `${name}.1`, dir);

    id = getID(`legacy-${name}`);
    await createWebExtension(id, `${name}.1`, dir);
  }

  await promiseStartupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, [
    getID("profile"),
    ...legacyIDs,
  ]);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  await promiseShutdownManager();
});

add_task(async function test_sideloads_removed() {
  // Side-delete all the extensions then test that we get the
  // startup changes for the legacy sideloads and the one "profile" addon.
  for (let [name, dir] of scopeDirectories) {
    let id = getID(name);
    await OS.File.remove(OS.Path.join(dir.path, `${id}.xpi`));

    id = getID(`legacy-${name}`);
    await OS.File.remove(OS.Path.join(dir.path, `${id}.xpi`));
  }

  await promiseStartupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_CHANGED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, [
    getID("profile"),
    ...legacyIDs,
  ]);

  await promiseShutdownManager();
});
