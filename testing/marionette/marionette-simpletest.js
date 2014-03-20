/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * The Marionette object, passed to the script context.
 */

this.Marionette = function Marionette(scope, window, context, logObj, timeout,
                                      heartbeatCallback, testName) {
  this.scope = scope;
  this.window = window;
  this.tests = [];
  this.logObj = logObj;
  this.context = context;
  this.timeout = timeout;
  this.heartbeatCallback = heartbeatCallback;
  this.testName = testName;
  this.TEST_UNEXPECTED_FAIL = "TEST-UNEXPECTED-FAIL";
  this.TEST_PASS = "TEST-PASS";
  this.TEST_KNOWN_FAIL = "TEST-KNOWN-FAIL";
}

Marionette.prototype = {
  exports: ['ok', 'is', 'isnot', 'log', 'getLogs', 'generate_results', 'waitFor',
            'runEmulatorCmd', 'runEmulatorShell', 'TEST_PASS', 'TEST_KNOWN_FAIL',
            'TEST_UNEXPECTED_FAIL'],

  ok: function Marionette__ok(condition, name, passString, failString, diag) {
    this.heartbeatCallback();
    if (typeof(diag) == "undefined") {
      diag = this.repr(condition) + " was " + !!condition + ", expected true";
    }
    let test = {'result': !!condition, 'name': name, 'diag': diag};
    this.logResult(test,
                   typeof(passString) == "undefined" ? this.TEST_PASS : passString,
                   typeof(failString) == "undefined" ? this.TEST_UNEXPECTED_FAIL : failString);
    this.tests.push(test);
  },

  is: function Marionette__is(a, b, name, passString, failString) {
    this.heartbeatCallback();
    let pass = (a == b);
    let diag = pass ? this.repr(a) + " should equal " + this.repr(b)
                    : "got " + this.repr(a) + ", expected " + this.repr(b);
    this.ok(pass, name, passString, failString, diag);
  },

  isnot: function Marionette__isnot (a, b, name, passString, failString) {
    this.heartbeatCallback();
    let pass = (a != b);
    let diag = pass ? this.repr(a) + " should not equal " + this.repr(b)
                    : "didn't expect " + this.repr(a) + ", but got it";
    this.ok(pass, name, passString, failString, diag);
  },

  log: function Marionette__log(msg, level) {
    this.heartbeatCallback();
    dump("MARIONETTE LOG: " + (level ? level : "INFO") + ": " + msg + "\n");
    if (this.logObj != null) {
      this.logObj.log(msg, level);
    }
  },

  getLogs: function Marionette__getLogs() {
    this.heartbeatCallback();
    if (this.logObj != null) {
      this.logObj.getLogs();
    }
  },

  generate_results: function Marionette__generate_results() {
    this.heartbeatCallback();
    let passed = 0;
    let failed = 0;
    let failures = [];
    for (let i in this.tests) {
      if(this.tests[i].result) {
        passed++;
      }
      else {
        failed++;
        failures.push({'name': this.tests[i].name,
                       'diag': this.tests[i].diag});
      }
    }
    // Reset state in case this object is reused for more tests.
    this.tests = [];
    return {"passed": passed, "failed": failed, "failures": failures};
  },

  logToFile: function Marionette__logToFile(file) {
    this.heartbeatCallback();
    //TODO
  },

  logResult: function Marionette__logResult(test, passString, failString) {
    this.heartbeatCallback();
    //TODO: dump to file
    let resultString = test.result ? passString : failString;
    let diagnostic = test.name + (test.diag ? " - " + test.diag : "");
    let msg = resultString + " | " + this.testName + " | " + diagnostic;
    dump("MARIONETTE TEST RESULT:" + msg + "\n");
  },

  repr: function Marionette__repr(o) {
      if (typeof(o) == "undefined") {
          return "undefined";
      } else if (o === null) {
          return "null";
      }
      try {
          if (typeof(o.__repr__) == 'function') {
              return o.__repr__();
          } else if (typeof(o.repr) == 'function' && o.repr != arguments.callee) {
              return o.repr();
          }
     } catch (e) {
     }
     try {
          if (typeof(o.NAME) == 'string' && (
                  o.toString == Function.prototype.toString ||
                  o.toString == Object.prototype.toString
              )) {
              return o.NAME;
          }
      } catch (e) {
      }
      let ostring;
      try {
          ostring = (o + "");
      } catch (e) {
          return "[" + typeof(o) + "]";
      }
      if (typeof(o) == "function") {
          o = ostring.replace(/^\s+/, "");
          let idx = o.indexOf("{");
          if (idx != -1) {
              o = o.substr(0, idx) + "{...}";
          }
      }
      return ostring;
  },

  waitFor: function test_waitFor(callback, test, timeout) {
      this.heartbeatCallback();
      if (test()) {
          callback();
          return;
      }
      var now = Date.now();
      var deadline = now + (typeof(timeout) == "undefined" ? this.timeout : timeout);
      if (deadline <= now) {
        dump("waitFor timeout: " + test.toString() + "\n");
        // the script will timeout here, so no need to raise a separate
        // timeout exception
        return;
      }
      this.window.setTimeout(this.waitFor.bind(this), 100, callback, test, deadline - now);
  },

  runEmulatorCmd: function runEmulatorCmd(cmd, callback) {
    this.heartbeatCallback();
    this.scope.runEmulatorCmd(cmd, callback);
  },

  runEmulatorShell: function runEmulatorShell(args, callback) {
    this.heartbeatCallback();
    this.scope.runEmulatorShell(args, callback);
  },

};

