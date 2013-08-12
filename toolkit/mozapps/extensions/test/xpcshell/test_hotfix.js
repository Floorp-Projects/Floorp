/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that hotfix installation works

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
// Ignore any certificate requirements the app has set
Services.prefs.setBoolPref("extensions.hotfix.cert.checkAttributes", false);

Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;
testserver.registerDirectory("/addons/", do_get_file("addons"));
mapFile("/data/test_hotfix_1.rdf", testserver);
mapFile("/data/test_hotfix_2.rdf", testserver);
mapFile("/data/test_hotfix_3.rdf", testserver);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Test that background updates find and install any available hotfix
function run_test_1() {
  Services.prefs.setCharPref("extensions.hotfix.id", "hotfix@tests.mozilla.org");
  Services.prefs.setCharPref("extensions.update.background.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_1.rdf");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], callback_soon(check_test_1));

  // Fake a timer event
  gInternalManager.notify(null);
}

function check_test_1() {
  restartManager();

  AddonManager.getAddonByID("hotfix@tests.mozilla.org", function(aAddon) {
    do_check_neq(aAddon, null);
    do_check_eq(aAddon.version, "1.0");

    aAddon.uninstall();
    do_execute_soon(run_test_2);
  });
}

// Don't install an already used hotfix
function run_test_2() {
  restartManager();

  AddonManager.addInstallListener({
    onNewInstall: function() {
      do_throw("Should not have seen a new install created");
    }
  });

  function observer() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    do_execute_soon(run_test_3);
  }

  Services.obs.addObserver(observer, "addons-background-update-complete", false);

  // Fake a timer event
  gInternalManager.notify(null);
}

// Install a newer hotfix
function run_test_3() {
  restartManager();
  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_2.rdf");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], callback_soon(check_test_3));

  // Fake a timer event
  gInternalManager.notify(null);
}

function check_test_3() {
  restartManager();

  AddonManager.getAddonByID("hotfix@tests.mozilla.org", function(aAddon) {
    do_check_neq(aAddon, null);
    do_check_eq(aAddon.version, "2.0");

    aAddon.uninstall();
    do_execute_soon(run_test_4);
  });
}

// Don't install an incompatible hotfix
function run_test_4() {
  restartManager();

  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_3.rdf");

  AddonManager.addInstallListener({
    onNewInstall: function() {
      do_throw("Should not have seen a new install created");
    }
  });

  function observer() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    do_execute_soon(run_test_5);
  }

  Services.obs.addObserver(observer, "addons-background-update-complete", false);

  // Fake a timer event
  gInternalManager.notify(null);
}

// Don't install an older hotfix
function run_test_5() {
  restartManager();

  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_1.rdf");

  AddonManager.addInstallListener({
    onNewInstall: function() {
      do_throw("Should not have seen a new install created");
    }
  });

  function observer() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    do_execute_soon(run_test_6);
  }

  Services.obs.addObserver(observer, "addons-background-update-complete", false);

  // Fake a timer event
  gInternalManager.notify(null);
}

// Don't re-download an already pending install
function run_test_6() {
  restartManager();

  Services.prefs.setCharPref("extensions.hotfix.lastVersion", "0");
  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_1.rdf");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], callback_soon(check_test_6));

  // Fake a timer event
  gInternalManager.notify(null);
}

function check_test_6() {
  AddonManager.addInstallListener({
    onNewInstall: function() {
      do_throw("Should not have seen a new install created");
    }
  });

  function observer() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    restartManager();

    AddonManager.getAddonByID("hotfix@tests.mozilla.org", function(aAddon) {
      aAddon.uninstall();
      do_execute_soon(run_test_7);
    });
  }

  Services.obs.addObserver(observer, "addons-background-update-complete", false);

  // Fake a timer event
  gInternalManager.notify(null);
}

// Start downloading again if something cancels the install
function run_test_7() {
  restartManager();

  Services.prefs.setCharPref("extensions.hotfix.lastVersion", "0");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_7);

  // Fake a timer event
  gInternalManager.notify(null);
}

function check_test_7(aInstall) {
  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  }, [
    "onInstallCancelled",
  ]);

  aInstall.cancel();

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], callback_soon(finish_test_7));

  // Fake a timer event
  gInternalManager.notify(null);
}

function finish_test_7() {
  restartManager();

  AddonManager.getAddonByID("hotfix@tests.mozilla.org", function(aAddon) {
    do_check_neq(aAddon, null);
    do_check_eq(aAddon.version, "1.0");

    aAddon.uninstall();
    do_execute_soon(run_test_8);
  });
}

// Cancel a pending install when a newer version is already available
function run_test_8() {
  restartManager();

  Services.prefs.setCharPref("extensions.hotfix.lastVersion", "0");
  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_1.rdf");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_8);

  // Fake a timer event
  gInternalManager.notify(null);
}

function check_test_8() {
  Services.prefs.setCharPref("extensions.hotfix.url", "http://localhost:" +
                             gPort + "/data/test_hotfix_2.rdf");

  prepare_test({
    "hotfix@tests.mozilla.org": [
      "onOperationCancelled",
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallCancelled",
    "onInstallEnded",
  ], finish_test_8);

  // Fake a timer event
  gInternalManager.notify(null);
}

function finish_test_8() {
  AddonManager.getAllInstalls(callback_soon(function(aInstalls) {
    do_check_eq(aInstalls.length, 1);
    do_check_eq(aInstalls[0].version, "2.0");

    restartManager();

    AddonManager.getAddonByID("hotfix@tests.mozilla.org", callback_soon(function(aAddon) {
      do_check_neq(aAddon, null);
      do_check_eq(aAddon.version, "2.0");

      aAddon.uninstall();
      restartManager();

      end_test();
    }));
  }));
}
