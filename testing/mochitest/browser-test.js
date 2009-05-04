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

function Tester(aTests, aCallback) {
  this.tests = aTests;
  this.callback = aCallback;
}
Tester.prototype = {
  checker: null,
  currentTestIndex: -1,
  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return this.currentTestIndex == this.tests.length - 1;
  },
  step: function Tester_step() {
    this.currentTestIndex++;
  },

  start: function Tester_start() {
    if (this.tests.length)
      this.execTest();
    else
      this.finish();
  },

  finish: function Tester_finish() {
    // Tests complete, notify the callback and return
    this.callback(this.tests);
    this.callback = null;
    this.tests = null;
  },

  execTest: function Tester_execTest() {
    if (this.done) {
      this.finish();
      return;
    }

    // Move to the next test (or first test).
    this.step();

    // Load the tests into a testscope
    this.currentTest.scope = new testScope(this, this.currentTest.tests);

    var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                       getService(Ci.mozIJSSubScriptLoader);
    try {
      scriptLoader.loadSubScript(this.currentTest.path, this.currentTest.scope);

      // Run the test
      this.currentTest.scope.test();
    } catch (ex) {
      this.currentTest.tests.push(new testResult(false, "Exception thrown", ex, false));
      this.currentTest.scope.done = true;
    }

    // If the test ran synchronously, move to the next test, otherwise the test
    // will trigger the next test when it is done.
    if (this.currentTest.scope.done) {
      this.execTest();
    }
    else {
      var self = this;
      this.currentTest.scope.waitTimer = setTimeout(function() {
        self.currentTest.tests.push(new testResult(false, "Timed out", "", false));
        self.currentTest.scope.waitTimer = null;
        self.execTest();
      }, TIMEOUT_SECONDS * 1000);
    }
  }
};

function testResult(aCondition, aName, aDiag, aIsTodo) {
  aName = aName || "";

  this.pass = !!aCondition;
  this.todo = aIsTodo;
  this.msg = aName;
  if (this.pass) {
    if (aIsTodo)
      this.result = "TEST-KNOWN-FAIL";
    else
      this.result = "TEST-PASS";
  } else {
    if (aDiag)
      this.msg += " - " + aDiag;
    if (aIsTodo)
      this.result = "TEST-UNEXPECTED-PASS";
    else
      this.result = "TEST-UNEXPECTED-FAIL";
  }
}

function testScope(aTester, aTests) {
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.EventUtils);

  this.tester = aTester;
  this.tests = aTests;

  var self = this;
  this.ok = function test_ok(condition, name, diag) {
    self.tests.push(new testResult(condition, name, diag, false));
  };
  this.is = function test_is(a, b, name) {
    self.ok(a == b, name, "Got " + a + ", expected " + b);
  };
  this.isnot = function test_isnot(a, b, name) {
    self.ok(a != b, name, "Didn't expect " + a + ", but got it");
  };
  this.todo = function test_todo(condition, name, diag) {
    self.tests.push(new testResult(!condition, name, diag, true));
  };
  this.todo_is = function test_todo_is(a, b, name) {
    self.todo(a == b, name, "Got " + a + ", expected " + b);
  };
  this.todo_isnot = function test_todo_isnot(a, b, name) {
    self.todo(a != b, name, "Didn't expect " + a + ", but got it");
  };

  this.executeSoon = function test_executeSoon(func) {
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

    tm.mainThread.dispatch({
      run: function() {
        func();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  };

  this.waitForExplicitFinish = function test_WFEF() {
    self.done = false;
  };
  this.finish = function test_finish() {
    self.done = true;
    if (self.waitTimer) {
      self.executeSoon(function() {
        if (self.done && self.waitTimer) {
          clearTimeout(self.waitTimer);
          self.waitTimer = null;
          self.tester.execTest();
        }
      });
    }
  };
}
testScope.prototype = {
  done: true,
  waitTimer: null,

  EventUtils: {}
};
