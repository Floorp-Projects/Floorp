/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that background updates & notifications work as expected

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

do_load_httpd_js();
var testserver;
const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  startupManager();

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Verify that with no add-ons installed the background update notifications get
// called
function run_test_1() {
  AddonManager.getAddonsByTypes(["extension", "theme", "locale"], function(aAddons) {
    do_check_eq(aAddons.length, 0);

    Services.obs.addObserver(function() {
      Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");

      run_test_2();
    }, "addons-background-update-complete", false);

    AddonManagerPrivate.backgroundUpdateCheck();
  });
}

// Verify that with two add-ons installed both of which claim to have updates
// available we get the notification after both updates attempted to start
function run_test_2() {
  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_backgroundupdate.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_backgroundupdate.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 2",
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon3@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 3",
  }, profileDir);

  // Background update uses a different pref, if set
  Services.prefs.setCharPref("extensions.update.background.url",
                             "http://localhost:4444/data/test_backgroundupdate.rdf");
  restartManager();

  // Do hotfix checks
  Services.prefs.setCharPref("extensions.hotfix.id", "hotfix@tests.mozilla.org");
  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:4444/missing.rdf");

  let installCount = 0;
  let completeCount = 0;
  let sawCompleteNotification = false;

  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");

    do_check_eq(installCount, 3);
    sawCompleteNotification = true;
  }, "addons-background-update-complete", false);

  AddonManager.addInstallListener({
    onNewInstall: function(aInstall) {
      installCount++;
    },

    onDownloadFailed: function(aInstall) {
      completeCount++;
      if (completeCount == 3) {
        do_check_true(sawCompleteNotification);
        end_test();
      }
    }
  });

  AddonManagerPrivate.backgroundUpdateCheck();
}
