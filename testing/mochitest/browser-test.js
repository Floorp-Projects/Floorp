// Test timeout (seconds)
const TIMEOUT_SECONDS = 30;

if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
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

  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  var sstring = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
  sstring.data = location.search;
  ww.openWindow(window, "chrome://mochikit/content/browser-harness.xul", "browserTest",
                "chrome,centerscreen,dialog,resizable,titlebar,toolbar=no,width=800,height=600", sstring);
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
  // Avoid polluting this scope with packed.js contents.
  var simpleTestScope = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/MochiKit/packed.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},

  checker: null,
  currentTestIndex: -1,
  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },

  start: function Tester_start() {
    this.dumper.dump("*** Start BrowserChrome Test Results ***\n");
    this._cs.registerListener(this);

    if (this.tests.length)
      this.nextTest();
    else
      this.finish();
  },

  waitForWindowsState: function Tester_waitForWindowsState(aCallback) {
    this.dumper.dump("TEST-INFO | checking window state\n");
    let windowsEnum = this._wm.getEnumerator("navigator:browser");
    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      if (win != window && !win.closed) {
        let msg = "Found an unexpected browser window";
        if (this.currentTest) {
          msg += " at the end of test run";
          this.currentTest.addResult(new testResult(false, msg, "", false));
        }
        else
          this.dumper.dump("TEST-UNEXPECTED-FAIL | (browser-test.js) | " + msg + "\n");

        win.close();
      }
    }

    // Make sure the window is raised before each test.
    if (this._fm.activeWindow != window) {
      this.dumper.dump("TEST-INFO | (browser-test.js) | Waiting for window activation...\n");
      let self = this;
      window.addEventListener("activate", function () {
        window.removeEventListener("activate", arguments.callee, false);
        setTimeout(function () {
          aCallback.apply(self);
        }, 0);
      }, false);
      window.focus();
      return;
    }

    aCallback.apply(this);
  },

  finish: function Tester_finish(aSkipSummary) {
    this._cs.unregisterListener(this);

    if (this.tests.length) {
      this.dumper.dump("\nBrowser Chrome Test Summary\n");

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
  },

  observe: function Tester_observe(aConsoleMessage) {
    var msg = "Console message: " + aConsoleMessage.message;
    if (this.currentTest)
      this.currentTest.addResult(new testMessage(msg));
    else
      this.dumper.dump("TEST-INFO | (browser-test.js) | " + msg);
  },

  nextTest: function Tester_nextTest() {
    if (this.currentTest) {
      // Run cleanup functions for the current test before moving on to the
      // next one.
      let testScope = this.currentTest.scope;
      while (testScope.__cleanupFunctions.length > 0) {
        let func = testScope.__cleanupFunctions.shift();
        func.apply(testScope);
      };
    }

    // Check the window state for the current test before moving to the next one.
    // This also causes us to check before starting any tests, since nextTest()
    // is invoked to start the tests.
    this.waitForWindowsState(this.realNextTest);
  },

  realNextTest: function Test_realNextTest() {
    if (this.done) {
      this.finish();
      return;
    }

    this.currentTestIndex++;
    this.execTest();
  },

  execTest: function Tester_execTest() {
    this.dumper.dump("Running " + this.currentTest.path + "...\n");

    // Load the tests into a testscope
    this.currentTest.scope = new testScope(this, this.currentTest);

    // Import utils in the test scope.
    this.currentTest.scope.EventUtils = this.EventUtils;
    this.currentTest.scope.SimpleTest = this.SimpleTest;

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
      this.currentTest.scope.test();
    } catch (ex) {
      this.currentTest.addResult(new testResult(false, "Exception thrown", ex, false));
      this.currentTest.scope.__done = true;
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
        self.currentTest.addResult(new testResult(false, "Timed out", "", false));
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

  this.waitForExplicitFinish = function test_waitForExplicitFinish() {
    self.__done = false;
  };

  this.waitForFocus = function test_waitForFocus(callback, targetWindow) {
    self.SimpleTest.waitForFocus(callback, targetWindow);
  };

  this.registerCleanupFunction = function test_registerCleanupFunction(aFunction) {
    self.__cleanupFunctions.push(aFunction);
  };

  this.requestLongerTimeout = function test_requestLongerTimeout(aFactor) {
    self.__timeoutFactor = aFactor;
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
  __waitTimer: null,
  __cleanupFunctions: [],
  __timeoutFactor: 1,

  EventUtils: {},
  SimpleTest: {}
};
