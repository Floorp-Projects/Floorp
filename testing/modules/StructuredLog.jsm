/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "StructuredLogger",
  "StructuredFormatter"
];

/**
 * TestLogger: Logger class generating messages compliant with the
 * structured logging protocol for tests exposed by mozlog
 *
 * @param name
 *        The name of the logger to instantiate.
 * @param dumpFun
 *        An underlying function to be used to log raw messages. This function
 *        will receive the complete serialized json string to log.
 * @param mutators
 *        An array of functions used to add global context to log messages.
 *        These will each be called with the complete object to log as an
 *        argument.
 */
this.StructuredLogger = function(name, dumpFun=dump, mutators=[]) {
  this.name = name;
  this._dumpFun = dumpFun;
  this._mutatorFuns = mutators;
}

/**
 * Log functions producing messages in the format specified by mozlog
 */
StructuredLogger.prototype = {
  testStart: function (test) {
    var data = {test: test};
    this._logData("test_start", data);
  },

  testStatus: function (test, subtest, status, expected="PASS",
                        message=null, stack=null, extra=null) {

    if (subtest === null || subtest === undefined) {
      // Fix for assertions that don't pass in a name
      subtest = "undefined assertion name";
    }

    var data = {
      test: test,
      subtest: subtest,
      status: status,
    };

    if (expected != status && status != "SKIP") {
      data.expected = expected;
    }
    if (message !== null) {
      data.message = String(message);
    }
    if (stack !== null) {
      data.stack = stack;
    }
    if (extra !== null) {
      data.extra = extra;
    }

    this._logData("test_status", data);
  },

  testEnd: function (test, status, expected="OK", message=null, stack=null, extra=null) {
    var data = {test: test, status: status};

    if (expected != status && status != "SKIP") {
      data.expected = expected;
    }
    if (message !== null) {
      data.message = String(message);
    }
    if (stack !== null) {
      data.stack = stack;
    }
    if (extra !== null) {
      data.extra = extra;
    }

    this._logData("test_end", data);
  },

  suiteStart: function (tests, runinfo=null) {
    var data = {tests: tests};
    if (runinfo !== null) {
      data.runinfo = runinfo;
    }

    this._logData("suite_start", data);
  },

  suiteEnd: function () {
    this._logData("suite_end");
  },


  /**
   * Unstructured logging functions. The "extra" parameter can always by used to
   * log suite specific data. If a "stack" field is provided it is logged at the
   * top level of the data object for the benefit of mozlog's formatters.
   */
  log: function (level, message, extra=null) {
    var data = {
      level: level,
      message: String(message),
    };

    if (extra !== null) {
      data.extra = extra;
      if ("stack" in extra) {
        data.stack = extra.stack;
      }
    }

    this._logData("log", data);
  },

  debug: function (message, extra=null) {
    this.log("DEBUG", message, extra);
  },

  info: function (message, extra=null) {
    this.log("INFO", message, extra);
  },

  warning: function (message, extra=null) {
    this.log("WARNING", message, extra);
  },

  error: function (message, extra=null) {
    this.log("ERROR", message, extra);
  },

  critical: function (message, extra=null) {
    this.log("CRITICAL", message, extra);
  },

  _logData: function (action, data={}) {
    var allData = {
      action: action,
      time: Date.now(),
      thread: null,
      pid: null,
      source: this.name
    };

    for (var field in data) {
      allData[field] = data[field];
    }

    for (var fun of this._mutatorFuns) {
      fun(allData);
    }

    this._dumpFun(allData);
  }
};


/**
 * StructuredFormatter: Formatter class turning structured messages
 * into human-readable messages.
 */
this.StructuredFormatter = function() {
    this.testStartTimes = {};
};

StructuredFormatter.prototype = {

  log: function(message) {
    return message.message;
  },

  suite_start: function(message) {
    this.suiteStartTime = message.time;
    return "SUITE-START | Running " +  message.tests.length + " tests";
  },

  test_start: function(message) {
    this.testStartTimes[message.test] = new Date().getTime();
    return "TEST-START | " + message.test;
  },

  test_status: function(message) {
    var statusInfo = message.test + " | " + message.subtest +
                    (message.message ? " | " + message.message : "");
    if (message.expected) {
        return "TEST-UNEXPECTED-" + message.status + " | " + statusInfo +
               " - expected: " + message.expected;
    } else {
        return "TEST-" + message.status + " | " + statusInfo;
    }
  },

  test_end: function(message) {
    var startTime = this.testStartTimes[message.test];
    delete this.testStartTimes[message.test];
    var statusInfo = message.test + (message.message ? " | " + String(message.message) : "");
    var result;
    if (message.expected) {
        result = "TEST-UNEXPECTED-" + message.status + " | " + statusInfo +
                 " - expected: " + message.expected;
    } else {
        return "TEST-" + message.status + " | " + statusInfo;
    }
    result = " | took " + message.time - startTime + "ms";
  },

  suite_end: function(message) {
    return "SUITE-END | took " + message.time - this.suiteStartTime + "ms";
  }
};

