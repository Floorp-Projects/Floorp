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

// Start the tests after the window has been displayed
window.addEventListener("load", function testOnLoad() {
  window.removeEventListener("load", testOnLoad);
  window.addEventListener("MozAfterPaint", function testOnMozAfterPaint() {
    window.removeEventListener("MozAfterPaint", testOnMozAfterPaint);
    setTimeout(testInit, 0);
  });
});

let sdkpath = null;

// Installs a single add-on returning a promise for when install is completed
function installAddon(url) {
  return new Promise(function(resolve, reject) {
    AddonManager.getInstallForURL(url, function(install) {
      install.addListener({
        onDownloadEnded: function(install) {
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
          }
          setPrefs("extensions." + install.addon.id + ".sdk", options);

          // If necessary override the add-ons module paths to point somewhere
          // else
          if (sdkpath) {
            let paths = {}
            for (let path of ["dev", "diffpatcher", "framescript", "method", "node", "sdk", "toolkit"]) {
              paths[path] = sdkpath + path;
            }
            setPrefs("extensions.modules." + install.addon.id + ".path", paths);
          }
        },

        onInstallEnded: function(install, addon) {
          resolve(addon);
        },

        onInstallFailed: function(install) {
          reject();
        }
      });

      install.install();
    }, "application/x-xpinstall");
  });
}

// Uninstalls an add-on returning a promise for when it is gone
function uninstallAddon(oldAddon) {
  return new Promise(function(resolve, reject) {
    AddonManager.addAddonListener({
      onUninstalled: function(addon) {
        if (addon.id != oldAddon.id)
          return;

        // Some add-ons do async work on uninstall, we must wait for that to
        // complete
        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.init(resolve, 500, timer.TYPE_ONE_SHOT);
      }
    });

    oldAddon.uninstall();
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
let testAddon = Task.async(function*(url) {
  let addon = yield installAddon(url);
  let results = yield waitForResults();
  yield uninstallAddon(addon);

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

      if (config.totalChunks && config.thisChunk) {
        fileNames = chunkifyTests(fileNames, config.totalChunks,
                                  config.thisChunk, config.chunkByDir);
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
               "No tests to run. Did you pass an invalid --test-path?\n");
        }
        else {
          dump("Jetpack Addon Test Summary\n");
          dump("\tPassed: " + passed + "\n" +
               "\tFailed: " + failed + "\n" +
               "\tTodo: 0\n");
        }

        if (config.closeWhenDone) {
          const appStartup = Cc['@mozilla.org/toolkit/app-startup;1'].
                             getService(Ci.nsIAppStartup);
          appStartup.quit(appStartup.eAttemptQuit);
        }
      }

      function testNextAddon() {
        if (fileNames.length == 0)
          return finish();

        let filename = fileNames.shift();
        testAddon(filename).then(results => {
          passed += results.passed;
          failed += results.failed;
        }).then(testNextAddon);
      }

      testNextAddon();
    }
    catch (e) {
      dump("TEST-UNEXPECTED-FAIL: jetpack-addon-harness.js | error starting test harness (" + e + ")\n");
      dump(e.stack);
    }
  });
}
