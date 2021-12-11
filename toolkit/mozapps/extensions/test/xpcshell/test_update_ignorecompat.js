/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is disabled but is being kept around so it can eventualy
// be modernized and re-enabled.  But is uses obsolete test helpers that
// fail lint, so just skip linting it for now.
/* eslint-disable */

// This verifies that add-on update checks work correctly when compatibility
// check is disabled.

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/data/", do_get_file("data"));

const profileDir = gProfD.clone();
profileDir.append("extensions");


createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const updateFile = "test_update.json";
const appId = "toolkit@mozilla.org";

// Test that the update check correctly observes the
// extensions.strictCompatibility pref.
add_test(async function() {
  await promiseWriteInstallRDFForExtension({
    id: "addon9@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://example.com/data/" + updateFile,
    targetApplications: [{
      id: appId,
      minVersion: "0.1",
      maxVersion: "0.2",
    }],
    name: "Test Addon 9",
  }, profileDir);

  await promiseRestartManager();

  AddonManager.addInstallListener({
    onNewInstall(aInstall) {
      if (aInstall.existingAddon.id != "addon9@tests.mozilla.org")
        do_throw("Saw unexpected onNewInstall for " + aInstall.existingAddon.id);
      Assert.equal(aInstall.version, "4.0");
    },
    onDownloadFailed(aInstall) {
      run_next_test();
    },
  });

  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                             `http://example.com/data/test_update_addons.json`);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  AddonManagerInternal.backgroundUpdateCheck();
});

// Test that the update check correctly observes when an addon opts-in to
// strict compatibility checking.
add_test(async function() {
  await promiseWriteInstallRDFForExtension({
    id: "addon11@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://example.com/data/" + updateFile,
    targetApplications: [{
      id: appId,
      minVersion: "0.1",
      maxVersion: "0.2",
    }],
    name: "Test Addon 11",
  }, profileDir);

  await promiseRestartManager();

  let a11 = await AddonManager.getAddonByID("addon11@tests.mozilla.org");
  Assert.notEqual(a11, null);

  a11.findUpdates({
    onCompatibilityUpdateAvailable() {
      do_throw("Should not have seen compatibility information");
    },

    onUpdateAvailable() {
      do_throw("Should not have seen an available update");
    },

    onUpdateFinished() {
      run_next_test();
    },
  }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
});
