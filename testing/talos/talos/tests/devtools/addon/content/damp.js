const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

XPCOMUtils.defineLazyGetter(this, "require", function() {
  let { require } =
    ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
  return require;
});
XPCOMUtils.defineLazyGetter(this, "gDevTools", function() {
  let { gDevTools } = require("devtools/client/framework/devtools");
  return gDevTools;
});
XPCOMUtils.defineLazyGetter(this, "TargetFactory", function() {
  let { TargetFactory } = require("devtools/client/framework/target");
  return TargetFactory;
});

// Record allocation count in new subtests if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "normal". Print allocation sites to stdout if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "verbose".
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");

function getMostRecentBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}

function getActiveTab(window) {
  return window.gBrowser.selectedTab;
}

let gmm = window.getGroupMessageManager("browsers");

const frameScript = "data:," + encodeURIComponent(`(${
  function() {
    addEventListener("load", function(event) {
      let subframe = event.target != content.document;
      sendAsyncMessage("browser-test-utils:loadEvent",
        {subframe, url: event.target.documentURI});
    }, true);
  }
})()`);

gmm.loadFrameScript(frameScript, true);

// This is duplicated from BrowserTestUtils.jsm
function awaitBrowserLoaded(browser, includeSubFrames = false, wantLoad = null) {
  // If browser belongs to tabbrowser-tab, ensure it has been
  // inserted into the document.
  let tabbrowser = browser.ownerGlobal.gBrowser;
  if (tabbrowser && tabbrowser.getTabForBrowser) {
    tabbrowser._insertBrowser(tabbrowser.getTabForBrowser(browser));
  }

  function isWanted(url) {
    if (!wantLoad) {
      return true;
    } else if (typeof(wantLoad) == "function") {
      return wantLoad(url);
    }
    // It's a string.
    return wantLoad == url;
  }

  return new Promise(resolve => {
    let mm = browser.ownerGlobal.messageManager;
    mm.addMessageListener("browser-test-utils:loadEvent", function onLoad(msg) {
      if (msg.target == browser && (!msg.data.subframe || includeSubFrames) &&
          isWanted(msg.data.url)) {
        mm.removeMessageListener("browser-test-utils:loadEvent", onLoad);
        resolve(msg.data.url);
      }
    });
  });
}

/* globals res:true */

function Damp() {
  Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
}

