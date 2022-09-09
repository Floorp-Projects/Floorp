/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals dampWindow */

const { Ci, Cc, Cu } = require("chrome");
const { gBrowser, MozillaFileLogger, requestIdleCallback } = dampWindow;

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");

const DampLoadParentModule = require("damp-test/actors/DampLoadParent.jsm");
const DAMP_TESTS = require("damp-test/damp-tests.js");

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

// Record allocation count in new subtests if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "normal". Print allocation sites to stdout if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "verbose".
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");

const DEBUG_SCREENSHOTS = env.get("DEBUG_DEVTOOLS_SCREENSHOTS");

// Maximum time spent in one test, in milliseconds
const TEST_TIMEOUT = 5 * 60000;

function getMostRecentBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}

function Damp() {}

Damp.prototype = {
  async garbageCollect() {
    dump("Garbage collect\n");
    let startTime = Cu.now();

    // Minimize memory usage
    // mimic miminizeMemoryUsage, by only flushing JS objects via GC.
    // We don't want to flush all the cache like minimizeMemoryUsage,
    // as it slow down next executions almost like a cold start.

    // See minimizeMemoryUsage code to justify the 3 iterations and the setTimeout:
    // https://searchfox.org/mozilla-central/rev/33c21c060b7f3a52477a73d06ebcb2bf313c4431/xpcom/base/nsMemoryReporterManager.cpp#2574-2585,2591-2594
    for (let i = 0; i < 3; i++) {
      // See minimizeMemoryUsage code here to justify the GC+CC+GC:
      // https://searchfox.org/mozilla-central/rev/be78e6ea9b10b1f5b2b3b013f01d86e1062abb2b/dom/base/nsJSEnvironment.cpp#341-349
      Cu.forceGC();
      Cu.forceCC();
      Cu.forceGC();
      await new Promise(done => setTimeout(done, 0));
    }
    ChromeUtils.addProfilerMarker(
      "DAMP",
      { startTime, category: "Test" },
      "GC"
    );
  },

  async ensureTalosParentProfiler() {
    // TalosParentProfiler is part of TalosPowers, which is a separate WebExtension
    // that may or may not already have finished loading at this point (unlike most
    // Pageloader tests, Damp doesn't wait for Pageloader to find TalosPowers before
    // running). getTalosParentProfiler is used to wait for TalosPowers to be around
    // before continuing.
    async function getTalosParentProfiler() {
      try {
        const {
          TalosParentProfiler,
        } = require("resource://talos-powers/TalosParentProfiler.jsm");
        return TalosParentProfiler;
      } catch (err) {
        await new Promise(resolve => setTimeout(resolve, 500));
        return getTalosParentProfiler();
      }
    }

    this.TalosParentProfiler = await getTalosParentProfiler();
  },

  // Take a screenshot of the whole browser window and open it in a background tab
  async screenshot(label) {
    const win = this._win;
    const canvas = win.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "html:canvas"
    );
    const context = canvas.getContext("2d");
    canvas.width = win.innerWidth;
    canvas.height = win.innerHeight;
    context.drawWindow(win, 0, 0, canvas.width, canvas.height, "white");
    const imgURL = canvas.toDataURL();
    const url = `data:text/html,<title>${label}</title>
      <h1>${label}</h1>
      <img width="100%" height="100%" src="${imgURL}"/>`;
    this._win.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  /**
   * Helper to tell when a test start and when it is finished.
   * It helps recording its duration, but also put markers for profiler.firefox.com
   * when profiling DAMP.
   *
   * When this method is called, the test is considered to be starting immediately
   * When the test is over, the returned object's `done` method should be called.
   *
   * @param label String
   *        Test title, displayed everywhere in PerfHerder, DevTools Perf Dashboard, ...
   * @param record Boolean
   *        Optional, if passed false, the test won't be recorded. It won't appear in
   *        PerfHerder. Instead we will record profiler.firefox.com markers and only
   *        print the timings on stdout.
   *
   * @return object
   *         With a `done` method, to be called whenever the test is finished running
   *         and we should record its duration.
   */
  runTest(label, record = true) {
    if (DEBUG_ALLOCATIONS) {
      if (!this.allocationTracker) {
        this.allocationTracker = this.startAllocationTracker();
      }
      // Flush the current allocations before running the test
      this.allocationTracker.flushAllocations();
    }

    let start = Cu.now();

    return {
      done: () => {
        let end = Cu.now();
        let duration = end - start;
        ChromeUtils.addProfilerMarker(
          "DAMP",
          { startTime: start, category: "Test" },
          label
        );
        if (record) {
          this._results.push({
            name: label,
            value: duration,
          });
        } else {
          dump(`'${label}' took ${duration}ms.\n`);
        }

        if (DEBUG_ALLOCATIONS == "normal" && record) {
          this._results.push({
            name: label + ".allocations",
            value: this.allocationTracker.countAllocations(),
          });
        } else if (DEBUG_ALLOCATIONS == "verbose") {
          this.allocationTracker.logAllocationSites();
        }
        if (DEBUG_SCREENSHOTS) {
          this.screenshot(label);
        }
      },
    };
  },

  async addTab(url) {
    // Disable opening animation to avoid intermittents and prevent having to wait for
    // animation's end. (See bug 1480953)
    let tab = (this._win.gBrowser.selectedTab = this._win.gBrowser.addTrustedTab(
      url,
      { skipAnimation: true }
    ));
    let browser = tab.linkedBrowser;
    await this._awaitBrowserLoaded(browser);
    return tab;
  },

  async waitForPendingPaints(window) {
    let utils = window.windowUtils;
    let startTime = Cu.now();
    while (utils.isMozAfterPaintPending) {
      await new Promise(done => {
        window.addEventListener(
          "MozAfterPaint",
          function listener() {
            ChromeUtils.addProfilerMarker(
              "DAMP",
              { category: "Test" },
              "pending paint"
            );
            done();
          },
          { once: true }
        );
      });
    }
    ChromeUtils.addProfilerMarker(
      "DAMP",
      { startTime, category: "Test" },
      "pending paints"
    );
  },

  reloadPage(onReload) {
    return new Promise(resolve => {
      let browser = gBrowser.selectedBrowser;
      if (typeof onReload == "function") {
        onReload().then(resolve);
      } else {
        resolve(this._awaitBrowserLoaded(browser));
      }
      browser.reload();
    });
  },

  async testSetup(url) {
    let tab = await this.addTab(url);
    await new Promise(resolve => {
      setTimeout(resolve, 100);
    });
    return tab;
  },

  async testTeardown(url) {
    // Disable closing animation to avoid intermittents and prevent having to wait for
    // animation's end. (See bug 1480953)
    this._win.gBrowser.removeCurrentTab({ animate: false });

    // Force freeing memory now so that it doesn't happen during the next test
    await this.garbageCollect();

    let duration = Math.round(Cu.now() - this._startTime);
    dump(`${this._currentTest} took ${duration}ms.\n`);

    this._runNextTest();
  },

  _win: undefined,
  _dampTab: undefined,
  _results: [],
  _nextTestIndex: 0,
  _tests: [],
  _onSequenceComplete: 0,

  // Timeout ID to guard against current test never finishing
  _timeout: null,

  // The unix time at which the current test started (ms)
  _startTime: null,

  // Name of the test currently executed (i.e. path from /tests folder)
  _currentTest: null,

  _runNextTest() {
    clearTimeout(this._timeout);

    if (this._nextTestIndex >= this._tests.length) {
      this._onSequenceComplete();
      return;
    }

    let test = this._tests[this._nextTestIndex++];
    this._startTime = Cu.now();
    this._currentTest = test;

    dump(`Loading test '${test}'\n`);
    let testMethod = require(`damp-test/tests/${test}`);

    this._timeout = setTimeout(() => {
      this.error("Test timed out");
    }, TEST_TIMEOUT);

    dump(`Executing test '${test}'\n`);
    let promise = testMethod();

    // If test method is an async function, ensure catching its exceptions
    if (promise && typeof promise.catch == "function") {
      promise.catch(e => {
        this.exception(e);
      });
    }
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
    if (MozillaFileLogger && MozillaFileLogger.log) {
      MozillaFileLogger.log(str);
    }

    dump(str);
  },

  _logLine(str) {
    return this._log(str + "\n");
  },

  _reportAllResults() {
    const testNames = [];
    const testResults = [];

    let out = "";
    for (const i in this._results) {
      const res = this._results[i];
      const disp = []
        .concat(res.value)
        .map(function(a) {
          return isNaN(a) ? -1 : a.toFixed(1);
        })
        .join(" ");
      out += res.name + ": " + disp + "\n";

      if (!Array.isArray(res.value)) {
        // Waw intervals array is not reported to talos
        testNames.push(res.name);
        testResults.push(res.value);
      }
    }
    this._log("\n" + out);

    if (DEBUG_SCREENSHOTS) {
      // When we are printing screenshots, we don't want to want to exit firefox
      // so that we have time to view them.
      dump(
        "All tests are finished, please review the screenshots and close the browser manually.\n"
      );
      return;
    }

    if (this.testDone) {
      this.testDone({ testResults, testNames });
    } else {
      // alert(out);
    }
  },

  _doneInternal() {
    // Ignore any duplicated call to this method
    // Call startTest() again in order to reset this flag.
    if (this._done) {
      return;
    }
    this._done = true;

    if (this.allocationTracker) {
      this.allocationTracker.stop();
      this.allocationTracker = null;
    }
    this._win.gBrowser.selectedTab = this._dampTab;

    if (this._results) {
      this._logLine("DAMP_RESULTS_JSON=" + JSON.stringify(this._results));
      this._reportAllResults();
    }

    ChromeUtils.addProfilerMarker("DAMP", {
      startTime: this._startTimestamp,
      category: "Test",
    });
    this.TalosParentProfiler.pause();

    this._unregisterDampLoadActors();
  },

  startAllocationTracker() {
    const {
      allocationTracker,
    } = require("devtools/shared/test-helpers/allocation-tracker");
    return allocationTracker();
  },

  error(message) {
    // Log a unique prefix in order to be interpreted as an error and stop DAMP from
    // testing/talos/talos/talos_process.py
    dump("TEST-UNEXPECTED-FAIL | damp | ");

    // Print the currently executed test, if we already started executing one
    if (this._currentTest) {
      dump(this._currentTest + ": ");
    }

    dump(message + "\n");

    // Stop further test execution and immediatly close DAMP
    this._tests = [];
    this._results = null;
    this._doneInternal();
  },

  exception(e) {
    this.error(e);
    dump(e.stack + "\n");
  },

  // Waits for any pending operations that may execute on Firefox startup and that
  // can still be pending when we start running DAMP tests.
  async waitBeforeRunningTests() {
    // Addons may still be being loaded, so wait for them to be fully set up.
    if (!AddonManager.isReady) {
      let onAddonManagerReady = new Promise(resolve => {
        let listener = {
          onStartup() {
            AddonManager.removeManagerListener(listener);
            resolve();
          },
          onShutdown() {},
        };
        AddonManager.addManagerListener(listener);
      });
      await onAddonManagerReady;
    }

    // SessionRestore triggers some saving sequence on idle,
    // so wait for that to be processed before starting tests.
    // https://searchfox.org/mozilla-central/rev/83a923ef7a3b95a516f240a6810c20664b1e0ac9/browser/components/sessionstore/content/content-sessionStore.js#828-830
    // https://searchfox.org/mozilla-central/rev/83a923ef7a3b95a516f240a6810c20664b1e0ac9/browser/components/sessionstore/content/content-sessionStore.js#858
    await new Promise(resolve => {
      setTimeout(resolve, 1500);
    });
    await new Promise(resolve => {
      requestIdleCallback(resolve, { timeout: 15000 });
    });

    await this.ensureTalosParentProfiler();

    // Free memory before running the first test, otherwise we may have a GC
    // related to Firefox startup or DAMP setup during the first test.
    await this.garbageCollect();
  },

  /**
   * This is the main entry point for DAMP, called from
   * testing/talos/talos/tests/devtools/addon/api
   */
  startTest() {
    let promise = new Promise(resolve => {
      this.testDone = resolve;
    });

    try {
      // Is DAMP finished executing? Help preventing async execution when DAMP had an error
      this._done = false;

      this._registerDampLoadActors();

      this._win = Services.wm.getMostRecentWindow("navigator:browser");
      this._dampTab = this._win.gBrowser.selectedTab;
      this._win.gBrowser.selectedBrowser.focus(); // Unfocus the URL bar to avoid caret blink

      // Filter tests via `./mach --subtests filter` command line argument
      let filter = Services.prefs.getCharPref("talos.subtests", "");

      const suite = Services.prefs.getCharPref("talos.damp.suite", "");
      let testSuite;
      if (suite === "all") {
        testSuite = Object.values(DAMP_TESTS).flat();
      } else {
        testSuite = DAMP_TESTS[suite];
        if (!testSuite) {
          this.error(`Unable to find any test suite matching '${suite}'`);
        }
      }

      let tests = testSuite
        .filter(test => !test.disabled)
        .filter(test => test.name.includes(filter));

      if (tests.length === 0) {
        this.error(`Unable to find any test matching '${filter}'`);
      }

      // Run cold test only once
      let topWindow = getMostRecentBrowserWindow();
      if (topWindow.coldRunDAMPDone) {
        tests = tests.filter(test => !test.cold);
      } else {
        topWindow.coldRunDAMPDone = true;
      }

      // Construct the sequence array while filtering tests
      let sequenceArray = [];
      for (let test of tests) {
        sequenceArray.push(test.path);
      }

      this.waitBeforeRunningTests()
        .then(() => {
          this._startTimestamp = Cu.now();
          this.TalosParentProfiler.resume();
          this._doSequence(sequenceArray, this._doneInternal);
        })
        .catch(e => {
          this.exception(e);
        });
    } catch (e) {
      this.exception(e);
    }

    return promise;
  },

  /**
   * Wait for a page-show/load event on the provided browser element, using the
   * JSWindowActor pair at content/actors/DampLoad.
   */
  _awaitBrowserLoaded(browser) {
    dump(
      `Wait for a pageshow event for browsing context ${browser.browsingContext.id}\n`
    );
    return new Promise(resolve => {
      const eventDispatcher = DampLoadParentModule.EventDispatcher;
      const onPageShow = (eventName, data) => {
        dump(`Received pageshow event for ${data.browsingContext.id}\n`);
        if (data.browsingContext !== browser.browsingContext) {
          return;
        }

        eventDispatcher.off("DampLoadParent:PageShow", onPageShow);
        resolve();
      };

      eventDispatcher.on("DampLoadParent:PageShow", onPageShow);
    });
  },

  _registerDampLoadActors() {
    dump(`[DampLoad helper] Register DampLoad actors\n`);
    ChromeUtils.registerWindowActor("DampLoad", {
      kind: "JSWindowActor",
      parent: {
        moduleURI: "resource://damp-test/content/actors/DampLoadParent.jsm",
      },
      child: {
        moduleURI: "resource://damp-test/content/actors/DampLoadChild.jsm",
        events: {
          pageshow: { mozSystemGroup: true },
        },
      },

      // Only listen to top level content frame load.
      allFrames: false,
      includeChrome: false,
    });
  },

  _unregisterDampLoadActors() {
    dump(`[DampLoad helper] Unregister DampLoad actors\n`);
    ChromeUtils.unregisterWindowActor("DampLoad");
  },
};

exports.damp = new Damp();
