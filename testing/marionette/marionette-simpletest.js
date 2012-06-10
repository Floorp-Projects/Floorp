/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * The Marionette object, passed to the script context.
 */

function Marionette(scope, window, context, logObj) {
  this.scope = scope;
  this.window = window;
  this.tests = [];
  this.logObj = logObj;
  this.context = context;
  this.timeout = 0;
}

Marionette.prototype = {
  exports: ['ok', 'is', 'isnot', 'log', 'getLogs', 'generate_results', 'waitFor',
            'runEmulatorCmd'],

  ok: function Marionette__ok(condition, name, diag) {
    let test = {'result': !!condition, 'name': name, 'diag': diag};
    this.logResult(test, "TEST-PASS", "TEST-UNEXPECTED-FAIL");
    this.tests.push(test);
  },

  is: function Marionette__is(a, b, name) {
    let pass = (a == b);
    let diag = pass ? this.repr(a) + " should equal " + this.repr(b)
                    : "got " + this.repr(a) + ", expected " + this.repr(b);
    this.ok(pass, name, diag);
  },

  isnot: function Marionette__isnot (a, b, name) {
    let pass = (a != b);
    let diag = pass ? this.repr(a) + " should not equal " + this.repr(b)
                    : "didn't expect " + this.repr(a) + ", but got it";
    this.ok(pass, name, diag);
  },

  log: function Marionette__log(msg, level) {
    dump("MARIONETTE LOG: " + (level ? level : "INFO") + ": " + msg + "\n");
    if (this.logObj != null) {
      this.logObj.log(msg, level);
    }
  },

  getLogs: function Marionette__getLogs() {
    if (this.logObj != null) {
      this.logObj.getLogs();
    }
  },

  generate_results: function Marionette__generate_results() {
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
    //TODO
  },

  logResult: function Marionette__logResult(test, passString, failString) {
    //TODO: dump to file
    let resultString = test.result ? passString : failString;
    let diagnostic = test.name + (test.diag ? " - " + test.diag : "");
    let msg = [resultString, diagnostic].join(" | ");
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
      if (test()) {
          callback();
          return;
      }
      timeout = timeout || Date.now();
      if (Date.now() - timeout > this.timeout) {
        dump("waitFor timeout: " + test.toString() + "\n");
        // the script will timeout here, so no need to raise a separate
        // timeout exception
        return;
      }
      this.window.setTimeout(this.waitFor.bind(this), 100, callback, test, timeout);
  },

  runEmulatorCmd: function runEmulatorCmd(cmd, callback) {
    this.scope.runEmulatorCmd(cmd, callback);
  },

};

