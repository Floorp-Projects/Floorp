/* -*- js-indent-level: 2; tab-width: 2; indent-tabs-mode: nil -*- */
// Test timeout (seconds)
var gTimeoutSeconds = 45;
var gConfig;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContentSearch",
  "resource:///modules/ContentSearch.jsm");

const SIMPLETEST_OVERRIDES =
  ["ok", "is", "isnot", "todo", "todo_is", "todo_isnot", "info", "expectAssertions", "requestCompleteLog"];

// non-android is bootstrapped by marionette
if (Services.appinfo.OS == 'Android') {
  window.addEventListener("load", function() {
    window.addEventListener("MozAfterPaint", function() {
      setTimeout(testInit, 0);
    }, {once: true});
  }, {once: true});
} else {
  setTimeout(testInit, 0);
}

var TabDestroyObserver = {
  outstanding: new Set(),
  promiseResolver: null,

  init: function() {
    Services.obs.addObserver(this, "message-manager-close");
    Services.obs.addObserver(this, "message-manager-disconnect");
  },

  destroy: function() {
    Services.obs.removeObserver(this, "message-manager-close");
    Services.obs.removeObserver(this, "message-manager-disconnect");
  },

  observe: function(subject, topic, data) {
    if (topic == "message-manager-close") {
      this.outstanding.add(subject);
    } else if (topic == "message-manager-disconnect") {
      this.outstanding.delete(subject);
      if (!this.outstanding.size && this.promiseResolver) {
        this.promiseResolver();
      }
    }
  },

  wait: function() {
    if (!this.outstanding.size) {
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      this.promiseResolver = resolve;
    });
  },
};

function testInit() {
  gConfig = readConfig();
  if (gConfig.testRoot == "browser") {
    // Make sure to launch the test harness for the first opened window only
    var prefs = Services.prefs;
    if (prefs.prefHasUserValue("testing.browserTestHarness.running"))
      return;

    prefs.setBoolPref("testing.browserTestHarness.running", true);

    if (prefs.prefHasUserValue("testing.browserTestHarness.timeout"))
      gTimeoutSeconds = prefs.getIntPref("testing.browserTestHarness.timeout");

    var sstring = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
    sstring.data = location.search;

    Services.ww.openWindow(window, "chrome://mochikit/content/browser-harness.xul", "browserTest",
                           "chrome,centerscreen,dialog=no,resizable,titlebar,toolbar=no,width=800,height=600", sstring);
  } else {
    // This code allows us to redirect without requiring specialpowers for chrome and a11y tests.
    let messageHandler = function(m) {
      messageManager.removeMessageListener("chromeEvent", messageHandler);
      var url = m.json.data;

      // Window is the [ChromeWindow] for messageManager, so we need content.window
      // Currently chrome tests are run in a content window instead of a ChromeWindow
      var webNav = content.window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation);
      webNav.loadURI(url, null, null, null, null);
    };

    var listener = 'data:,function doLoad(e) { var data=e.detail&&e.detail.data;removeEventListener("contentEvent", function (e) { doLoad(e); }, false, true);sendAsyncMessage("chromeEvent", {"data":data}); };addEventListener("contentEvent", function (e) { doLoad(e); }, false, true);';
    messageManager.addMessageListener("chromeEvent", messageHandler);
    messageManager.loadFrameScript(listener, true);
  }
  if (gConfig.e10s) {
    e10s_init();

    let processCount = prefs.getIntPref("dom.ipc.processCount", 1);
    if (processCount > 1) {
      // Currently starting a content process is slow, to aviod timeouts, let's
      // keep alive content processes.
      prefs.setIntPref("dom.ipc.keepProcessesAlive.web", processCount);
    }

    let globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);
    globalMM.loadFrameScript("chrome://mochikit/content/shutdown-leaks-collector.js", true);
  } else {
    // In non-e10s, only run the ShutdownLeaksCollector in the parent process.
    Components.utils.import("chrome://mochikit/content/ShutdownLeaksCollector.jsm");
  }

  let gmm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
  gmm.loadFrameScript("chrome://mochikit/content/tests/SimpleTest/AsyncUtilsContent.js", true);
}

