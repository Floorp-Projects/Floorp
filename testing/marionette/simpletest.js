/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["simpletest"];

this.simpletest = {};

/**
 * The simpletest harness, exposed in the script evaluation sandbox.
 */
simpletest.Harness = class {
  constructor(window, context, contentLogger, timeout, testName) {
    this.window = window;
    this.tests = [];
    this.logger = contentLogger;
    this.context = context;
    this.timeout = timeout;
    this.testName = testName;
    this.TEST_UNEXPECTED_FAIL = "TEST-UNEXPECTED-FAIL";
    this.TEST_UNEXPECTED_PASS = "TEST-UNEXPECTED-PASS";
    this.TEST_PASS = "TEST-PASS";
    this.TEST_KNOWN_FAIL = "TEST-KNOWN-FAIL";
  }

  get exports() {
    return new Map([
      ["ok", this.ok.bind(this)],
      ["is", this.is.bind(this)],
      ["isnot", this.isnot.bind(this)],
      ["todo", this.todo.bind(this)],
      ["log", this.log.bind(this)],
      ["getLogs", this.getLogs.bind(this)],
      ["generate_results", this.generate_results.bind(this)],
      ["waitFor", this.waitFor.bind(this)],
      ["TEST_PASS", this.TEST_PASS],
      ["TEST_KNOWN_FAIL", this.TEST_KNOWN_FAIL],
      ["TEST_UNEXPECTED_FAIL", this.TEST_UNEXPECTED_FAIL],
      ["TEST_UNEXPECTED_PASS", this.TEST_UNEXPECTED_PASS],
    ]);
  }

  addTest(condition, name, passString, failString, diag, state) {
    let test = {
      result: !!condition,
      name: name,
      diag: diag,
      state: state
    };
    this.logResult(
        test,
        typeof passString == "undefined" ? this.TEST_PASS : passString,
        typeof failString == "undefined" ? this.TEST_UNEXPECTED_FAIL : failString);
    this.tests.push(test);
  }

  ok(condition, name, passString, failString) {
    let diag = `${this.repr(condition)} was ${!!condition}, expected true`;
    this.addTest(condition, name, passString, failString, diag);
  }

  is(a, b, name, passString, failString) {
    let pass = (a == b);
    let diag = pass ? this.repr(a) + " should equal " + this.repr(b)
                    : "got " + this.repr(a) + ", expected " + this.repr(b);
    this.addTest(pass, name, passString, failString, diag);
  }

  isnot(a, b, name, passString, failString) {
    let pass = (a != b);
    let diag = pass ? this.repr(a) + " should not equal " + this.repr(b)
                    : "didn't expect " + this.repr(a) + ", but got it";
    this.addTest(pass, name, passString, failString, diag);
  }

  todo(condition, name, passString, failString) {
    let diag = this.repr(condition) + " was expected false";
    this.addTest(!condition,
                 name,
                 typeof(passString) == "undefined" ? this.TEST_KNOWN_FAIL : passString,
                 typeof(failString) == "undefined" ? this.TEST_UNEXPECTED_FAIL : failString,
                 diag,
                 "todo");
  }

  log(msg, level) {
    dump("MARIONETTE LOG: " + (level ? level : "INFO") + ": " + msg + "\n");
    if (this.logger) {
      this.logger.log(msg, level);
    }
  }

  // TODO(ato): Suspect this isn't used anywhere
  getLogs() {
    if (this.logger) {
      return this.logger.get();
    }
  }

  generate_results() {
    let passed = 0;
    let failures = [];
    let expectedFailures = [];
    let unexpectedSuccesses = [];
    for (let i in this.tests) {
      let isTodo = (this.tests[i].state == "todo");
      if(this.tests[i].result) {
        if (isTodo) {
          expectedFailures.push({'name': this.tests[i].name, 'diag': this.tests[i].diag});
        }
        else {
          passed++;
        }
      }
      else {
        if (isTodo) {
          unexpectedSuccesses.push({'name': this.tests[i].name, 'diag': this.tests[i].diag});
        }
        else {
          failures.push({'name': this.tests[i].name, 'diag': this.tests[i].diag});
        }
      }
    }
    // Reset state in case this object is reused for more tests.
    this.tests = [];
    return {
      passed: passed,
      failures: failures,
      expectedFailures: expectedFailures,
      unexpectedSuccesses: unexpectedSuccesses,
    };
  }

  logToFile(file) {
    //TODO
  }

  logResult(test, passString, failString) {
    //TODO: dump to file
    let resultString = test.result ? passString : failString;
    let diagnostic = test.name + (test.diag ? " - " + test.diag : "");
    let msg = resultString + " | " + this.testName + " | " + diagnostic;
    dump("MARIONETTE TEST RESULT:" + msg + "\n");
  }

  repr(o) {
    if (typeof o == "undefined") {
      return "undefined";
    } else if (o === null) {
      return "null";
    }

    try {
        if (typeof o.__repr__ == "function") {
          return o.__repr__();
        } else if (typeof o.repr == "function" && o.repr !== arguments.callee) {
          return o.repr();
        }
   } catch (e) {}

   try {
      if (typeof o.NAME === "string" &&
          (o.toString === Function.prototype.toString || o.toString === Object.prototype.toString)) {
        return o.NAME;
      }
    } catch (e) {}

    let ostring;
    try {
      ostring = (o + "");
    } catch (e) {
      return "[" + typeof(o) + "]";
    }

    if (typeof o == "function") {
      o = ostring.replace(/^\s+/, "");
      let idx = o.indexOf("{");
      if (idx != -1) {
        o = o.substr(0, idx) + "{...}";
      }
    }
    return ostring;
  }

  waitFor(callback, test, timeout) {
    if (test()) {
      callback();
      return;
    }

    let now = new Date();
    let deadline = (timeout instanceof Date) ? timeout :
        new Date(now.valueOf() + (typeof timeout == "undefined" ? this.timeout : timeout));
    if (deadline <= now) {
      dump("waitFor timeout: " + test.toString() + "\n");
      // the script will timeout here, so no need to raise a separate
      // timeout exception
      return;
    }
    this.window.setTimeout(this.waitFor.bind(this), 100, callback, test, deadline);
  }
};
