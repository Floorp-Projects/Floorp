/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "webextension1@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");
startupManager();

const { GlobalManager, Management } = Components.utils.import("resource://gre/modules/Extension.jsm", {});

function promiseAddonStartup() {
  return new Promise(resolve => {
    let listener = (extension) => {
      Management.off("startup", listener);
      resolve(extension);
    }

    Management.on("startup", listener);
  });
}

add_task(function*() {
  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  yield Promise.all([
    promiseInstallAllFiles([do_get_addon("webextension_1")], true),
    promiseAddonStartup()
  ]);

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  // Should persist through a restart
  yield promiseShutdownManager();

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  startupManager();
  yield promiseAddonStartup();

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  let file = getFileForAddon(profileDir, ID);
  do_check_true(file.exists());

  addon.userDisabled = true;

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  addon.userDisabled = false;
  yield promiseAddonStartup();

  do_check_eq(GlobalManager.count, 1);
  do_check_true(GlobalManager.extensionMap.has(ID));

  addon.uninstall();

  do_check_eq(GlobalManager.count, 0);
  do_check_false(GlobalManager.extensionMap.has(ID));

  yield promiseShutdownManager();
});

// Writing the manifest direct to the profile should work
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  startupManager();

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");
  do_check_eq(addon.name, "Web Extension Name");
  do_check_true(addon.isCompatible);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);
  do_check_eq(addon.type, "extension");
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  let file = getFileForAddon(profileDir, ID);
  do_check_true(file.exists());

  addon.uninstall();

  yield promiseRestartManager();
});

// Missing ID should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 2,
  }, profileDir, ID);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});

// Missing version should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    manifest_version: 2,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});

// Incorrect manifest version should cause a failure
add_task(function*() {
  writeWebManifestForExtension({
    name: "Web Extension Name",
    version: "1.0",
    manifest_version: 1,
    applications: {
      gecko: {
        id: ID
      }
    }
  }, profileDir);

  yield promiseRestartManager();

  let addon = yield promiseAddonByID(ID);
  do_check_eq(addon, null);

  let file = getFileForAddon(profileDir, ID);
  do_check_false(file.exists());

  yield promiseRestartManager();
});
