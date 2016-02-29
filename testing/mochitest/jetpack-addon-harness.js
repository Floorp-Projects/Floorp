/* -*- js-indent-level: 2; tab-width: 2; indent-tabs-mode: nil -*- */
var gConfig;

if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var Cu = Components.utils;
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

// How long to wait for an add-on to uninstall before aborting
const MAX_UNINSTALL_TIME = 10000;
setTimeout(testInit, 0);

var sdkpath = null;

// Strip off the chrome prefix to get the actual path of the test directory
function realPath(chrome) {
  return chrome.substring("chrome://mochitests/content/jetpack-addon/".length)
               .replace(".xpi", "");
}

const chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"]
      .getService(Ci.nsIChromeRegistry);

// Installs a single add-on returning a promise for when install is completed
function installAddon(url) {
  let chromeURL = Services.io.newURI(url, null, null);
  let file = chromeRegistry.convertChromeURL(chromeURL)
      .QueryInterface(Ci.nsIFileURL).file;

  let addon;
  const listener = {
    onInstalling(_addon) {
      addon = _addon;
      // Set add-on's test options
      const options = {
        test: {
          iterations: 1,
          stop: false,
          keepOpen: true,
        },
        profile: {
          memory: false,
          leaks: false,
        },
        output: {
          logLevel: "verbose",
          format: "tbpl",
        },
        console: {
          logLevel: "info",
        },
      }
      setPrefs("extensions." + addon.id + ".sdk", options);

      // If necessary override the add-ons module paths to point somewhere
      // else
      if (sdkpath) {
        let paths = {}
        for (let path of ["dev", "diffpatcher", "framescript", "method", "node", "sdk", "toolkit"]) {
          paths[path] = sdkpath + path;
        }
        setPrefs("extensions.modules." + addon.id + ".path", paths);
      }
    },
  };
  AddonManager.addAddonListener(listener);

  return AddonManager.installTemporaryAddon(file)
    .then(() => {
      AddonManager.removeAddonListener(listener);
      return addon;
    });
}

// Uninstalls an add-on returning a promise for when it is gone
function uninstallAddon(oldAddon) {
  return new Promise(function(resolve, reject) {
    AddonManager.addAddonListener({
      onUninstalled: function(addon) {
        if (addon.id != oldAddon.id)
          return;

        AddonManager.removeAddonListener(this);

        dump("TEST-INFO | jetpack-addon-harness.js | Uninstalled test add-on " + addon.id + "\n");

        // Some add-ons do async work on uninstall, we must wait for that to
        // complete
        setTimeout(resolve, 500);
      }
    });

    oldAddon.uninstall();

    // The uninstall should happen quickly, if not throw an exception
    setTimeout(() => {
      reject(new Error(`Addon ${oldAddon.id} failed to uninstall in a timely fashion.`));
    }, MAX_UNINSTALL_TIME);
  });
}

// Waits for a test add-on to signal it has completed its tests
function waitForResults() {
  return new Promise(function(resolve, reject) {
    Services.obs.addObserver(function(subject, topic, data) {
      Services.obs.removeObserver(arguments.callee, "sdk:test:results");

      resolve(JSON.parse(data));
    }, "sdk:test:results", false);
  });
}

// Runs tests for the add-on available at URL.
var testAddon = Task.async(function*({ url }) {
  dump("TEST-INFO | jetpack-addon-harness.js | Installing test add-on " + realPath(url) + "\n");
  let addon = yield installAddon(url);

  let results = yield waitForResults();

  dump("TEST-INFO | jetpack-addon-harness.js | Uninstalling test add-on " + addon.id + "\n");
  yield uninstallAddon(addon);

  dump("TEST-INFO | jetpack-addon-harness.js | Testing add-on " + realPath(url) + " is complete\n");
  return results;
});

// Sets a set of prefs for test add-ons
function setPrefs(root, options) {
  Object.keys(options).forEach(id => {
    const key = root + "." + id;
    const value = options[id]
    const type = typeof(value);

    value === null ? void(0) :
    value === undefined ? void(0) :
    type === "boolean" ? Services.prefs.setBoolPref(key, value) :
    type === "string" ? Services.prefs.setCharPref(key, value) :
    type === "number" ? Services.prefs.setIntPref(key, parseInt(value)) :
    type === "object" ? setPrefs(key, value) :
    void(0);
  });
}

function testInit() {
  // Make sure to run the test harness for the first opened window only
  if (Services.prefs.prefHasUserValue("testing.jetpackTestHarness.running"))
    return;

  Services.prefs.setBoolPref("testing.jetpackTestHarness.running", true);

  // Get the list of tests to run
  let config = readConfig();
  getTestList(config, function(links) {
    try {
      let fileNames = [];
      let fileNameRegexp = /.+\.xpi$/;
      arrayOfTestFiles(links, fileNames, fileNameRegexp);

      if (config.startAt || config.endAt) {
        fileNames = skipTests(fileNames, config.startAt, config.endAt);
      }

      // Override the SDK modules if necessary
      try {
        let sdklibs = Services.prefs.getCharPref("extensions.sdk.path");
        // sdkpath is a file path, make it a URI
        let sdkfile = Cc["@mozilla.org/file/local;1"].
                      createInstance(Ci.nsIFile);
        sdkfile.initWithPath(sdklibs);
        sdkpath = Services.io.newFileURI(sdkfile).spec;
      }
      catch (e) {
        // Stick with the built-in modules
      }

      let passed = 0;
      let failed = 0;

      function finish() {
        if (passed + failed == 0) {
          dump("TEST-UNEXPECTED-FAIL | jetpack-addon-harness.js | " +
               "No tests to run. Did you pass invalid test_paths?\n");
        }
        else {
          dump("Jetpack Addon Test Summary\n");
          dump("\tPassed: " + passed + "\n" +
               "\tFailed: " + failed + "\n" +
               "\tTodo: 0\n");
        }

        if (config.closeWhenDone) {
          dump("TEST-INFO | jetpack-addon-harness.js | Shutting down.\n");

          const appStartup = Cc['@mozilla.org/toolkit/app-startup;1'].
                             getService(Ci.nsIAppStartup);
          appStartup.quit(appStartup.eAttemptQuit);
        }
      }

      function testNextAddon() {
        if (fileNames.length == 0)
          return finish();

        let filename = fileNames.shift();
        dump("TEST-INFO | jetpack-addon-harness.js | Starting test add-on " + realPath(filename.url) + "\n");
        testAddon(filename).then(results => {
          passed += results.passed;
          failed += results.failed;
        }).then(testNextAddon, error => {
          // If something went wrong during the test then a previous test add-on
          // may still be installed, this leaves us in an unexpected state so
          // probably best to just abandon testing at this point
          failed++;
          dump("TEST-UNEXPECTED-FAIL | jetpack-addon-harness.js | Error testing " + realPath(filename.url) + ": " + error + "\n");
          finish();
        });
      }

      testNextAddon();
    }
    catch (e) {
      dump("TEST-UNEXPECTED-FAIL | jetpack-addon-harness.js | error starting test harness (" + e + ")\n");
      dump(e.stack);
    }
  });
}