Damp.prototype = {

  async garbageCollect() {
    dump("Garbage collect\n");

    // Minimize memory usage
    // mimic miminizeMemoryUsage, by only flushing JS objects via GC.
    // We don't want to flush all the cache like minimizeMemoryUsage,
    // as it slow down next executions almost like a cold start.

    // See minimizeMemoryUsage code to justify the 3 iterations and the setTimeout:
    // https://searchfox.org/mozilla-central/source/xpcom/base/nsMemoryReporterManager.cpp#2574-2585
    for (let i = 0; i < 3; i++) {
      // See minimizeMemoryUsage code here to justify the GC+CC+GC:
      // https://searchfox.org/mozilla-central/rev/be78e6ea9b10b1f5b2b3b013f01d86e1062abb2b/dom/base/nsJSEnvironment.cpp#341-349
      Cu.forceGC();
      Cu.forceCC();
      Cu.forceGC();
      await new Promise(done => setTimeout(done, 0));
    }
  },

  /**
   * Helper to tell when a test start and when it is finished.
   * It helps recording its duration, but also put markers for perf-html when profiling
   * DAMP.
   *
   * When this method is called, the test is considered to be starting immediately
   * When the test is over, the returned object's `done` method should be called.
   *
   * @param label String
   *        Test title, displayed everywhere in PerfHerder, DevTools Perf Dashboard, ...
   *
   * @return object
   *         With a `done` method, to be called whenever the test is finished running
   *         and we should record its duration.
   */
  runTest(label) {
    if (DEBUG_ALLOCATIONS) {
      if (!this.allocationTracker) {
        this.allocationTracker = this.startAllocationTracker();
      }
      // Flush the current allocations before running the test
      this.allocationTracker.flushAllocations();
    }

    let startLabel = label + ".start";
    performance.mark(startLabel);
    let start = performance.now();

    return {
      done: () => {
        let end = performance.now();
        let duration = end - start;
        performance.measure(label, startLabel);
        this._results.push({
          name: label,
          value: duration
        });

        if (DEBUG_ALLOCATIONS == "normal") {
          this._results.push({
            name: label + ".allocations",
            value: this.allocationTracker.countAllocations()
          });
        } else if (DEBUG_ALLOCATIONS == "verbose") {
          this.allocationTracker.logAllocationSites();
        }
      }
    };
  },

  async addTab(url) {
    let tab = this._win.gBrowser.selectedTab = this._win.gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    await awaitBrowserLoaded(browser);
    return tab;
  },

  closeCurrentTab() {
    this._win.BrowserCloseTabOrWindow();
    return this._win.gBrowser.selectedTab;
  },

  reloadPage(onReload) {
    return new Promise(resolve => {
      let browser = gBrowser.selectedBrowser;
      if (typeof(onReload) == "function") {
        onReload().then(resolve);
      } else {
        resolve(awaitBrowserLoaded(browser));
      }
      browser.reload();
    });
  },

  async testSetup(url) {
    let tab = await this.addTab(url);
    await new Promise(resolve => {
      setTimeout(resolve, this._config.rest);
    });
    return tab;
  },

  async testTeardown() {
    this.closeCurrentTab();

    // Force freeing memory now so that it doesn't happen during the next test
    await this.garbageCollect();

    this._runNextTest();
  },

  // Everything below here are common pieces needed for the test runner to function,
  // just copy and pasted from Tart with /s/TART/DAMP

  _win: undefined,
  _dampTab: undefined,
  _results: [],
  _config: {subtests: [], repeat: 1, rest: 100},
  _nextTestIndex: 0,
  _tests: [],
  _onSequenceComplete: 0,
  _runNextTest() {
    if (this._nextTestIndex >= this._tests.length) {
      this._onSequenceComplete();
      return;
    }

    let test = this._tests[this._nextTestIndex++];

    dump("Loading test : " + test + "\n");
    let testMethod = require("chrome://damp/content/tests/" + test);

    dump("Executing test : " + test + "\n");
    testMethod();
  },
  // Each command at the array a function which must call nextCommand once it's done
  _doSequence(tests, onComplete) {
    this._tests = tests;
    this._onSequenceComplete = onComplete;
    this._results = [];
    this._nextTestIndex = 0;

    this._runNextTest();
  },

  _log(str) {
    if (window.MozillaFileLogger && window.MozillaFileLogger.log)
      window.MozillaFileLogger.log(str);

    window.dump(str);
  },

  _logLine(str) {
    return this._log(str + "\n");
  },

  _reportAllResults() {
    var testNames = [];
    var testResults = [];

    var out = "";
    for (var i in this._results) {
      res = this._results[i];
      var disp = [].concat(res.value).map(function(a) { return (isNaN(a) ? -1 : a.toFixed(1)); }).join(" ");
      out += res.name + ": " + disp + "\n";

      if (!Array.isArray(res.value)) { // Waw intervals array is not reported to talos
        testNames.push(res.name);
        testResults.push(res.value);
      }
    }
    this._log("\n" + out);

    if (content && content.tpRecordTime) {
      content.tpRecordTime(testResults.join(","), 0, testNames.join(","));
    } else {
      // alert(out);
    }
  },

  _onTestComplete: null,

  _doneInternal() {
    if (this.allocationTracker) {
      this.allocationTracker.stop();
      this.allocationTracker = null;
    }
    this._logLine("DAMP_RESULTS_JSON=" + JSON.stringify(this._results));
    this._reportAllResults();
    this._win.gBrowser.selectedTab = this._dampTab;

    if (this._onTestComplete) {
      this._onTestComplete(JSON.parse(JSON.stringify(this._results))); // Clone results
    }
  },

  startAllocationTracker() {
    const { allocationTracker } = require("devtools/shared/test-helpers/allocation-tracker");
    return allocationTracker();
  },

  startTest(doneCallback, config) {
    dump("Initialize the head file with a reference to this DAMP instance\n");
    let head = require("chrome://damp/content/tests/head.js");
    head.initialize(this);

    this._onTestComplete = function(results) {
      TalosParentProfiler.pause("DAMP - end");
      doneCallback(results);
    };
    this._config = config;

    this._win = Services.wm.getMostRecentWindow("navigator:browser");
    this._dampTab = this._win.gBrowser.selectedTab;
    this._win.gBrowser.selectedBrowser.focus(); // Unfocus the URL bar to avoid caret blink

    TalosParentProfiler.resume("DAMP - start");

    let tests = {};

    // Run cold test only once
    let topWindow = getMostRecentBrowserWindow();
    if (!topWindow.coldRunDAMP) {
      topWindow.coldRunDAMP = true;
      tests["cold.inspector"] = "inspector/cold-open.js";
    }

    tests["panelsInBackground.reload"] = "toolbox/panels-in-background.js";

    // Run all tests against "simple" document
    tests["simple.inspector"] = "inspector/simple.js";
    tests["simple.webconsole"] = "webconsole/simple.js";
    tests["simple.debugger"] = "debugger/simple.js";
    tests["simple.styleeditor"] = "styleeditor/simple.js";
    tests["simple.performance"] = "performance/simple.js";
    tests["simple.netmonitor"] = "netmonitor/simple.js";
    tests["simple.saveAndReadHeapSnapshot"] = "memory/simple.js";

    // Run all tests against "complicated" document
    tests["complicated.inspector"] = "inspector/complicated.js";
    tests["complicated.webconsole"] = "webconsole/complicated.js";
    tests["complicated.debugger"] = "debugger/complicated.js";
    tests["complicated.styleeditor"] = "styleeditor/complicated.js";
    tests["complicated.performance"] = "performance/complicated.js";
    tests["complicated.netmonitor"] = "netmonitor/complicated.js";
    tests["complicated.saveAndReadHeapSnapshot"] = "memory/complicated.js";

    // Run all tests against a document specific to each tool
    tests["custom.inspector"] = "inspector/custom.js";
    tests["custom.debugger"] = "debugger/custom.js";
    tests["custom.webconsole"] = "webconsole/custom.js";

    // Run individual tests covering a very precise tool feature.
    tests["console.bulklog"] = "webconsole/bulklog.js";
    tests["console.streamlog"] = "webconsole/streamlog.js";
    tests["console.objectexpand"] = "webconsole/objectexpand.js";
    tests["console.openwithcache"] = "webconsole/openwithcache.js";
    tests["inspector.mutations"] = "inspector/mutations.js";
    tests["inspector.layout"] = "inspector/layout.js";
    // ⚠  Adding new individual tests slows down DAMP execution ⚠
    // ⚠  Consider contributing to custom.${tool} rather than adding isolated tests ⚠
    // ⚠  See http://docs.firefox-dev.tools/tests/writing-perf-tests.html ⚠

    // Filter tests via `./mach --subtests filter` command line argument
    let filter = Services.prefs.getCharPref("talos.subtests", "");
    if (filter) {
      for (let name in tests) {
        if (!name.includes(filter)) {
          delete tests[name];
        }
      }
      if (Object.keys(tests).length == 0) {
        dump("ERROR: Unable to find any test matching '" + filter + "'\n");
        this._doneInternal();
        return;
      }
    }

    // Construct the sequence array while filtering tests
    let sequenceArray = [];
    for (var i in config.subtests) {
      for (var r = 0; r < config.repeat; r++) {
        if (!config.subtests[i] || !tests[config.subtests[i]]) {
          continue;
        }

        sequenceArray.push(tests[config.subtests[i]]);
      }
    }

    // Free memory before running the first test, otherwise we may have a GC
    // related to Firefox startup or DAMP setup during the first test.
    this.garbageCollect().then(() => {
      this._doSequence(sequenceArray, this._doneInternal);
    }).catch(e => {
      dump("Exception while running DAMP tests: " + e + "\n" + e.stack + "\n");
    });
  }
};
