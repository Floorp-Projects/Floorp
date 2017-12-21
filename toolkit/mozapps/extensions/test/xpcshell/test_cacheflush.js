/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that flushing the zipreader cache happens when appropriate

var gExpectedFile = null;
var gCacheFlushCount = 0;

var CacheFlushObserver = {
  observe(aSubject, aTopic, aData) {
    if (aTopic != "flush-cache-entry")
      return;
    // Ignore flushes triggered by the fake cert DB
    if (aData == "cert-override")
      return;

    Assert.ok(gExpectedFile != null);
    Assert.ok(aSubject instanceof AM_Ci.nsIFile);
    Assert.equal(aSubject.path, gExpectedFile.path);
    gCacheFlushCount++;
  }
};

function run_test() {
  do_test_pending();
  Services.obs.addObserver(CacheFlushObserver, "flush-cache-entry");
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

      Assert.equal(gCacheFlushCount, 1);
      gExpectedFile = null;
      gCacheFlushCount = 0;

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
    Assert.equal(gCacheFlushCount, 1);
    gExpectedFile = null;
    gCacheFlushCount = 0;

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      // We should flush the installed XPI when uninstalling
      Assert.ok(a1 != null);
      a1.uninstall();
      Assert.equal(gCacheFlushCount, 0);

      gExpectedFile = gProfD.clone();
      gExpectedFile.append("extensions");
      gExpectedFile.append("addon1@tests.mozilla.org.xpi");
      restartManager();
      Assert.equal(gCacheFlushCount, 1);
      gExpectedFile = null;
      gCacheFlushCount = 0;

      do_execute_soon(run_test_3);
    });
  });
}

// Tests that the cache is flushed when installing a restartless add-on
function run_test_3() {
  AddonManager.getInstallForFile(do_get_addon("test_cacheflush2"), function(aInstall) {
    aInstall.addListener({
      onInstallStarted() {
        // We should flush the staged XPI when completing the install
        gExpectedFile = gProfD.clone();
        gExpectedFile.append("extensions");
        gExpectedFile.append("staged");
        gExpectedFile.append("addon2@tests.mozilla.org.xpi");
      },

      onInstallEnded() {
        Assert.equal(gCacheFlushCount, 1);
        gExpectedFile = null;
        gCacheFlushCount = 0;

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
    Assert.equal(gCacheFlushCount, 2);
    gExpectedFile = null;
    gCacheFlushCount = 0;

    do_execute_soon(do_test_finished);
  });
}
