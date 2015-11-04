/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check failures are propogated correctly

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver;
const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(-1);
  gPort = testserver.identity.primaryPort;

  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_missing.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  startupManager();

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Verify that an update check returns the correct errors.
function run_test_1() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "1.0");

    let sawCompat = false;
    let sawUpdate = false;
    a1.findUpdates({
      onNoCompatibilityUpdateAvailable: function(addon) {
        sawCompat = true;
      },

      onCompatibilityUpdateAvailable: function(addon) {
        do_throw("Should not have seen a compatibility update");
      },

      onNoUpdateAvailable: function(addon) {
        sawUpdate = true;
      },

      onUpdateAvailable: function(addon, install) {
        do_throw("Should not have seen an update");
      },

      onUpdateFinished: function(addon, error) {
        do_check_true(sawCompat);
        do_check_true(sawUpdate);
        do_check_eq(error, AddonManager.UPDATE_STATUS_DOWNLOAD_ERROR);
        end_test();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}
