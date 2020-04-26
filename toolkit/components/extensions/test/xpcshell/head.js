"use strict";

/* exported createHttpServer, cleanupDir, clearCache, promiseConsoleOutput,
            promiseQuotaManagerServiceReset, promiseQuotaManagerServiceClear,
            runWithPrefs, testEnv, withHandlingUserInput, resetHandlingUserInput */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var {
  clearInterval,
  clearTimeout,
  setInterval,
  setIntervalWithTarget,
  setTimeout,
  setTimeoutWithTarget,
} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  ContentTask: "resource://testing-common/ContentTask.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PromiseTestUtils: "resource://testing-common/PromiseTestUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);

// These values may be changed in later head files and tested in check_remote
// below.
Services.prefs.setBoolPref("extensions.webextensions.remote", false);
const testEnv = {
  expectRemote: false,
};

add_task(function check_remote() {
  Assert.equal(
    WebExtensionPolicy.useRemoteWebExtensions,
    testEnv.expectRemote,
    "useRemoteWebExtensions matches"
  );
  Assert.equal(
    WebExtensionPolicy.isExtensionProcess,
    !testEnv.expectRemote,
    "testing from extension process"
  );
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

    return { messages, result };
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

// Run a test with the specified preferences and then restores their initial values
// right after the test function run (whether it passes or fails).
async function runWithPrefs(prefsToSet, testFn) {
  const setPrefs = prefs => {
    for (let [pref, value] of prefs) {
      if (value === undefined) {
        // Clear any pref that didn't have a user value.
        info(`Clearing pref "${pref}"`);
        Services.prefs.clearUserPref(pref);
        continue;
      }

      info(`Setting pref "${pref}": ${value}`);
      switch (typeof value) {
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
  };

  const getPrefs = prefs => {
    return prefs.map(([pref, value]) => {
      info(`Getting initial pref value for "${pref}"`);
      if (!Services.prefs.prefHasUserValue(pref)) {
        // Check if the pref doesn't have a user value.
        return [pref, undefined];
      }
      switch (typeof value) {
        case "boolean":
          return [pref, Services.prefs.getBoolPref(pref)];
        case "number":
          return [pref, Services.prefs.getIntPref(pref)];
        case "string":
          return [pref, Services.prefs.getStringPref(pref)];
        default:
          throw new Error("runWithPrefs doesn't support this pref type yet");
      }
    });
  };

  let initialPrefsValues = [];

  try {
    initialPrefsValues = getPrefs(prefsToSet);

    setPrefs(prefsToSet);

    await testFn();
  } finally {
    info("Restoring initial preferences values on exit");
    setPrefs(initialPrefsValues);
  }
}

// "Handling User Input" test helpers.

let extensionHandlers = new WeakSet();

function handlingUserInputFrameScript() {
  /* globals content */
  // eslint-disable-next-line no-shadow
  const { MessageChannel } = ChromeUtils.import(
    "resource://gre/modules/MessageChannel.jsm"
  );

  let handle;
  MessageChannel.addListener(this, "ExtensionTest:HandleUserInput", {
    receiveMessage({ name, data }) {
      if (data) {
        handle = content.windowUtils.setHandlingUserInput(true);
      } else if (handle) {
        handle.destruct();
        handle = null;
      }
    },
  });
}

// If you use withHandlingUserInput then restart the addon manager,
// you need to reset this before using withHandlingUserInput again.
function resetHandlingUserInput() {
  extensionHandlers = new WeakSet();
}

async function withHandlingUserInput(extension, fn) {
  let { messageManager } = extension.extension.groupFrameLoader;

  if (!extensionHandlers.has(extension)) {
    messageManager.loadFrameScript(
      `data:,(${encodeURI(handlingUserInputFrameScript)}).call(this)`,
      false,
      true
    );
    extensionHandlers.add(extension);
  }

  await MessageChannel.sendMessage(
    messageManager,
    "ExtensionTest:HandleUserInput",
    true
  );
  await fn();
  await MessageChannel.sendMessage(
    messageManager,
    "ExtensionTest:HandleUserInput",
    false
  );
}

// QuotaManagerService test helpers.

function promiseQuotaManagerServiceReset() {
  info("Calling QuotaManagerService.reset to enforce new test storage limits");
  return new Promise(resolve => {
    Services.qms.reset().callback = resolve;
  });
}

function promiseQuotaManagerServiceClear() {
  info(
    "Calling QuotaManagerService.clear to empty the test data and refresh test storage limits"
  );
  return new Promise(resolve => {
    Services.qms.clear().callback = resolve;
  });
}
