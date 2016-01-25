/* -*- js-indent-level: 2; tab-width: 2; indent-tabs-mode: nil -*- */
const TEST_PACKAGE = "chrome://mochitests/content/";

// Make sure to use the real add-on ID to get the e10s shims activated
const TEST_ID = "mochikit@mozilla.org";

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

setTimeout(testInit, 0);

// Tests a single module
function testModule(require, { url, expected }) {
  return new Promise(resolve => {
    let path = url.substring(TEST_PACKAGE.length);

    const { stdout } = require("sdk/system");

    const { runTests } = require("sdk/test/harness");
    const loaderModule = require("toolkit/loader");
    const options = require("sdk/test/options");

    function findAndRunTests(loader, nextIteration) {
      const { TestRunner } = loaderModule.main(loader, "sdk/deprecated/unit-test");

      const NOT_TESTS = ['setup', 'teardown'];
      var runner = new TestRunner();

      let tests = [];

      let suiteModule;
      try {
        dump("TEST-INFO: " + path + " | Loading test module\n");
        suiteModule = loaderModule.main(loader, "tests/" + path.substring(0, path.length - 3));
      }
      catch (e) {
        // If `Unsupported Application` error thrown during test,
        // skip the test suite
        suiteModule = {
          'test suite skipped': assert => assert.pass(e.message)
        };
      }

      for (let name of Object.keys(suiteModule).sort()) {
        if (NOT_TESTS.indexOf(name) != -1)
          continue;

        tests.push({
          setup: suiteModule.setup,
          teardown: suiteModule.teardown,
          testFunction: suiteModule[name],
          name: path + "." + name
        });
      }

      runner.startMany({
        tests: {
          getNext: () => Promise.resolve(tests.shift())
        },
        stopOnError: options.stopOnError,
        onDone: nextIteration
      });
    }

    runTests({
      findAndRunTests: findAndRunTests,
      iterations: options.iterations,
      filter: options.filter,
      profileMemory: options.profileMemory,
      stopOnError: options.stopOnError,
      verbose: options.verbose,
      parseable: options.parseable,
      print: stdout.write,
      onDone: resolve
    });
  });
}

// Sets the test prefs
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

  // Need to set this very early, otherwise the false value gets cached in
  // DOM bindings code.
  Services.prefs.setBoolPref("dom.indexedDB.experimental", true);

  // Get the list of tests to run
  let config = readConfig();
  getTestList(config, function(links) {
    try {
      let fileNames = [];
      let fileNameRegexp = /test-.+\.js$/;
      arrayOfTestFiles(links, fileNames, fileNameRegexp);

      if (config.startAt || config.endAt) {
        fileNames = skipTests(fileNames, config.startAt, config.endAt);
      }

      // The SDK assumes it is being run from resource URIs
      let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
      let realPath = chromeReg.convertChromeURL(Services.io.newURI(TEST_PACKAGE, null, null));
      let resProtocol = Cc["@mozilla.org/network/protocol;1?name=resource"].getService(Ci.nsIResProtocolHandler);
      resProtocol.setSubstitution("jetpack-package-tests", realPath);

      // Set the test options
      const options = {
        test: {
          iterations: config.runUntilFailure ? config.repeat : 1,
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
      setPrefs("extensions." + TEST_ID + ".sdk", options);

      // Override the SDK modules if necessary
      let sdkpath = "resource://gre/modules/commonjs/";
      try {
        let sdklibs = Services.prefs.getCharPref("extensions.sdk.path");
        // sdkpath is a file path, make it a URI and map a resource URI to it
        let sdkfile = Cc["@mozilla.org/file/local;1"].
                      createInstance(Ci.nsIFile);
        sdkfile.initWithPath(sdklibs);
        let sdkuri = Services.io.newFileURI(sdkfile);
        resProtocol.setSubstitution("jetpack-modules", sdkuri);
        sdkpath = "resource://jetpack-modules/";
      }
      catch (e) {
        // Stick with the built-in modules
      }

      const paths = {
        "": sdkpath,
        "tests/": "resource://jetpack-package-tests/",
      };

      // Create the base module loader to load the test harness
      const loaderID = "toolkit/loader";
      const loaderURI = paths[""] + loaderID + ".js";
      const loaderModule = Cu.import(loaderURI, {}).Loader;

      const modules = {};

      // Manually set the loader's module cache to include itself;
      // which otherwise fails due to lack of `Components`.
      modules[loaderID] = loaderModule;
      modules["@test/options"] = {};

      let loader = loaderModule.Loader({
        id: TEST_ID,
        name: "addon-sdk",
        version: "1.0",
        loadReason: "install",
        paths: paths,
        modules: modules,
        isNative: true,
        rootURI: paths["tests/"],
        prefixURI: paths["tests/"],
        metadata: {},
      });

      const module = loaderModule.Module(loaderID, loaderURI);
      const require = loaderModule.Require(loader, module);

      // Wait until the add-on window is ready
      require("sdk/addon/window").ready.then(() => {
        let passed = 0;
        let failed = 0;

        function finish() {
          if (passed + failed == 0) {
            dump("TEST-UNEXPECTED-FAIL | jetpack-package-harness.js | " +
                 "No tests to run. Did you pass invalid test_paths?\n");
          }
          else {
            dump("Jetpack Package Test Summary\n");
            dump("\tPassed: " + passed + "\n" +
                 "\tFailed: " + failed + "\n" +
                 "\tTodo: 0\n");
          }

          if (config.closeWhenDone) {
            require("sdk/system").exit(failed == 0 ? 0 : 1);
          }
          else {
            loaderModule.unload(loader, "shutdown");
          }
        }

        function testNextModule() {
          if (fileNames.length == 0)
            return finish();

          let filename = fileNames.shift();
          testModule(require, filename).then(tests => {
            passed += tests.passed;
            failed += tests.failed;
          }).then(testNextModule);
        }

        testNextModule();
      });
    }
    catch (e) {
      dump("TEST-UNEXPECTED-FAIL: jetpack-package-harness.js | error starting test harness (" + e + ")\n");
      dump(e.stack);
    }
  });
}
