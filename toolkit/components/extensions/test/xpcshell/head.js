"use strict";
/* exported createHttpServer, cleanupDir, clearCache, optionalPermissionsPromptHandler, promiseConsoleOutput,
            promiseQuotaManagerServiceReset, promiseQuotaManagerServiceClear,
            runWithPrefs, testEnv, withHandlingUserInput, resetHandlingUserInput,
            assertPersistentListeners, promiseExtensionEvent, assertHasPersistedScriptsCachedFlag,
            assertIsPersistedScriptsCachedFlag
*/

var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var {
  clearInterval,
  clearTimeout,
  setInterval,
  setIntervalWithTarget,
  setTimeout,
  setTimeoutWithTarget,
} = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ContentTask: "resource://testing-common/ContentTask.sys.mjs",
  Extension: "resource://gre/modules/Extension.sys.mjs",
  ExtensionData: "resource://gre/modules/Extension.sys.mjs",
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  MessageChannel: "resource://testing-common/MessageChannel.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  PromiseTestUtils: "resource://testing-common/PromiseTestUtils.sys.mjs",
  Schemas: "resource://gre/modules/Schemas.sys.mjs",
});

PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

// Persistent Listener test functionality
const { assertPersistentListeners } = ExtensionTestUtils.testAssertions;

// https_first automatically upgrades http to https, but the tests are not
// designed to expect that. And it is not easy to change that because
// nsHttpServer does not support https (bug 1742061). So disable https_first.
Services.prefs.setBoolPref("dom.security.https_first", false);

// These values may be changed in later head files and tested in check_remote
// below.
Services.prefs.setBoolPref("browser.tabs.remote.autostart", false);
Services.prefs.setBoolPref("extensions.webextensions.remote", false);
const testEnv = {
  expectRemote: false,
};

add_setup(function check_remote() {
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

// Some tests load non-moz-extension:-URLs in their extension document. When
// extensions run in-process (extensions.webextensions.remote set to false),
// that fails.
// For details, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1724099
// To avoid skip-if on the whole file, use this:
//
//   add_task(async function test_description_here() {
//     // Comment explaining why.
//     allow_unsafe_parent_loads_when_extensions_not_remote();
//     ...
//     revert_allow_unsafe_parent_loads_when_extensions_not_remote();
//   });
var private_upl_cleanup_handlers = [];
function allow_unsafe_parent_loads_when_extensions_not_remote() {
  if (WebExtensionPolicy.useRemoteWebExtensions) {
    // We should only allow remote iframes in the main process.
    return;
  }
  if (!Cu.isInAutomation) {
    // isInAutomation is false by default in xpcshell (bug 1598804). Flip pref.
    Services.prefs.setBoolPref(
      "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
      true
    );
    private_upl_cleanup_handlers.push(() => {
      Services.prefs.setBoolPref(
        "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
        false
      );
    });
    // Sanity check: Fail immediately if setting the above pref does somehow
    // not flip the isInAutomation flag.
    if (!Cu.isInAutomation) {
      // This condition is unexpected, because it is enforced at:
      // https://searchfox.org/mozilla-central/rev/ea65de7c/js/xpconnect/src/xpcpublic.h#753-759
      throw new Error("Failed to set isInAutomation to true");
    }
  }
  // Note: The following pref requires the isInAutomation flag to be set.
  // When unset, the pref is ignored, and tests would encounter bug 1724099.
  if (!Services.prefs.getBoolPref("security.allow_unsafe_parent_loads")) {
    info("Setting pref security.allow_unsafe_parent_loads to true");
    Services.prefs.setBoolPref("security.allow_unsafe_parent_loads", true);
    private_upl_cleanup_handlers.push(() => {
      info("Reverting pref security.allow_unsafe_parent_loads to false");
      Services.prefs.setBoolPref("security.allow_unsafe_parent_loads", false);
    });
  }

  registerCleanupFunction(
    // eslint-disable-next-line no-use-before-define
    revert_allow_unsafe_parent_loads_when_extensions_not_remote
  );
}

function revert_allow_unsafe_parent_loads_when_extensions_not_remote() {
  for (let revert of private_upl_cleanup_handlers.splice(0)) {
    revert();
  }
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

var promiseConsoleOutput = async function (task) {
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
  const { MessageChannel } = ChromeUtils.importESModule(
    "resource://testing-common/MessageChannel.sys.mjs"
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

// Optional Permission prompt handling
const optionalPermissionsPromptHandler = {
  sawPrompt: false,
  acceptPrompt: false,

  init() {
    Services.prefs.setBoolPref(
      "extensions.webextOptionalPermissionPrompts",
      true
    );
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");
    registerCleanupFunction(() => {
      Services.obs.removeObserver(
        this,
        "webextension-optional-permission-prompt"
      );
      Services.prefs.clearUserPref(
        "extensions.webextOptionalPermissionPrompts"
      );
    });
  },

  observe(subject, topic, data) {
    if (topic == "webextension-optional-permission-prompt") {
      this.sawPrompt = true;
      let { resolve } = subject.wrappedJSObject;
      resolve(this.acceptPrompt);
    }
  },
};

function promiseExtensionEvent(wrapper, event) {
  return new Promise(resolve => {
    wrapper.extension.once(event, (...args) => resolve(args));
  });
}

async function assertHasPersistedScriptsCachedFlag(ext) {
  const { StartupCache } = ExtensionParent;
  const allCachedGeneral = StartupCache._data.get("general");
  equal(
    allCachedGeneral
      .get(ext.id)
      ?.get(ext.version)
      ?.get("scripting")
      ?.has("hasPersistedScripts"),
    true,
    "Expect the StartupCache to include hasPersistedScripts flag"
  );
}

async function assertIsPersistentScriptsCachedFlag(ext, expectedValue) {
  const { StartupCache } = ExtensionParent;
  const allCachedGeneral = StartupCache._data.get("general");
  equal(
    allCachedGeneral
      .get(ext.id)
      ?.get(ext.version)
      ?.get("scripting")
      ?.get("hasPersistedScripts"),
    expectedValue,
    "Expected cached value set on hasPersistedScripts flag"
  );
}
