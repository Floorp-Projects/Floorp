// Test timeout (seconds)
var gTimeoutSeconds = 30;
var gConfig;

if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var Cu = Components.utils;
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

window.addEventListener("load", testOnLoad, false);

function testOnLoad() {
  window.removeEventListener("load", testOnLoad, false);

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
    function messageHandler(m) {
      messageManager.removeMessageListener("chromeEvent", messageHandler);
      var url = m.json.data;

      // Window is the [ChromeWindow] for messageManager, so we need content.window 
      // Currently chrome tests are run in a content window instead of a ChromeWindow
      var webNav = content.window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation);
      webNav.loadURI(url, null, null, null, null);
    }

    var listener = 'data:,function doLoad(e) { var data=e.getData("data");removeEventListener("contentEvent", function (e) { doLoad(e); }, false, true);sendAsyncMessage("chromeEvent", {"data":data}); };addEventListener("contentEvent", function (e) { doLoad(e); }, false, true);';
    messageManager.loadFrameScript(listener, true);
    messageManager.addMessageListener("chromeEvent", messageHandler);
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
  this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},

  repeat: 0,
  checker: null,
  currentTestIndex: -1,
  lastStartTime: null,
  openedWindows: null,

  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },

  start: function Tester_start() {
    // Check whether this window is ready to run tests.
    if (window.BrowserChromeTest) {
      BrowserChromeTest.runWhenReady(this.actuallyStart.bind(this));
      return;
    }
    this.actuallyStart();
  },

  actuallyStart: function Tester_actuallyStart() {
    //if testOnLoad was not called, then gConfig is not defined
    if (!gConfig)
      gConfig = readConfig();
    this.repeat = gConfig.repeat;
    this.dumper.dump("*** Start BrowserChrome Test Results ***\n");
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

    if (this.currentTest && window.gBrowser && gBrowser.tabs.length > 1) {
      while (gBrowser.tabs.length > 1) {
        let lastTab = gBrowser.tabContainer.lastChild;
        let msg = baseMsg.replace("{elt}", "tab") +
                  ": " + lastTab.linkedBrowser.currentURI.spec;
        this.currentTest.addResult(new testResult(false, msg, "", false));
        gBrowser.removeTab(lastTab);
      }
    }

    this.dumper.dump("TEST-INFO | checking window state\n");
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
          this.dumper.dump("TEST-UNEXPECTED-FAIL | (browser-test.js) | " + msg + "\n");

        win.close();
      }
    }

    // Make sure the window is raised before each test.
    this.SimpleTest.waitForFocus(aCallback);
  },

  finish: function Tester_finish(aSkipSummary) {
    if (this.repeat > 0) {
      --this.repeat;
      this.currentTestIndex = -1;
      this.nextTest();
    }
    else{
      Services.console.unregisterListener(this);
      Services.obs.removeObserver(this, "chrome-document-global-created");
      Services.obs.removeObserver(this, "content-document-global-created");
  
      this.dumper.dump("\nINFO TEST-START | Shutdown\n");
      if (this.tests.length) {
        this.dumper.dump("Browser Chrome Test Summary\n");
  
        function sum(a,b) a+b;
        var passCount = this.tests.map(function (f) f.passCount).reduce(sum);
        var failCount = this.tests.map(function (f) f.failCount).reduce(sum);
        var todoCount = this.tests.map(function (f) f.todoCount).reduce(sum);
  
        this.dumper.dump("\tPassed: " + passCount + "\n" +
                         "\tFailed: " + failCount + "\n" +
                         "\tTodo: " + todoCount + "\n");
      } else {
        this.dumper.dump("TEST-UNEXPECTED-FAIL | (browser-test.js) | " +
                         "No tests to run. Did you pass an invalid --test-path?\n");
      }
  
      this.dumper.dump("\n*** End BrowserChrome Test Results ***\n");
  
      this.dumper.done();
  
      // Tests complete, notify the callback and return
      this.callback(this.tests);
      this.callback = null;
      this.tests = null;
      this.openedWindows = null;
    }
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

  nextTest: function Tester_nextTest() {
    if (this.currentTest) {
      // Run cleanup functions for the current test before moving on to the
      // next one.
      let testScope = this.currentTest.scope;
      while (testScope.__cleanupFunctions.length > 0) {
        let func = testScope.__cleanupFunctions.shift();
        try {
          func.apply(testScope);
        }
        catch (ex) {
          this.currentTest.addResult(new testResult(false, "Cleanup function threw an exception", ex, false));
        }
      };

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

      // Note the test run time
      let time = Date.now() - this.lastStartTime;
      this.dumper.dump("INFO TEST-END | " + this.currentTest.path + " | finished in " + time + "ms\n");
      this.currentTest.setDuration(time);

      testScope.destroy();
      this.currentTest.scope = null;
    }

    // Check the window state for the current test before moving to the next one.
    // This also causes us to check before starting any tests, since nextTest()
    // is invoked to start the tests.
    this.waitForWindowsState((function () {
      if (this.done) {
        // Many tests randomly add and remove tabs, resulting in the original
        // tab being replaced by a new one. The last test in the suite doing this
        // will erroneously be blamed for leaking this new tab's DOM window and
        // docshell until shutdown. We can prevent this by removing this tab now
        // that all tests are done.
        if (window.gBrowser) {
          gBrowser.addTab();
          gBrowser.removeCurrentTab();
        }

        // Schedule GC and CC runs before finishing in order to detect
        // DOM windows leaked by our tests or the tested code.
        Cu.schedulePreciseGC((function () {
          let analyzer = new CCAnalyzer();
          analyzer.run(function () {
            for (let obj of analyzer.find("nsGlobalWindow ")) {
              let m = obj.name.match(/^nsGlobalWindow #(\d+)/);
              if (m && m[1] in this.openedWindows) {
                let test = this.openedWindows[m[1]];
                let msg = "leaked until shutdown [" + obj.name +
                          " " + (this.openedURLs[m[1]] || "NULL") + "]";
                test.addResult(new testResult(false, msg, "", false));
              }
            }

            this.finish();
          }.bind(this));
        }).bind(this));
        return;
      }

      this.currentTestIndex++;
      this.execTest();
    }).bind(this));
  },

  execTest: function Tester_execTest() {
    this.dumper.dump("TEST-START | " + this.currentTest.path + "\n");

    this.SimpleTest.reset();

    // Load the tests into a testscope
    this.currentTest.scope = new testScope(this, this.currentTest);

    // Import utils in the test scope.
    this.currentTest.scope.EventUtils = this.EventUtils;
    this.currentTest.scope.SimpleTest = this.SimpleTest;
    this.currentTest.scope.gTestPath = this.currentTest.path;

    // Override SimpleTest methods with ours.
    ["ok", "is", "isnot", "ise", "todo", "todo_is", "todo_isnot", "info"].forEach(function(m) {
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

      // Run the test
      this.lastStartTime = Date.now();
      if ("generatorTest" in this.currentTest.scope) {
        if ("test" in this.currentTest.scope)
          throw "Cannot run both a generator test and a normal test at the same time.";

        // This test is a generator. It will not finish immediately.
        this.currentTest.scope.waitForExplicitFinish();
        var result = this.currentTest.scope.generatorTest();
        this.currentTest.scope.__generator = result;
        result.next();
      } else {
        this.currentTest.scope.test();
      }
    } catch (ex) {
      var isExpected = !!this.SimpleTest.isExpectingUncaughtException();
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
      this.currentTest.scope.__waitTimer = setTimeout(function() {
        if (--self.currentTest.scope.__timeoutFactor > 0) {
          // We were asked to wait a bit longer.
          self.currentTest.scope.info(
            "Longer timeout required, waiting longer...  Remaining timeouts: " +
            self.currentTest.scope.__timeoutFactor);
          self.currentTest.scope.__waitTimer =
            setTimeout(arguments.callee, gTimeoutSeconds * 1000);
          return;
        }
        self.currentTest.addResult(new testResult(false, "Test timed out", "", false));
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
  this.msg = aName || "";

  this.info = false;
  this.pass = !!aCondition;
  this.todo = aIsTodo;

  if (this.pass) {
    if (aIsTodo)
      this.result = "TEST-KNOWN-FAIL";
    else
      this.result = "TEST-PASS";
  } else {
    if (aDiag) {
      if (typeof aDiag == "object" && "fileName" in aDiag) {
        // we have an exception - print filename and linenumber information
        this.msg += " at " + aDiag.fileName + ":" + aDiag.lineNumber;
      }
      this.msg += " - " + aDiag;
    }
    if (aStack) {
      this.msg += "\nStack trace:\n";
      var frame = aStack;
      while (frame) {
        this.msg += "    " + frame + "\n";
        frame = frame.caller;
      }
    }
    if (aIsTodo)
      this.result = "TEST-UNEXPECTED-PASS";
    else
      this.result = "TEST-UNEXPECTED-FAIL";
  }
}

function testMessage(aName) {
  this.msg = aName || "";
  this.info = true;
  this.result = "TEST-INFO";
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
}
testScope.prototype = {
  __done: true,
  __generator: null,
  __waitTimer: null,
  __cleanupFunctions: [],
  __timeoutFactor: 1,

  EventUtils: {},
  SimpleTest: {},

  destroy: function test_destroy() {
    for (let prop in this)
      delete this[prop];
  }
};
