/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Verify that API functions fail if the Add-ons Manager isn't initialised.

const IGNORE = ["getPreferredIconURL", "escapeAddonURI",
                "shouldAutoUpdate", "getStartupChanges",
                "addTypeListener", "removeTypeListener",
                "addAddonListener", "removeAddonListener",
                "addInstallListener", "removeInstallListener",
                "addManagerListener", "removeManagerListener",
                "shutdown", "init",
                "stateToString", "errorToString", "getUpgradeListener",
                "addUpgradeListener", "removeUpgradeListener"];

const IGNORE_PRIVATE = ["AddonAuthor", "AddonCompatibilityOverride",
                        "AddonScreenshot", "AddonType", "startup", "shutdown",
                        "addonIsActive", "registerProvider", "unregisterProvider",
                        "addStartupChange", "removeStartupChange",
                        "getNewSideloads",
                        "recordTimestamp", "recordSimpleMeasure",
                        "recordException", "getSimpleMeasures", "simpleTimer",
                        "setTelemetryDetails", "getTelemetryDetails",
                        "callNoUpdateListeners", "backgroundUpdateTimerHandler",
                        "hasUpgradeListener", "getUpgradeListener",
                        "isDBLoaded", "recordTiming", "BOOTSTRAP_REASONS"];

async function test_functions() {
  for (let prop in AddonManager) {
    if (IGNORE.includes(prop))
      continue;
    if (typeof AddonManager[prop] != "function")
      continue;

    let args = [];

    // Getter functions need a callback and in some cases not having one will
    // throw before checking if the add-ons manager is initialized so pass in
    // an empty one.
    if (prop.startsWith("get")) {
      // For now all getter functions with more than one argument take the
      // callback in the second argument.
      if (AddonManager[prop].length > 1) {
        args.push(undefined, () => {});
      } else {
        args.push(() => {});
      }
    }

    // Clean this up in bug 1365720
    if (prop == "getActiveAddons") {
      args = [];
    }

    try {
      info("AddonManager." + prop);
      await AddonManager[prop](...args);
      do_throw(prop + " did not throw an exception");
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NOT_INITIALIZED)
        do_throw(prop + " threw an unexpected exception: " + e);
    }
  }

  for (let prop in AddonManagerPrivate) {
    if (IGNORE_PRIVATE.includes(prop))
      continue;
    if (typeof AddonManagerPrivate[prop] != "function")
      continue;

    try {
      info("AddonManagerPrivate." + prop);
      AddonManagerPrivate[prop]();
      do_throw(prop + " did not throw an exception");
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NOT_INITIALIZED)
        do_throw(prop + " threw an unexpected exception: " + e);
    }
  }
}

add_task(async function() {
  await test_functions();
  await promiseStartupManager();
  await promiseShutdownManager();
  await test_functions();
});

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  run_next_test();
}
