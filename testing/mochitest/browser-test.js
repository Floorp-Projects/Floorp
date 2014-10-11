/* -*- js-indent-level: 2; tab-width: 2; indent-tabs-mode: nil -*- */
// Test timeout (seconds)
var gTimeoutSeconds = 45;
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

XPCOMUtils.defineLazyModuleGetter(this, "BrowserNewTabPreloader",
  "resource:///modules/BrowserNewTabPreloader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizationTabPreloader",
  "resource:///modules/CustomizationTabPreloader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContentSearch",
  "resource:///modules/ContentSearch.jsm");

const SIMPLETEST_OVERRIDES =
  ["ok", "is", "isnot", "ise", "todo", "todo_is", "todo_isnot", "info", "expectAssertions", "requestCompleteLog"];

window.addEventListener("load", function testOnLoad() {
  window.removeEventListener("load", testOnLoad);
  window.addEventListener("MozAfterPaint", function testOnMozAfterPaint() {
    window.removeEventListener("MozAfterPaint", testOnMozAfterPaint);
    setTimeout(testInit, 0);
  });
});

function testInit() {
  gConfig = readConfig();
  if (gConfig.testRoot == "browser" ||
      gConfig.testRoot == "metro" ||
      gConfig.testRoot == "webapprtChrome") {
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
    messageManager.loadFrameScript(listener, true);
    messageManager.addMessageListener("chromeEvent", messageHandler);
  }
  if (gConfig.e10s) {
    e10s_init();
  }
}