function Tester(aTests, structuredLogger, aCallback) {
  this.structuredLogger = structuredLogger;
  this.tests = aTests;
  this.callback = aCallback;

  this._scriptLoader = Services.scriptloader;
  this.EventUtils = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.EventUtils);
  var simpleTestScope = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/specialpowersAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SpecialPowersObserverAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromePowers.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/MemoryStats.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;

  var extensionUtilsScope = {
    registerCleanupFunction: (fn) => {
      this.currentTest.scope.registerCleanupFunction(fn);
    },
  };
  extensionUtilsScope.SimpleTest = this.SimpleTest;
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ExtensionTestUtils.js", extensionUtilsScope);
  this.ExtensionTestUtils = extensionUtilsScope.ExtensionTestUtils;

  this.SimpleTest.harnessParameters = gConfig;

  this.MemoryStats = simpleTestScope.MemoryStats;
  this.Task = Task;
  this.ContentTask = Components.utils.import("resource://testing-common/ContentTask.jsm", null).ContentTask;
  this.BrowserTestUtils = Components.utils.import("resource://testing-common/BrowserTestUtils.jsm", null).BrowserTestUtils;
  this.TestUtils = Components.utils.import("resource://testing-common/TestUtils.jsm", null).TestUtils;
  this.Task.Debugging.maintainStack = true;
  this.Promise = Components.utils.import("resource://gre/modules/Promise.jsm", null).Promise;
  this.Assert = Components.utils.import("resource://testing-common/Assert.jsm", null).Assert;

  this.SimpleTestOriginal = {};
  SIMPLETEST_OVERRIDES.forEach(m => {
    this.SimpleTestOriginal[m] = this.SimpleTest[m];
  });

  this._coverageCollector = null;

  this._toleratedUncaughtRejections = null;
  this._uncaughtErrorObserver = ({message, date, fileName, stack, lineNumber}) => {
    let error = message;
    if (fileName || lineNumber) {
      error = {
        fileName: fileName,
        lineNumber: lineNumber,
        message: message,
        toString: function() {
          return message;
        }
      };
    }

    // We may have a whitelist of rejections we wish to tolerate.
    let tolerate = this._toleratedUncaughtRejections &&
      this._toleratedUncaughtRejections.indexOf(message) != -1;
    let name = "A promise chain failed to handle a rejection: ";
    if (tolerate) {
      name = "WARNING: (PLEASE FIX THIS AS PART OF BUG 1077403) " + name;
    }

    this.currentTest.addResult(
      new testResult(
	      /*success*/tolerate,
        /*name*/name,
        /*error*/error,
        /*known*/tolerate,
        /*stack*/stack));
    };
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},
  Task: null,
  ContentTask: null,
  ExtensionTestUtils: null,
  Assert: null,

  repeat: 0,
  runUntilFailure: false,
  checker: null,
  currentTestIndex: -1,
  lastStartTime: null,
  lastAssertionCount: 0,
  failuresFromInitialWindowState: 0,

  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },

  start: function Tester_start() {
    TabDestroyObserver.init();

    //if testOnLoad was not called, then gConfig is not defined
    if (!gConfig)
      gConfig = readConfig();

    if (gConfig.runUntilFailure)
      this.runUntilFailure = true;

    if (gConfig.repeat)
      this.repeat = gConfig.repeat;

    if (gConfig.jscovDirPrefix) {
      let coveragePath = gConfig.jscovDirPrefix;
      let {CoverageCollector} = Cu.import("resource://testing-common/CoverageUtils.jsm",
                                          {});
      this._coverageCollector = new CoverageCollector(coveragePath);
    }

    this.structuredLogger.info("*** Start BrowserChrome Test Results ***");
    Services.console.registerListener(this);
    this._globalProperties = Object.keys(window);
    this._globalPropertyWhitelist = [
      "navigator", "constructor", "top",
      "Application",
      "__SS_tabsToRestore", "__SSi",
      "webConsoleCommandController",
    ];

    this.Promise.Debugging.clearUncaughtErrorObservers();
    this.Promise.Debugging.addUncaughtErrorObserver(this._uncaughtErrorObserver);

    if (this.tests.length)
      this.waitForGraphicsTestWindowToBeGone(this.nextTest.bind(this));
    else
      this.finish();
  },

  waitForGraphicsTestWindowToBeGone(aCallback) {
    let windowsEnum = Services.wm.getEnumerator(null);
    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      if (win != window && !win.closed &&
          win.document.documentURI == "chrome://gfxsanity/content/sanityparent.html") {
        this.BrowserTestUtils.domWindowClosed(win).then(aCallback);
        return;
      }
    }
    // graphics test window is already gone, just call callback immediately
    aCallback();
  },

  waitForWindowsState: function Tester_waitForWindowsState(aCallback) {
    let timedOut = this.currentTest && this.currentTest.timedOut;
    let startTime = Date.now();
    let baseMsg = timedOut ? "Found a {elt} after previous test timed out"
                           : this.currentTest ? "Found an unexpected {elt} at the end of test run"
                                              : "Found an unexpected {elt}";

    // Remove stale tabs
    if (this.currentTest && window.gBrowser && gBrowser.tabs.length > 1) {
      while (gBrowser.tabs.length > 1) {
        let lastTab = gBrowser.tabContainer.lastChild;
        let msg = baseMsg.replace("{elt}", "tab") +
                  ": " + lastTab.linkedBrowser.currentURI.spec;
        this.currentTest.addResult(new testResult(false, msg, "", false));
        gBrowser.removeTab(lastTab);
      }
    }

    // Replace the last tab with a fresh one
    if (window.gBrowser) {
      let newTab = gBrowser.addTab("about:blank", { skipAnimation: true });
      gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
      gBrowser.stop();
    }

    // Remove stale windows
    this.structuredLogger.info("checking window state");
    let windowsEnum = Services.wm.getEnumerator(null);
    let createdFakeTestForLogging = false;
    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      if (win != window && !win.closed &&
          win.document.documentElement.getAttribute("id") != "browserTestHarness") {
        let type = win.document.documentElement.getAttribute("windowtype");
        switch (type) {
        case "navigator:browser":
          type = "browser window";
          break;
        case null:
          type = "unknown window with document URI: " + win.document.documentURI +
                 " and title: " + win.document.title;
          break;
        }
        let msg = baseMsg.replace("{elt}", type);
        if (this.currentTest) {
          this.currentTest.addResult(new testResult(false, msg, "", false));
        } else {
          if (!createdFakeTestForLogging) {
            createdFakeTestForLogging = true;
            this.structuredLogger.testStart("browser-test.js");
          }
          this.failuresFromInitialWindowState++;
          this.structuredLogger.testStatus("browser-test.js",
                                           msg, "FAIL", false, "");
        }

        win.close();
      }
    }
    if (createdFakeTestForLogging) {
      let time = Date.now() - startTime;
      this.structuredLogger.testEnd("browser-test.js",
                                    "OK",
                                    undefined,
                                    "finished window state check in " + time + "ms");
    }

    // Make sure the window is raised before each test.
    this.SimpleTest.waitForFocus(aCallback);
  },

  finish: function Tester_finish(aSkipSummary) {
    this.Promise.Debugging.flushUncaughtErrors();

    var passCount = this.tests.reduce((a, f) => a + f.passCount, 0);
    var failCount = this.tests.reduce((a, f) => a + f.failCount, 0);
    var todoCount = this.tests.reduce((a, f) => a + f.todoCount, 0);

    // Include failures from window state checking prior to running the first test
    failCount += this.failuresFromInitialWindowState;

    if (this.repeat > 0) {
      --this.repeat;
      this.currentTestIndex = -1;
      this.nextTest();
    } else {
      TabDestroyObserver.destroy();
      Services.console.unregisterListener(this);
      this.Promise.Debugging.clearUncaughtErrorObservers();
      this._treatUncaughtRejectionsAsFailures = false;

      // In the main process, we print the ShutdownLeaksCollector message here.
      let pid = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processID;
      dump("Completed ShutdownLeaks collections in process " + pid + "\n");

      this.structuredLogger.info("TEST-START | Shutdown");

      if (this.tests.length) {
        let e10sMode = gMultiProcessBrowser ? "e10s" : "non-e10s";
        this.structuredLogger.info("Browser Chrome Test Summary");
        this.structuredLogger.info("Passed:  " + passCount);
        this.structuredLogger.info("Failed:  " + failCount);
        this.structuredLogger.info("Todo:    " + todoCount);
        this.structuredLogger.info("Mode:    " + e10sMode);
      } else {
        this.structuredLogger.testEnd("browser-test.js",
                                      "FAIL",
                                      "PASS",
                                      "No tests to run. Did you pass invalid test_paths?");
      }
      this.structuredLogger.info("*** End BrowserChrome Test Results ***");

      // Tests complete, notify the callback and return
      this.callback(this.tests);
      this.callback = null;
      this.tests = null;
    }
  },

  haltTests: function Tester_haltTests() {
    // Do not run any further tests
    this.currentTestIndex = this.tests.length - 1;
    this.repeat = 0;
  },

  observe: function Tester_observe(aSubject, aTopic, aData) {
    if (!aTopic) {
      this.onConsoleMessage(aSubject);
    }
  },

  onConsoleMessage: function Tester_onConsoleMessage(aConsoleMessage) {
    // Ignore empty messages.
    if (!aConsoleMessage.message)
      return;

    try {
      var msg = "Console message: " + aConsoleMessage.message;
      if (this.currentTest)
        this.currentTest.addResult(new testMessage(msg));
      else
        this.structuredLogger.info("TEST-INFO | (browser-test.js) | " + msg.replace(/\n$/, "") + "\n");
    } catch (ex) {
      // Swallow exception so we don't lead to another error being reported,
      // throwing us into an infinite loop
    }
  },

  nextTest: Task.async(function*() {
    if (this.currentTest) {
      this.Promise.Debugging.flushUncaughtErrors();
      if (this._coverageCollector) {
        this._coverageCollector.recordTestCoverage(this.currentTest.path);
      }

      // Run cleanup functions for the current test before moving on to the
      // next one.
      let testScope = this.currentTest.scope;
      while (testScope.__cleanupFunctions.length > 0) {
        let func = testScope.__cleanupFunctions.shift();
        try {
          yield func.apply(testScope);
        }
        catch (ex) {
          this.currentTest.addResult(new testResult(false, "Cleanup function threw an exception", ex, false));
        }
      }

      if (this.currentTest.passCount === 0 &&
          this.currentTest.failCount === 0 &&
          this.currentTest.todoCount === 0) {
        this.currentTest.addResult(new testResult(false, "This test contains no passes, no fails and no todos. Maybe it threw a silent exception? Make sure you use waitForExplicitFinish() if you need it.", "", false));
      }

      if (testScope.__expected == 'fail' && testScope.__num_failed <= 0) {
        this.currentTest.addResult(new testResult(false, "We expected at least one assertion to fail because this test file was marked as fail-if in the manifest!", "", true));
      }

      this.Promise.Debugging.flushUncaughtErrors();

      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      if (winUtils.isTestControllingRefreshes) {
        this.currentTest.addResult(new testResult(false, "test left refresh driver under test control", "", false));
        winUtils.restoreNormalRefresh();
      }

      if (this.SimpleTest.isExpectingUncaughtException()) {
        this.currentTest.addResult(new testResult(false, "expectUncaughtException was called but no uncaught exception was detected!", "", false));
      }

      Object.keys(window).forEach(function (prop) {
        if (parseInt(prop) == prop) {
          // This is a string which when parsed as an integer and then
          // stringified gives the original string.  As in, this is in fact a
          // string representation of an integer, so an index into
          // window.frames.  Skip those.
          return;
        }
        if (this._globalProperties.indexOf(prop) == -1) {
          this._globalProperties.push(prop);
          if (this._globalPropertyWhitelist.indexOf(prop) == -1)
            this.currentTest.addResult(new testResult(false, "test left unexpected property on window: " + prop, "", false));
        }
      }, this);

      // Clear document.popupNode.  The test could have set it to a custom value
      // for its own purposes, nulling it out it will go back to the default
      // behavior of returning the last opened popup.
      document.popupNode = null;

      yield new Promise(resolve => SpecialPowers.flushPrefEnv(resolve));

      if (gConfig.cleanupCrashes) {
        let gdir = Services.dirsvc.get("UAppData", Ci.nsIFile);
        gdir.append("Crash Reports");
        gdir.append("pending");
        if (gdir.exists()) {
          let entries = gdir.directoryEntries;
          while (entries.hasMoreElements()) {
            let entry = entries.getNext().QueryInterface(Ci.nsIFile);
            if (entry.isFile()) {
              entry.remove(false);
              let msg = "this test left a pending crash report; deleted " + entry.path;
              this.structuredLogger.info(msg);
            }
          }
        }
      }

      // Notify a long running test problem if it didn't end up in a timeout.
      if (this.currentTest.unexpectedTimeouts && !this.currentTest.timedOut) {
        let msg = "This test exceeded the timeout threshold. It should be " +
                  "rewritten or split up. If that's not possible, use " +
                  "requestLongerTimeout(N), but only as a last resort.";
        this.currentTest.addResult(new testResult(false, msg, "", false));
      }

      // If we're in a debug build, check assertion counts.  This code
      // is similar to the code in TestRunner.testUnloaded in
      // TestRunner.js used for all other types of mochitests.
      let debugsvc = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
      if (debugsvc.isDebugBuild) {
        let newAssertionCount = debugsvc.assertionCount;
        let numAsserts = newAssertionCount - this.lastAssertionCount;
        this.lastAssertionCount = newAssertionCount;

        let max = testScope.__expectedMaxAsserts;
        let min = testScope.__expectedMinAsserts;
        if (numAsserts > max) {
          let msg = "Assertion count " + numAsserts +
                    " is greater than expected range " +
                    min + "-" + max + " assertions.";
          // TEST-UNEXPECTED-FAIL (TEMPORARILY TEST-KNOWN-FAIL)
          //this.currentTest.addResult(new testResult(false, msg, "", false));
          this.currentTest.addResult(new testResult(true, msg, "", true));
        } else if (numAsserts < min) {
          let msg = "Assertion count " + numAsserts +
                    " is less than expected range " +
                    min + "-" + max + " assertions.";
          // TEST-UNEXPECTED-PASS
          this.currentTest.addResult(new testResult(false, msg, "", true));
        } else if (numAsserts > 0) {
          let msg = "Assertion count " + numAsserts +
                    " is within expected range " +
                    min + "-" + max + " assertions.";
          // TEST-KNOWN-FAIL
          this.currentTest.addResult(new testResult(true, msg, "", true));
        }
      }

      // Dump memory stats for main thread.
      if (Cc["@mozilla.org/xre/runtime;1"]
          .getService(Ci.nsIXULRuntime)
          .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT)
      {
        this.MemoryStats.dump(this.currentTestIndex,
                              this.currentTest.path,
                              gConfig.dumpOutputDirectory,
                              gConfig.dumpAboutMemoryAfterTest,
                              gConfig.dumpDMDAfterTest);
      }

      // Note the test run time
      let time = Date.now() - this.lastStartTime;
      this.structuredLogger.testEnd(this.currentTest.path,
                                           "OK",
                                           undefined,
                                           "finished in " + time + "ms");
      this.currentTest.setDuration(time);

      if (this.runUntilFailure && this.currentTest.failCount > 0) {
        this.haltTests();
      }

      // Restore original SimpleTest methods to avoid leaks.
      SIMPLETEST_OVERRIDES.forEach(m => {
        this.SimpleTest[m] = this.SimpleTestOriginal[m];
      });

      this.ContentTask.setTestScope(null);
      testScope.destroy();
      this.currentTest.scope = null;
    }

    // Check the window state for the current test before moving to the next one.
    // This also causes us to check before starting any tests, since nextTest()
    // is invoked to start the tests.
    this.waitForWindowsState(() => {
      if (this.done) {
        if (this._coverageCollector) {
          this._coverageCollector.finalize();
        }

        // Uninitialize a few things explicitly so that they can clean up
        // frames and browser intentionally kept alive until shutdown to
        // eliminate false positives.
        if (gConfig.testRoot == "browser") {
          //Skip if SeaMonkey
          if (AppConstants.MOZ_APP_NAME != "seamonkey") {
            // Replace the document currently loaded in the browser's sidebar.
            // This will prevent false positives for tests that were the last
            // to touch the sidebar. They will thus not be blamed for leaking
            // a document.
            let sidebar = document.getElementById("sidebar");
            sidebar.setAttribute("src", "data:text/html;charset=utf-8,");
            sidebar.docShell.createAboutBlankContentViewer(null);
            sidebar.setAttribute("src", "about:blank");

            SocialShare.uninit();
          }

          // Destroy BackgroundPageThumbs resources.
          let {BackgroundPageThumbs} =
            Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm", {});
          BackgroundPageThumbs._destroy();

          // Destroy preloaded browsers.
          if (gBrowser._preloadedBrowser) {
            let browser = gBrowser._preloadedBrowser;
            gBrowser._preloadedBrowser = null;
            gBrowser.getNotificationBox(browser).remove();
          }
        }

        // Schedule GC and CC runs before finishing in order to detect
        // DOM windows leaked by our tests or the tested code. Note that we
        // use a shrinking GC so that the JS engine will discard JIT code and
        // JIT caches more aggressively.

        let shutdownCleanup = aCallback => {
          Cu.schedulePreciseShrinkingGC(() => {
            // Run the GC and CC a few times to make sure that as much
            // as possible is freed.
            let numCycles = 3;
            for (let i = 0; i < numCycles; i++) {
              Cu.forceGC();
              Cu.forceCC();
            }
            aCallback();
          });
        };


        let {AsyncShutdown} =
          Cu.import("resource://gre/modules/AsyncShutdown.jsm", {});

        let barrier = new AsyncShutdown.Barrier(
          "ShutdownLeaks: Wait for cleanup to be finished before checking for leaks");
        Services.obs.notifyObservers({wrappedJSObject: barrier},
          "shutdown-leaks-before-check");

        barrier.client.addBlocker("ShutdownLeaks: Wait for tabs to finish closing",
                                  TabDestroyObserver.wait());

        barrier.wait().then(() => {
          // Simulate memory pressure so that we're forced to free more resources
          // and thus get rid of more false leaks like already terminated workers.
          Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

          Services.ppmm.broadcastAsyncMessage("browser-test:collect-request");

          shutdownCleanup(() => {
            setTimeout(() => {
              shutdownCleanup(() => {
                this.finish();
              });
            }, 1000);
          });
        });

        return;
      }

      this.currentTestIndex++;
      this.execTest();
    });
  }),

  execTest: function Tester_execTest() {
    this.structuredLogger.testStart(this.currentTest.path);

    this.SimpleTest.reset();

    // Load the tests into a testscope
    let currentScope = this.currentTest.scope = new testScope(this, this.currentTest, this.currentTest.expected);
    let currentTest = this.currentTest;

    // Import utils in the test scope.
    this.currentTest.scope.EventUtils = this.EventUtils;
    this.currentTest.scope.SimpleTest = this.SimpleTest;
    this.currentTest.scope.gTestPath = this.currentTest.path;
    this.currentTest.scope.Task = this.Task;
    this.currentTest.scope.ContentTask = this.ContentTask;
    this.currentTest.scope.BrowserTestUtils = this.BrowserTestUtils;
    this.currentTest.scope.TestUtils = this.TestUtils;
    this.currentTest.scope.ExtensionTestUtils = this.ExtensionTestUtils;
    // Pass a custom report function for mochitest style reporting.
    this.currentTest.scope.Assert = new this.Assert(function(err, message, stack) {
      let res;
      if (err) {
        res = new testResult(false, err.message, err.stack, false, err.stack);
      } else {
        res = new testResult(true, message, "", false, stack);
      }
      currentTest.addResult(res);
    });

    this.ContentTask.setTestScope(currentScope);

    // Allow Assert.jsm methods to be tacked to the current scope.
    this.currentTest.scope.export_assertions = function() {
      for (let func in this.Assert) {
        this[func] = this.Assert[func].bind(this.Assert);
      }
    };

    // Override SimpleTest methods with ours.
    SIMPLETEST_OVERRIDES.forEach(function(m) {
      this.SimpleTest[m] = this[m];
    }, this.currentTest.scope);

    //load the tools to work with chrome .jar and remote
    try {
      this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", this.currentTest.scope);
    } catch (ex) { /* no chrome-harness tools */ }

    // Import head.js script if it exists.
    var currentTestDirPath =
      this.currentTest.path.substr(0, this.currentTest.path.lastIndexOf("/"));
    var headPath = currentTestDirPath + "/head.js";
    try {
      this._scriptLoader.loadSubScript(headPath, this.currentTest.scope);
    } catch (ex) {
      // Ignore if no head.js exists, but report all other errors.  Note this
      // will also ignore an existing head.js attempting to import a missing
      // module - see bug 755558 for why this strategy is preferred anyway.
      if (!/^Error opening input stream/.test(ex.toString())) {
       this.currentTest.addResult(new testResult(false, "head.js import threw an exception", ex, false));
      }
    }

    // Import the test script.
    try {
      this._scriptLoader.loadSubScript(this.currentTest.path,
                                       this.currentTest.scope);
      this.Promise.Debugging.flushUncaughtErrors();
      // Run the test
      this.lastStartTime = Date.now();
      if (this.currentTest.scope.__tasks) {
        // This test consists of tasks, added via the `add_task()` API.
        if ("test" in this.currentTest.scope) {
          throw "Cannot run both a add_task test and a normal test at the same time.";
        }
        let Promise = this.Promise;
        this.Task.spawn(function*() {
          let task;
          while ((task = this.__tasks.shift())) {
            this.SimpleTest.info("Entering test " + task.name);
            try {
              yield task();
            } catch (ex) {
              let isExpected = !!this.SimpleTest.isExpectingUncaughtException();
              let stack = (typeof ex == "object" && "stack" in ex)?ex.stack:null;
              let name = "Uncaught exception";
              let result = new testResult(isExpected, name, ex, false, stack);
              currentTest.addResult(result);
            }
            Promise.Debugging.flushUncaughtErrors();
            this.SimpleTest.info("Leaving test " + task.name);
          }
          this.finish();
        }.bind(currentScope));
      } else if (typeof this.currentTest.scope.test == "function") {
        this.currentTest.scope.test();
      } else {
        throw "This test didn't call add_task, nor did it define a generatorTest() function, nor did it define a test() function, so we don't know how to run it.";
      }
    } catch (ex) {
      let isExpected = !!this.SimpleTest.isExpectingUncaughtException();
      if (!this.SimpleTest.isIgnoringAllUncaughtExceptions()) {
        this.currentTest.addResult(new testResult(isExpected, "Exception thrown", ex, false));
        this.SimpleTest.expectUncaughtException(false);
      } else {
        this.currentTest.addResult(new testMessage("Exception thrown: " + ex));
      }
      this.currentTest.scope.finish();
    }

    // If the test ran synchronously, move to the next test, otherwise the test
    // will trigger the next test when it is done.
    if (this.currentTest.scope.__done) {
      this.nextTest();
    }
    else {
      var self = this;
      var timeoutExpires = Date.now() + gTimeoutSeconds * 1000;
      var waitUntilAtLeast = timeoutExpires - 1000;
      this.currentTest.scope.__waitTimer =
        this.SimpleTest._originalSetTimeout.apply(window, [function timeoutFn() {
        // We sometimes get woken up long before the gTimeoutSeconds
        // have elapsed (when running in chaos mode for example). This
        // code ensures that we don't wrongly time out in that case.
        if (Date.now() < waitUntilAtLeast) {
          self.currentTest.scope.__waitTimer =
            setTimeout(timeoutFn, timeoutExpires - Date.now());
          return;
        }

        if (--self.currentTest.scope.__timeoutFactor > 0) {
          // We were asked to wait a bit longer.
          self.currentTest.scope.info(
            "Longer timeout required, waiting longer...  Remaining timeouts: " +
            self.currentTest.scope.__timeoutFactor);
          self.currentTest.scope.__waitTimer =
            setTimeout(timeoutFn, gTimeoutSeconds * 1000);
          return;
        }

        // If the test is taking longer than expected, but it's not hanging,
        // mark the fact, but let the test continue.  At the end of the test,
        // if it didn't timeout, we will notify the problem through an error.
        // To figure whether it's an actual hang, compare the time of the last
        // result or message to half of the timeout time.
        // Though, to protect against infinite loops, limit the number of times
        // we allow the test to proceed.
        const MAX_UNEXPECTED_TIMEOUTS = 10;
        if (Date.now() - self.currentTest.lastOutputTime < (gTimeoutSeconds / 2) * 1000 &&
            ++self.currentTest.unexpectedTimeouts <= MAX_UNEXPECTED_TIMEOUTS) {
            self.currentTest.scope.__waitTimer =
              setTimeout(timeoutFn, gTimeoutSeconds * 1000);
          return;
        }

        self.currentTest.addResult(new testResult(false, "Test timed out", null, false));
        self.currentTest.timedOut = true;
        self.currentTest.scope.__waitTimer = null;
        self.nextTest();
      }, gTimeoutSeconds * 1000]);
    }
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIConsoleListener) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function testResult(aCondition, aName, aDiag, aIsTodo, aStack) {
  this.name = aName;
  this.msg = "";

  this.info = false;
  this.pass = !!aCondition;
  this.todo = aIsTodo;

  if (this.pass) {
    if (aIsTodo) {
      this.status = "FAIL";
      this.expected = "FAIL";
    } else {
      this.status = "PASS";
      this.expected = "PASS";
    }

  } else {
    if (aDiag) {
      if (typeof aDiag == "object" && "fileName" in aDiag) {
        // we have an exception - print filename and linenumber information
        this.msg += "at " + aDiag.fileName + ":" + aDiag.lineNumber + " - ";
      }
      this.msg += String(aDiag);
    }
    if (aStack) {
      this.msg += "\nStack trace:\n";
      let normalized;
      if (aStack instanceof Components.interfaces.nsIStackFrame) {
        let frames = [];
        for (let frame = aStack; frame; frame = frame.caller) {
          frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
        }
        normalized = frames.join("\n");
      } else {
        normalized = "" + aStack;
      }
      this.msg += Task.Debugging.generateReadableStack(normalized, "    ");
    }
    if (aIsTodo) {
      this.status = "PASS";
      this.expected = "FAIL";
    } else {
      this.status = "FAIL";
      this.expected = "PASS";
    }

    if (gConfig.debugOnFailure) {
      // You've hit this line because you requested to break into the
      // debugger upon a testcase failure on your test run.
      debugger;
    }
  }
}

