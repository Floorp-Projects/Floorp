/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that flushing the zipreader cache happens when appropriate

var gExpectedFile = null;
var gCacheFlushCount = 0;

var CacheFlushObserver = {
  observe(aSubject, aTopic, aData) {
    if (aTopic != "flush-cache-entry") {
      return;
    }

    // Ignore flushes from the fake cert DB or extension-process-script
    if (aData == "cert-override" || aSubject == null) {
      return;
    }

    if (!gExpectedFile) {
      return;
    }
    ok(aSubject instanceof Ci.nsIFile);
    equal(aSubject.path, gExpectedFile.path);
    gCacheFlushCount++;
  },
};

add_task(async function setup() {
  Services.obs.addObserver(CacheFlushObserver, "flush-cache-entry");
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  await promiseStartupManager();
});

// Tests that the cache is flushed when installing a restartless add-on
add_task(async function test_flush_restartless_install() {
  let xpi = await createTempWebExtensionFile({
    manifest: {
      name: "Cache Flush Test",
      version: "2.0",
      applications: { gecko: { id: "addon2@tests.mozilla.org" } },
    },
  });

  let install = await AddonManager.getInstallForFile(xpi);

  await new Promise(resolve => {
    install.addListener({
      onInstallStarted() {
        // We should flush the staged XPI when completing the install
        gExpectedFile = gProfD.clone();
        gExpectedFile.append("extensions");
        gExpectedFile.append("staged");
        gExpectedFile.append("addon2@tests.mozilla.org.xpi");
      },

      onInstallEnded() {
        equal(gCacheFlushCount, 1);
        gExpectedFile = null;
        gCacheFlushCount = 0;

        resolve();
      },
    });

    install.install();
  });
});

// Tests that the cache is flushed when uninstalling a restartless add-on
add_task(async function test_flush_uninstall() {
  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");

  // We should flush the installed XPI when uninstalling
  gExpectedFile = gProfD.clone();
  gExpectedFile.append("extensions");
  gExpectedFile.append("addon2@tests.mozilla.org.xpi");

  await addon.uninstall();

  ok(gCacheFlushCount >= 1);
  gExpectedFile = null;
  gCacheFlushCount = 0;
});
