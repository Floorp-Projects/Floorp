/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that updating an add-on to a new ID works

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function promiseInstallUpdate(install) {
  return new Promise((resolve, reject) => {
    install.addListener({
      onDownloadFailed: () => {
        let err = new Error("download error");
        err.code = install.error;
        reject(err);
      },
      onInstallFailed: () => {
        let err = new Error("install error");
        err.code = install.error;
        reject(err);
      },
      onInstallEnded: resolve,
    });

    install.install();
  });
}

// Create and configure the HTTP server.
let testserver = createHttpServer(4444);
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
  run_next_test();
}

// Verify that an update to an add-on with a new ID fails
add_task(function* test_update_new_id() {
  yield promiseInstallFile(do_get_addon("test_updateid1"));

  let addon = yield promiseAddonByID("addon1@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");

  let update = yield promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  let install = update.updateAvailable;
  do_check_eq(install.name, addon.name);
  do_check_eq(install.version, "2.0");
  do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
  do_check_eq(install.existingAddon, addon);

  yield Assert.rejects(promiseInstallUpdate(install),
                       function(err) { return err.code == AddonManager.ERROR_INCORRECT_ID },
                       "Upgrade to a different ID fails");

  addon.uninstall();
});

// Verify that an update to a multi-package xpi fails
add_task(function* test_update_new_id() {
  yield promiseInstallFile(do_get_addon("test_update_multi1"));

  let addon = yield promiseAddonByID("updatemulti@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.version, "1.0");

  let update = yield promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  let install = update.updateAvailable;
  do_check_eq(install.name, addon.name);
  do_check_eq(install.version, "2.0");
  do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
  do_check_eq(install.existingAddon, addon);

  yield Assert.rejects(promiseInstallUpdate(install),
                       function(err) { return err.code == AddonManager.ERROR_UNEXPECTED_ADDON_TYPE },
                       "Upgrade to a multipackage xpi fails");

  addon.uninstall();
});
