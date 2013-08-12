/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that flushing the zipreader cache happens when appropriate

var gExpectedFile = null;
var gCacheFlushed = false;

var CacheFlushObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "flush-cache-entry")
      return;

    do_check_true(gExpectedFile != null);
    do_check_true(aSubject instanceof AM_Ci.nsIFile);
    do_check_eq(aSubject.path, gExpectedFile.path);
    gCacheFlushed = true;
    gExpectedFile = null;
  }
};

function run_test() {
  // This test only makes sense when leaving extensions packed
  if (Services.prefs.getBoolPref("extensions.alwaysUnpack"))
    return;

  do_test_pending();
  Services.obs.addObserver(CacheFlushObserver, "flush-cache-entry", false);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  startupManager();

  run_test_1();
}

// Tests that the cache is flushed when cancelling a pending install
function run_test_1() {
  AddonManager.getInstallForFile(do_get_addon("test_cacheflush1"), function(aInstall) {
    completeAllInstalls([aInstall], function() {
      // We should flush the staged XPI when cancelling the install
      gExpectedFile = gProfD.clone();
      gExpectedFile.append("extensions");
      gExpectedFile.append("staged");
      gExpectedFile.append("addon1@tests.mozilla.org.xpi");
      aInstall.cancel();

      do_check_true(gCacheFlushed);
      gCacheFlushed = false;

      run_test_2();
    });
  });
}

// Tests that the cache is flushed when uninstalling an add-on
function run_test_2() {
  installAllFiles([do_get_addon("test_cacheflush1")], function() {
    // Installing will flush the staged XPI during startup
    gExpectedFile = gProfD.clone();
    gExpectedFile.append("extensions");
    gExpectedFile.append("staged");
    gExpectedFile.append("addon1@tests.mozilla.org.xpi");
    restartManager();
    do_check_true(gCacheFlushed);
    gCacheFlushed = false;

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      // We should flush the installed XPI when uninstalling
      gExpectedFile = gProfD.clone();
      gExpectedFile.append("extensions");
      gExpectedFile.append("addon1@tests.mozilla.org.xpi");

      do_check_true(a1 != null);
      a1.uninstall();
      do_check_false(gCacheFlushed);
      do_execute_soon(run_test_3);
    });
  });
}

// Tests that the cache is flushed when installing a restartless add-on
function run_test_3() {
  restartManager();

  AddonManager.getInstallForFile(do_get_addon("test_cacheflush2"), function(aInstall) {
    aInstall.addListener({
      onInstallStarted: function(aInstall) {
        // We should flush the staged XPI when completing the install
        gExpectedFile = gProfD.clone();
        gExpectedFile.append("extensions");
        gExpectedFile.append("staged");
        gExpectedFile.append("addon2@tests.mozilla.org.xpi");
      },

      onInstallEnded: function(aInstall) {
        do_check_true(gCacheFlushed);
        gCacheFlushed = false;

        do_execute_soon(run_test_4);
      }
    });

    aInstall.install();
  });
}

// Tests that the cache is flushed when uninstalling a restartless add-on
function run_test_4() {
  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
    // We should flush the installed XPI when uninstalling
    gExpectedFile = gProfD.clone();
    gExpectedFile.append("extensions");
    gExpectedFile.append("addon2@tests.mozilla.org.xpi");

    a2.uninstall();
    do_check_true(gCacheFlushed);
    gCacheFlushed = false;

    do_execute_soon(do_test_finished);
  });
}
