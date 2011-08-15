// Test timeout (seconds)
const TIMEOUT_SECONDS = 30;
var gConfig;

if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var Cu = Components.utils;
}
window.addEventListener("load", testOnLoad, false);

function testOnLoad() {
  window.removeEventListener("load", testOnLoad, false);

  // Make sure to launch the test harness for the first opened window only
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  if (prefs.prefHasUserValue("testing.browserTestHarness.running"))
    return;

  prefs.setBoolPref("testing.browserTestHarness.running", true);
  gConfig = readConfig();

  if (gConfig.testRoot == "browser") {
    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    var sstring = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
    sstring.data = location.search;

    ww.openWindow(window, "chrome://mochikit/content/browser-harness.xul", "browserTest",
                  "chrome,centerscreen,dialog=no,resizable,titlebar,toolbar=no,width=800,height=600", sstring);
  }
}

function Tester(aTests, aDumper, aCallback) {
  this.dumper = aDumper;
  this.tests = aTests;
  this.callback = aCallback;
  this._cs = Cc["@mozilla.org/consoleservice;1"].
             getService(Ci.nsIConsoleService);
  this._wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
  this._fm = Cc["@mozilla.org/focus-manager;1"].
             getService(Ci.nsIFocusManager);

  this._scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                       getService(Ci.mozIJSSubScriptLoader);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.EventUtils);
  var simpleTestScope = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},

  loops: 0,
  checker: null,
  currentTestIndex: -1,
  lastStartTime: null,
  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },

  start: function Tester_start() {
    //if testOnLoad was not called, then gConfig is not defined
    if(!gConfig)
      gConfig = readConfig();
    this.loops = gConfig.loops;
    this.dumper.dump("*** Start BrowserChrome Test Results ***\n");
    this._cs.registerListener(this);

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
    let windowsEnum = this._wm.getEnumerator(null);
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
    if(this.loops > 0){
      --this.loops;
      this.currentTestIndex = -1;
      this.nextTest();
    }
    else{
      this._cs.unregisterListener(this);
  
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
                         "No tests to run. Did you pass an invalid --test-path?");
      }
  
      this.dumper.dump("\n*** End BrowserChrome Test Results ***\n");
  
      this.dumper.done();
  
      // Tests complete, notify the callback and return
      this.callback(this.tests);
      this.callback = null;
      this.tests = null;
    }
  },

  observe: function Tester_observe(aConsoleMessage) {
    try {
      var msg = "Console message: " + aConsoleMessage.message;
      if (this.currentTest)
        this.currentTest.addResult(new testMessage(msg));
      else
        this.dumper.dump("TEST-INFO | (browser-test.js) | " + msg);
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
        // Schedule GC and CC runs before finishing in order to detect
        // DOM windows leaked by our tests or the tested code.
        Cu.schedulePreciseGC((function () {
          let winutils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDOMWindowUtils);
          winutils.garbageCollect();
          winutils.garbageCollect();
          winutils.garbageCollect();
          this.finish();
        }).bind(this));
        return;
      }

      this.currentTestIndex++;
      this.execTest();
    }).bind(this));
  },

  execTest: function Tester_execTest() {
    this.dumper.dump("TEST-START | " + this.currentTest.path + "\n");

    // Load the tests into a testscope
    this.currentTest.scope = new testScope(this, this.currentTest);

    // Import utils in the test scope.
    this.currentTest.scope.EventUtils = this.EventUtils;
    this.currentTest.scope.SimpleTest = this.SimpleTest;
    this.currentTest.scope.gTestPath = this.currentTest.path;

    // Override SimpleTest methods with ours.
    ["ok", "is", "isnot", "todo", "todo_is", "todo_isnot"].forEach(function(m) {
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
    } catch (ex) { /* no head */ }

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
      this.currentTest.addResult(new testResult(false, "Exception thrown", ex, false));
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
            setTimeout(arguments.callee, TIMEOUT_SECONDS * 1000);
          return;
        }
        self.currentTest.addResult(new testResult(false, "Test timed out", "", false));
        self.currentTest.timedOut = true;
        self.currentTest.scope.__waitTimer = null;
        self.nextTest();
      }, TIMEOUT_SECONDS * 1000);
    }
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIConsoleListener) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function testResult(aCondition, aName, aDiag, aIsTodo) {
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
  this.__browserTest = aTest;

  var self = this;
  this.ok = function test_ok(condition, name, diag) {
    self.__browserTest.addResult(new testResult(condition, name, diag, false));
  };
  this.is = function test_is(a, b, name) {
    self.ok(a == b, name, "Got " + a + ", expected " + b);
  };
  this.isnot = function test_isnot(a, b, name) {
    self.ok(a != b, name, "Didn't expect " + a + ", but got it");
  };
  this.todo = function test_todo(condition, name, diag) {
    self.__browserTest.addResult(new testResult(!condition, name, diag, true));
  };
  this.todo_is = function test_todo_is(a, b, name) {
    self.todo(a == b, name, "Got " + a + ", expected " + b);
  };
  this.todo_isnot = function test_todo_isnot(a, b, name) {
    self.todo(a != b, name, "Didn't expect " + a + ", but got it");
  };
  this.info = function test_info(name) {
    self.__browserTest.addResult(new testMessage(name));
  };

  this.executeSoon = function test_executeSoon(func) {
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

    tm.mainThread.dispatch({
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
      aTest.addResult(new testResult(false, "Exception thrown", ex, false));
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

  this.expectUncaughtException = function test_expectUncaughtException() {
    self.SimpleTest.expectUncaughtException();
  };

  this.finish = function test_finish() {
    self.__done = true;
    if (self.SimpleTest._expectingUncaughtException) {
      self.ok(false, "expectUncaughtException was called but no uncaught exception was detected!");
    }
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