function Tester(aTests, aDumper, aCallback) {
  this.dumper = aDumper;
  this.tests = aTests;
  this.callback = aCallback;
  this.openedWindows = {};
  this.openedURLs = {};

  this._scriptLoader = Services.scriptloader;
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.EventUtils);
  var simpleTestScope = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/specialpowersAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SpecialPowersObserverAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromePowers.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/MemoryStats.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;

  this.SimpleTest.harnessParameters = gConfig;

  this.MemoryStats = simpleTestScope.MemoryStats;
  this.Task = Task;
  this.Task.Debugging.maintainStack = true;
  this.Promise = Components.utils.import("resource://gre/modules/Promise.jsm", null).Promise;
  this.Assert = Components.utils.import("resource://testing-common/Assert.jsm", null).Assert;

  this.SimpleTestOriginal = {};
  SIMPLETEST_OVERRIDES.forEach(m => {
    this.SimpleTestOriginal[m] = this.SimpleTest[m];
  });

  this._toleratedUncaughtRejections = null;
  this._uncaughtErrorObserver = function({message, date, fileName, stack, lineNumber}) {
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
    }.bind(this);
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},
  Task: null,
  Promise: null,
  Assert: null,

  repeat: 0,
  runUntilFailure: false,
  checker: null,
  currentTestIndex: -1,
  lastStartTime: null,
  openedWindows: null,
  lastAssertionCount: 0,

  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },

  start: function Tester_start() {
    //if testOnLoad was not called, then gConfig is not defined
    if (!gConfig)
      gConfig = readConfig();

    if (gConfig.runUntilFailure)
      this.runUntilFailure = true;

    if (gConfig.repeat)
      this.repeat = gConfig.repeat;

    this.dumper.structuredLogger.info("*** Start BrowserChrome Test Results ***");
    Services.console.registerListener(this);
    Services.obs.addObserver(this, "chrome-document-global-created", false);
    Services.obs.addObserver(this, "content-document-global-created", false);
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
      this.nextTest();
    else
      this.finish();
  },

  waitForWindowsState: function Tester_waitForWindowsState(aCallback) {
    let timedOut = this.currentTest && this.currentTest.timedOut;
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
      gBrowser.addTab("about:blank", { skipAnimation: true });
      gBrowser.removeCurrentTab();
      gBrowser.stop();
    }

    // Remove stale windows
    this.dumper.structuredLogger.info("checking window state");
    let windowsEnum = Services.wm.getEnumerator(null);
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
          type = "unknown window";
          break;
        }
        let msg = baseMsg.replace("{elt}", type);
        if (this.currentTest)
          this.currentTest.addResult(new testResult(false, msg, "", false));
        else
          this.dumper.structuredLogger.testEnd("browser-test.js",
                                               "FAIL",
                                               "PASS",
                                               msg);

        win.close();
      }
    }

    // Make sure the window is raised before each test.
    this.SimpleTest.waitForFocus(aCallback);
  },

  finish: function Tester_finish(aSkipSummary) {
    this.Promise.Debugging.flushUncaughtErrors();

    var passCount = this.tests.reduce(function(a, f) a + f.passCount, 0);
    var failCount = this.tests.reduce(function(a, f) a + f.failCount, 0);
    var todoCount = this.tests.reduce(function(a, f) a + f.todoCount, 0);

    if (this.repeat > 0) {
      --this.repeat;
      this.currentTestIndex = -1;
      this.nextTest();
    }
    else{
      Services.console.unregisterListener(this);
      Services.obs.removeObserver(this, "chrome-document-global-created");
      Services.obs.removeObserver(this, "content-document-global-created");
      this.Promise.Debugging.clearUncaughtErrorObservers();
      this._treatUncaughtRejectionsAsFailures = false;
      this.dumper.structuredLogger.info("TEST-START | Shutdown");

      if (this.tests.length) {
        this.dumper.structuredLogger.info("Browser Chrome Test Summary");
        this.dumper.structuredLogger.info("Passed:  " + passCount);
        this.dumper.structuredLogger.info("Failed:  " + failCount);
        this.dumper.structuredLogger.info("Todo:    " + todoCount);
      } else {
        this.dumper.structuredLogger.testEnd("browser-test.js",
                                             "FAIL",
                                             "PASS",
                                             "No tests to run. Did you pass an invalid --test-path?");
      }
      this.dumper.structuredLogger.info("*** End BrowserChrome Test Results ***");

      this.dumper.done();

      // Tests complete, notify the callback and return
      this.callback(this.tests);
      this.callback = null;
      this.tests = null;
      this.openedWindows = null;
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
    } else if (this.currentTest) {
      this.onDocumentCreated(aSubject);
    }
  },

  onDocumentCreated: function Tester_onDocumentCreated(aWindow) {
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    let outerID = utils.outerWindowID;
    let innerID = utils.currentInnerWindowID;

    if (!(outerID in this.openedWindows)) {
      this.openedWindows[outerID] = this.currentTest;
    }
    this.openedWindows[innerID] = this.currentTest;

    let url = aWindow.location.href || "about:blank";
    this.openedURLs[outerID] = this.openedURLs[innerID] = url;
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
        this.dumper.dump("TEST-INFO | (browser-test.js) | " + msg.replace(/\n$/, "") + "\n");
    } catch (ex) {
      // Swallow exception so we don't lead to another error being reported,
      // throwing us into an infinite loop
    }
  },

  nextTest: Task.async(function*() {
    if (this.currentTest) {
      this.Promise.Debugging.flushUncaughtErrors();

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
      };

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
            this.currentTest.addResult(new testResult(false, "leaked window property: " + prop, "", false));
        }
      }, this);

      // Clear document.popupNode.  The test could have set it to a custom value
      // for its own purposes, nulling it out it will go back to the default
      // behavior of returning the last opened popup.
      document.popupNode = null;

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
        this.MemoryStats.dump(this.dumper.structuredLogger,
                              this.currentTestIndex,
                              this.currentTest.path,
                              gConfig.dumpOutputDirectory,
                              gConfig.dumpAboutMemoryAfterTest,
                              gConfig.dumpDMDAfterTest);
      }

      // Note the test run time
      let time = Date.now() - this.lastStartTime;
      this.dumper.structuredLogger.testEnd(this.currentTest.path,
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

      testScope.destroy();
      this.currentTest.scope = null;
    }

    // Check the window state for the current test before moving to the next one.
    // This also causes us to check before starting any tests, since nextTest()
    // is invoked to start the tests.
    this.waitForWindowsState((function () {
      if (this.done) {

        // Uninitialize a few things explicitly so that they can clean up
        // frames and browser intentionally kept alive until shutdown to
        // eliminate false positives.
        if (gConfig.testRoot == "browser") {
          // Replace the document currently loaded in the browser's sidebar.
          // This will prevent false positives for tests that were the last
          // to touch the sidebar. They will thus not be blamed for leaking
          // a document.
          let sidebar = document.getElementById("sidebar");
          sidebar.setAttribute("src", "data:text/html;charset=utf-8,");
          sidebar.docShell.createAboutBlankContentViewer(null);
          sidebar.setAttribute("src", "about:blank");

          // Do the same for the social sidebar.
          let socialSidebar = document.getElementById("social-sidebar-browser");
          socialSidebar.setAttribute("src", "data:text/html;charset=utf-8,");
          socialSidebar.docShell.createAboutBlankContentViewer(null);
          socialSidebar.setAttribute("src", "about:blank");

          // Destroy BackgroundPageThumbs resources.
          let {BackgroundPageThumbs} =
            Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm", {});
          BackgroundPageThumbs._destroy();

          BrowserNewTabPreloader.uninit();
          CustomizationTabPreloader.uninit();
          SocialFlyout.unload();
          SocialShare.uninit();
          TabView.uninit();
        }

        // Schedule GC and CC runs before finishing in order to detect
        // DOM windows leaked by our tests or the tested code. Note that we
        // use a shrinking GC so that the JS engine will discard JIT code and
        // JIT caches more aggressively.

        let checkForLeakedGlobalWindows = aCallback => {
          Cu.schedulePreciseShrinkingGC(() => {
            let analyzer = new CCAnalyzer();
            analyzer.run(() => {
              let results = [];
              for (let obj of analyzer.find("nsGlobalWindow ")) {
                let m = obj.name.match(/^nsGlobalWindow #(\d+)/);
                if (m && m[1] in this.openedWindows)
                  results.push({ name: obj.name, url: m[1] });
              }
              aCallback(results);
            });
          });
        };

        let reportLeaks = aResults => {
          for (let result of aResults) {
            let test = this.openedWindows[result.url];
            let msg = "leaked until shutdown [" + result.name +
                      " " + (this.openedURLs[result.url] || "NULL") + "]";
            test.addResult(new testResult(false, msg, "", false));
          }
        };

        let {AsyncShutdown} =
          Cu.import("resource://gre/modules/AsyncShutdown.jsm", {});

        let barrier = new AsyncShutdown.Barrier(
          "ShutdownLeaks: Wait for cleanup to be finished before checking for leaks");
        Services.obs.notifyObservers({wrappedJSObject: barrier},
          "shutdown-leaks-before-check", null);

        barrier.wait().then(() => {
          // Simulate memory pressure so that we're forced to free more resources
          // and thus get rid of more false leaks like already terminated workers.
          Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

          checkForLeakedGlobalWindows(aResults => {
            if (aResults.length == 0) {
              this.finish();
              return;
            }
            // After the first check, if there are reported leaked windows, sleep
            // for a while, to allow off-main-thread work to complete and free up
            // main-thread objects.  Then check again.
            setTimeout(() => {
              checkForLeakedGlobalWindows(aResults => {
                reportLeaks(aResults);
                this.finish();
              });
            }, 1000);
          });
        });

        return;
      }

      this.currentTestIndex++;
      this.execTest();
    }).bind(this));
  }),

  execTest: function Tester_execTest() {
    this.dumper.structuredLogger.testStart(this.currentTest.path);

    this.SimpleTest.reset();

    // Load the tests into a testscope
    let currentScope = this.currentTest.scope = new testScope(this, this.currentTest);
    let currentTest = this.currentTest;

    // Import utils in the test scope.
    this.currentTest.scope.EventUtils = this.EventUtils;
    this.currentTest.scope.SimpleTest = this.SimpleTest;
    this.currentTest.scope.gTestPath = this.currentTest.path;
    this.currentTest.scope.Task = this.Task;
    this.currentTest.scope.Promise = this.Promise;
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
      if (ex.toString() != 'Error opening input stream (invalid filename?)') {
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
        this.Task.spawn(function() {
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
            this.Promise.Debugging.flushUncaughtErrors();
            this.SimpleTest.info("Leaving test " + task.name);
          }
          this.finish();
        }.bind(currentScope));
      } else if ("generatorTest" in this.currentTest.scope) {
        if ("test" in this.currentTest.scope) {
          throw "Cannot run both a generator test and a normal test at the same time.";
        }

        // This test is a generator. It will not finish immediately.
        this.currentTest.scope.waitForExplicitFinish();
        var result = this.currentTest.scope.generatorTest();
        this.currentTest.scope.__generator = result;
        result.next();
      } else {
        this.currentTest.scope.test();
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
      this.currentTest.scope.__waitTimer = setTimeout(function timeoutFn() {
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
      }, gTimeoutSeconds * 1000);
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
function testScope(aTester, aTest) {
  this.__tester = aTester;

  var self = this;
  this.ok = function test_ok(condition, name, diag, stack) {
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
  this.ise = function test_ise(a, b, name) {
    self.ok(a === b, name, "Got " + a + ", strictly expected " + b, false,
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
    Services.tm.mainThread.dispatch({
      run: function() {
        func();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  };

  this.nextStep = function test_nextStep(arg) {
    if (self.__done) {
      aTest.addResult(new testResult(false, "nextStep was called too many times", "", false));
      return;
    }

    if (!self.__generator) {
      aTest.addResult(new testResult(false, "nextStep called with no generator", "", false));
      self.finish();
      return;
    }

    try {
      self.__generator.send(arg);
    } catch (ex if ex instanceof StopIteration) {
      // StopIteration means test is finished.
      self.finish();
    } catch (ex) {
      var isExpected = !!self.SimpleTest.isExpectingUncaughtException();
      if (!self.SimpleTest.isIgnoringAllUncaughtExceptions()) {
        aTest.addResult(new testResult(isExpected, "Exception thrown", ex, false));
        self.SimpleTest.expectUncaughtException(false);
      } else {
        aTest.addResult(new testMessage("Exception thrown: " + ex));
      }
      self.finish();
    }
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
    self.__tester.dumper.structuredLogger.deactivateBuffering();
    self.registerCleanupFunction(function() {
      self.__tester.dumper.structuredLogger.activateBuffering();
    })
  };
}
testScope.prototype = {
  __done: true,
  __generator: null,
  __tasks: null,
  __waitTimer: null,
  __cleanupFunctions: [],
  __timeoutFactor: 1,
  __expectedMinAsserts: 0,
  __expectedMaxAsserts: 0,

  EventUtils: {},
  SimpleTest: {},
  Task: null,
  Promise: null,
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
