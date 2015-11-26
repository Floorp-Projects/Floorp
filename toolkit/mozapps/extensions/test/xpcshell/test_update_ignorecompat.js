/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update checks work correctly when compatibility
// check is disabled.

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

var testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
mapFile("/data/test_update.rdf", testserver);
mapFile("/data/test_update.json", testserver);
mapFile("/data/test_update.xml", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

const profileDir = gProfD.clone();
profileDir.append("extensions");


createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

let testParams = [
  { updateFile: "test_update.rdf",
    appId: "xpcshell@tests.mozilla.org" },
  { updateFile: "test_update.json",
    appId: "toolkit@mozilla.org" },
];

for (let test of testParams) {
  let { updateFile, appId } = test;

  // Test that the update check correctly observes the
  // extensions.strictCompatibility pref and compatibility overrides.
  add_test(function () {
    writeInstallRDFForExtension({
      id: "addon9@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2"
      }],
      name: "Test Addon 9",
    }, profileDir);

    restartManager();

    AddonManager.addInstallListener({
      onNewInstall: function(aInstall) {
        if (aInstall.existingAddon.id != "addon9@tests.mozilla.org")
          do_throw("Saw unexpected onNewInstall for " + aInstall.existingAddon.id);
        do_check_eq(aInstall.version, "4.0");
      },
      onDownloadFailed: function(aInstall) {
        run_next_test();
      }
    });

    Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERFORMANCE,
                               "http://localhost:" + gPort + "/data/" + updateFile);
    Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

    AddonManagerInternal.backgroundUpdateCheck();
  });

  // Test that the update check correctly observes when an addon opts-in to
  // strict compatibility checking.
  add_test(function () {
    writeInstallRDFForExtension({
      id: "addon11@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2"
      }],
      name: "Test Addon 11",
    }, profileDir);

    restartManager();

    AddonManager.getAddonByID("addon11@tests.mozilla.org", function(a11) {
      do_check_neq(a11, null);

      a11.findUpdates({
        onCompatibilityUpdateAvailable: function() {
          do_throw("Should not have seen compatibility information");
        },

        onUpdateAvailable: function() {
          do_throw("Should not have seen an available update");
        },

        onUpdateFinished: function() {
          run_next_test();
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });
}
