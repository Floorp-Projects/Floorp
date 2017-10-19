"use strict";

/* exported createHttpServer, promiseConsoleOutput, cleanupDir, clearCache, testEnv
            runWithPrefs */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  ContentTask: "resource://testing-common/ContentTask.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PromiseTestUtils: "resource://testing-common/PromiseTestUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

// These values may be changed in later head files and tested in check_remote
// below.
Services.prefs.setBoolPref("extensions.webextensions.remote", false);
const testEnv = {
  expectRemote: false,
};

add_task(function check_remote() {
  Assert.equal(WebExtensionPolicy.useRemoteWebExtensions, testEnv.expectRemote, "useRemoteWebExtensions matches");
  Assert.equal(WebExtensionPolicy.isExtensionProcess, !testEnv.expectRemote, "testing from extension process");
});

ExtensionTestUtils.init(this);

var createHttpServer = (...args) => {
  AddonTestUtils.maybeInit(this);
  return AddonTestUtils.createHttpServer(...args);
};

if (AppConstants.platform === "android") {
  Services.io.offline = true;
}

/**
 * Clears the HTTP and content image caches.
 */
function clearCache() {
  Services.cache2.clear();

  let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
  imageCache.clearCache(false);
}

var promiseConsoleOutput = async function(task) {
  const DONE = `=== console listener ${Math.random()} done ===`;

  let listener;
  let messages = [];
  let awaitListener = new Promise(resolve => {
    listener = msg => {
      if (msg == DONE) {
        resolve();
      } else {
        void (msg instanceof Ci.nsIConsoleMessage);
        void (msg instanceof Ci.nsIScriptError);
        messages.push(msg);
      }
    };
  });

  Services.console.registerListener(listener);
  try {
    let result = await task();

    Services.console.logStringMessage(DONE);
    await awaitListener;

    return {messages, result};
  } finally {
    Services.console.unregisterListener(listener);
  }
};

// Attempt to remove a directory.  If the Windows OS is still using the
// file sometimes remove() will fail.  So try repeatedly until we can
// remove it or we give up.
function cleanupDir(dir) {
  let count = 0;
  return new Promise((resolve, reject) => {
    function tryToRemoveDir() {
      count += 1;
      try {
        dir.remove(true);
      } catch (e) {
        // ignore
      }
      if (!dir.exists()) {
        return resolve();
      }
      if (count >= 25) {
        return reject(`Failed to cleanup directory: ${dir}`);
      }
      setTimeout(tryToRemoveDir, 100);
    }
    tryToRemoveDir();
  });
}

// Run a test with the specified preferences and then clear them
// right after the test function run (whether it passes or fails).
async function runWithPrefs(prefsToSet, testFn) {
  try {
    for (let [pref, value] of prefsToSet) {
      info(`Setting pref "${pref}": ${value}`);
      switch (typeof(value)) {
        case "boolean":
          Services.prefs.setBoolPref(pref, value);
          break;
        case "number":
          Services.prefs.setIntPref(pref, value);
          break;
        case "string":
          Services.prefs.setStringPref(pref, value);
          break;
        default:
          throw new Error("runWithPrefs doesn't support this pref type yet");
      }
    }
    await testFn();
  } finally {
    for (let [prefName] of prefsToSet) {
      info(`Clearing pref "${prefName}"`);
      Services.prefs.clearUserPref(prefName);
    }
  }
}
