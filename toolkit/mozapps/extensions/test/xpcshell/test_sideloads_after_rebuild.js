// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";

// IDs for scopes that should sideload when sideloading
// is not disabled.
let legacyIDs = [
  getID(`legacy-global`),
  getID(`legacy-user`),
  getID(`legacy-app`),
  getID(`legacy-profile`),
];

// This tests that, on a rebuild after addonStartup.json and extensions.json
// are lost, we only sideload from the profile.
add_task(async function test_sideloads_after_rebuild() {
  let IDs = [];

  // Create a sideloaded addon for each scope before the restriction is put
  // in place (by updating the sideloadScopes preference).
  for (let [name, dir] of Object.entries(scopeDirectories)) {
    let id = getID(`legacy-${name}`);
    IDs.push(id);
    await createWebExtension(id, initialVersion(name), dir);
  }

  await promiseStartupManager();

  // SCOPE_APPLICATION will never sideload, so we expect 3
  let sideloaded = await AddonManagerPrivate.getNewSideloads();
  Assert.equal(sideloaded.length, 4, "four sideloaded addon");
  let sideloadedIds = sideloaded.map(a => a.id);
  for (let id of legacyIDs) {
    Assert.ok(sideloadedIds.includes(id));
  }

  // After a restart that causes a database rebuild, we should have
  // the same addons available
  await promiseShutdownManager();
  // Reset our scope pref so the scope limitation works.
  Services.prefs.setIntPref(
    "extensions.sideloadScopes",
    AddonManager.SCOPE_PROFILE
  );

  // Try to sideload from a non-profile directory.
  await createWebExtension(
    getID(`sideload-global-1`),
    initialVersion("sideload-global"),
    globalDir
  );

  await promiseStartupManager("2");

  // We should still only have 4 addons.
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 4, "addons remain installed");

  await promiseShutdownManager();

  // Install a sideload that will not load because it is not in
  // appStartup.json and is not in a sideloadScope.
  await createWebExtension(
    getID(`sideload-global-2`),
    initialVersion("sideload-global"),
    globalDir
  );
  await createWebExtension(
    getID(`sideload-app-2`),
    initialVersion("sideload-global"),
    globalDir
  );
  // Install a sideload that will load.  We cannot currently prevent
  // this situation.
  await createWebExtension(
    getID(`sideload-profile`),
    initialVersion("sideload-profile"),
    profileDir
  );

  // Replace the extensions.json with something bogus so we lose our xpidatabase.
  // On AOM startup, addons are restored with help from XPIState.  Existing
  // sideloads should all remain.  One new sideloaded addon should be added from
  // the profile.
  await saveJSON({ not: "what we expect to find" }, gExtensionsJSON.path);
  info(`**** restart AOM and rebuild XPI database`);
  await promiseStartupManager();

  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 5, "addons installed");

  await promiseShutdownManager();

  // Install a sideload that will not load.
  await createWebExtension(
    getID(`sideload-global-3`),
    initialVersion("sideload-global"),
    globalDir
  );
  // Install a sideload that will load.  We cannot currently prevent
  // this situation.
  await createWebExtension(
    getID(`sideload-profile-2`),
    initialVersion("sideload-profile"),
    profileDir
  );

  // Replace the extensions.json with something bogus so we lose our xpidatabase.
  await saveJSON({ not: "what we expect to find" }, gExtensionsJSON.path);
  // Delete our appStartup/XPIState data.  Now we should only be able to
  // restore extensions in the profile.
  gAddonStartup.remove(true);
  info(`**** restart AOM and rebuild XPI database`);

  await promiseStartupManager();

  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 3, "addons installed");

  let [a1, a2, a3] = await promiseAddonsByIDs([
    getID(`legacy-profile`),
    getID(`sideload-profile`),
    getID(`sideload-profile-2`),
  ]);

  Assert.notEqual(a1, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a2.id));

  Assert.notEqual(a3, null);
  Assert.ok(isExtensionInBootstrappedList(profileDir, a3.id));

  await promiseShutdownManager();
});