function testMessage(aName) {
  this.msg = aName || "";
  this.info = true;
}

// Need to be careful adding properties to this object, since its properties
// cannot conflict with global variables used in tests.
function testScope(aTester, aTest, expected) {
  this.__tester = aTester;
  this.__expected = expected;
  this.__num_failed = 0;

  var self = this;
  this.ok = function test_ok(condition, name, diag, stack) {
    if (self.__expected == 'fail') {
        if (!condition) {
          self.__num_failed++;
          condition = true;
        }
    }

    aTest.addResult(new testResult(condition, name, diag, false,
                                   stack ? stack : Components.stack.caller));
  };
  this.is = function test_is(a, b, name) {
    self.ok(a == b, name, "Got " + a + ", expected " + b, false,
            Components.stack.caller);
  };
  this.isnot = function test_isnot(a, b, name) {
    self.ok(a != b, name, "Didn't expect " + a + ", but got it", false,
            Components.stack.caller);
  };
  this.todo = function test_todo(condition, name, diag, stack) {
    aTest.addResult(new testResult(!condition, name, diag, true,
                                   stack ? stack : Components.stack.caller));
  };
  this.todo_is = function test_todo_is(a, b, name) {
    self.todo(a == b, name, "Got " + a + ", expected " + b,
              Components.stack.caller);
  };
  this.todo_isnot = function test_todo_isnot(a, b, name) {
    self.todo(a != b, name, "Didn't expect " + a + ", but got it",
              Components.stack.caller);
  };
  this.info = function test_info(name) {
    aTest.addResult(new testMessage(name));
  };

  this.executeSoon = function test_executeSoon(func) {
    Services.tm.dispatchToMainThread({
      run: function() {
        func();
      }
    });
  };

  this.waitForExplicitFinish = function test_waitForExplicitFinish() {
    self.__done = false;
  };

  this.waitForFocus = function test_waitForFocus(callback, targetWindow, expectBlankPage) {
    self.SimpleTest.waitForFocus(callback, targetWindow, expectBlankPage);
  };

  this.waitForClipboard = function test_waitForClipboard(expected, setup, success, failure, flavor) {
    self.SimpleTest.waitForClipboard(expected, setup, success, failure, flavor);
  };

  this.registerCleanupFunction = function test_registerCleanupFunction(aFunction) {
    self.__cleanupFunctions.push(aFunction);
  };

  this.requestLongerTimeout = function test_requestLongerTimeout(aFactor) {
    self.__timeoutFactor = aFactor;
  };

  this.copyToProfile = function test_copyToProfile(filename) {
    self.SimpleTest.copyToProfile(filename);
  };

  this.expectUncaughtException = function test_expectUncaughtException(aExpecting) {
    self.SimpleTest.expectUncaughtException(aExpecting);
  };

  this.ignoreAllUncaughtExceptions = function test_ignoreAllUncaughtExceptions(aIgnoring) {
    self.SimpleTest.ignoreAllUncaughtExceptions(aIgnoring);
  };

  this.thisTestLeaksUncaughtRejectionsAndShouldBeFixed = function(...rejections) {
    if (!aTester._toleratedUncaughtRejections) {
      aTester._toleratedUncaughtRejections = [];
    }
    aTester._toleratedUncaughtRejections.push(...rejections);
  };

  this.expectAssertions = function test_expectAssertions(aMin, aMax) {
    let min = aMin;
    let max = aMax;
    if (typeof(max) == "undefined") {
      max = min;
    }
    if (typeof(min) != "number" || typeof(max) != "number" ||
        min < 0 || max < min) {
      throw "bad parameter to expectAssertions";
    }
    self.__expectedMinAsserts = min;
    self.__expectedMaxAsserts = max;
  };

  this.setExpected = function test_setExpected() {
    self.__expected = 'fail';
  };

  this.finish = function test_finish() {
    self.__done = true;
    if (self.__waitTimer) {
      self.executeSoon(function() {
        if (self.__done && self.__waitTimer) {
          clearTimeout(self.__waitTimer);
          self.__waitTimer = null;
          self.__tester.nextTest();
        }
      });
    }
  };

  this.requestCompleteLog = function test_requestCompleteLog() {
    self.__tester.structuredLogger.deactivateBuffering();
    self.registerCleanupFunction(function() {
      self.__tester.structuredLogger.activateBuffering();
    })
  };
}
testScope.prototype = {
  __done: true,
  __tasks: null,
  __waitTimer: null,
  __cleanupFunctions: [],
  __timeoutFactor: 1,
  __expectedMinAsserts: 0,
  __expectedMaxAsserts: 0,
  __expected: 'pass',

  EventUtils: {},
  SimpleTest: {},
  Task: null,
  ContentTask: null,
  BrowserTestUtils: null,
  TestUtils: null,
  ExtensionTestUtils: null,
  Assert: null,

  /**
   * Add a test function which is a Task function.
   *
   * Task functions are functions fed into Task.jsm's Task.spawn(). They are
   * generators that emit promises.
   *
   * If an exception is thrown, an assertion fails, or if a rejected
   * promise is yielded, the test function aborts immediately and the test is
   * reported as a failure. Execution continues with the next test function.
   *
   * To trigger premature (but successful) termination of the function, simply
   * return or throw a Task.Result instance.
   *
   * Example usage:
   *
   * add_task(function test() {
   *   let result = yield Promise.resolve(true);
   *
   *   ok(result);
   *
   *   let secondary = yield someFunctionThatReturnsAPromise(result);
   *   is(secondary, "expected value");
   * });
   *
   * add_task(function test_early_return() {
   *   let result = yield somethingThatReturnsAPromise();
   *
   *   if (!result) {
   *     // Test is ended immediately, with success.
   *     return;
   *   }
   *
   *   is(result, "foo");
   * });
   */
  add_task: function(aFunction) {
    if (!this.__tasks) {
      this.waitForExplicitFinish();
      this.__tasks = [];
    }
    this.__tasks.push(aFunction.bind(this));
  },

  destroy: function test_destroy() {
    for (let prop in this)
      delete this[prop];
  }
};
