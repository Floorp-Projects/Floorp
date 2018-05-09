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
let testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();
});

// Verify that an update to an add-on with a new ID fails
add_task(async function test_update_new_id() {
  await promiseInstallFile(do_get_addon("test_updateid1"));

  let addon = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");

  let update = await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  let install = update.updateAvailable;
  Assert.equal(install.name, addon.name);
  Assert.equal(install.version, "2.0");
  Assert.equal(install.state, AddonManager.STATE_AVAILABLE);
  Assert.equal(install.existingAddon, addon);

  await Assert.rejects(promiseInstallUpdate(install),
                       function(err) { return err.code == AddonManager.ERROR_INCORRECT_ID; },
                       "Upgrade to a different ID fails");

  addon.uninstall();
});
