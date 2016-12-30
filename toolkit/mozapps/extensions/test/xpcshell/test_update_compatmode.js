/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check correctly fills in the
// %COMPATIBILITY_MODE% token in the update URL.


// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;
mapFile("/data/test_updatecompatmode_ignore.rdf", testserver);
mapFile("/data/test_updatecompatmode_normal.rdf", testserver);
mapFile("/data/test_updatecompatmode_strict.rdf", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension({
    id: "compatmode-normal@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - normal"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-strict@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-strict-optin@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict opt-in",
    strictCompatibility: true
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-ignore@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - ignore",
  }, profileDir);

  startupManager();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}


// Strict compatibility checking disabled.
function run_test_1() {
  do_print("Testing with strict compatibility checking disabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  AddonManager.getAddonByID("compatmode-normal@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable(unused, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished() {
        run_test_2();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Strict compatibility checking enabled.
function run_test_2() {
  do_print("Testing with strict compatibility checking enabled");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  AddonManager.getAddonByID("compatmode-strict@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable(unused, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished() {
        run_test_3();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Strict compatibility checking opt-in.
function run_test_3() {
  do_print("Testing with strict compatibility disabled, but addon opt-in");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  AddonManager.getAddonByID("compatmode-strict-optin@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable() {
        do_throw("Should have not have seen compatibility information");
      },

      onUpdateAvailable() {
        do_throw("Should not have seen an available update");
      },

      onUpdateFinished() {
        run_test_4();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Compatibility checking disabled.
function run_test_4() {
  do_print("Testing with all compatibility checking disabled");
  AddonManager.checkCompatibility = false;
  AddonManager.getAddonByID("compatmode-ignore@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable(unused, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished() {
        end_test();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}
