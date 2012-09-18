/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Verify that API functions fail if the Add-ons Manager isn't initialised.

const IGNORE = {
  funcs: ["escapeAddonURI", "getStartupChanges", "addTypeListener",
          "removeTypeListener", "addAddonListener", "removeAddonListener",
          "addInstallListener", "removeInstallListener", "addManagerListener",
          "removeManagerListener"],
  getters: [],
  setters: []
};

const IGNORE_PRIVATE = {
  funcs: ["AddonAuthor", "AddonCompatibilityOverride", "AddonScreenshot",
          "AddonType", "startup", "shutdown", "registerProvider",
          "unregisterProvider", "addStartupChange", "removeStartupChange"],
  getters: [],
  setters: []
};


function test_functions(aObjName, aIgnore) {
  let obj = this[aObjName];
  for (let prop in obj) {
    let desc = Object.getOwnPropertyDescriptor(obj, prop);

    if (typeof desc.value == "function") {
      if (aIgnore.funcs.indexOf(prop) != -1)
        continue;

      try {
        do_print(aObjName + "." + prop + "()");
        obj[prop]();
        do_throw(prop + " did not throw an exception");
      }
      catch (e) {
        if (e.result != Components.results.NS_ERROR_NOT_INITIALIZED)
          do_throw(prop + " threw an unexpected exception: " + e);
      }
    } else {
      if (typeof desc.get == "function" && aIgnore.getters.indexOf(prop) == -1) {
        do_print(aObjName + "." + prop + " getter");
        try {
          let temp = obj[prop];
          do_throw(prop + " did not throw an exception");
        }
        catch (e) {
          if (e.result != Components.results.NS_ERROR_NOT_INITIALIZED)
            do_throw(prop + " threw an unexpected exception: " + e);
        }
      }
      if (typeof desc.set == "function" && aIgnore.setters.indexOf(prop) == -1) {
        do_print(aObjName + "." + prop + " setter");
        try {
          obj[prop] = "i am the walrus";
          do_throw(prop + " did not throw an exception");
        }
        catch (e) {
          if (e.result != Components.results.NS_ERROR_NOT_INITIALIZED)
            do_throw(prop + " threw an unexpected exception: " + e);
        }
      }
    }
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  test_functions("AddonManager", IGNORE);
  test_functions("AddonManagerPrivate", IGNORE_PRIVATE);
  startupManager();
  shutdownManager();
  test_functions("AddonManager", IGNORE);
  test_functions("AddonManagerPrivate", IGNORE_PRIVATE);
}
