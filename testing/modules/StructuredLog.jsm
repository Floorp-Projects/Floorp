/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "StructuredLogger"
];

/**
 * TestLogger: Logger class generating messages compliant with the
 * structured logging protocol for tests exposed by the mozlog.structured
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
this.StructuredLogger = function (name, dumpFun=dump, mutators=[]) {
  this.name = name;
  this._dumpFun = dumpFun;
  this._mutatorFuns = mutators;
  this._runningTests = new Set();
}

/**
 * Log functions producing messages in the format specified by
 * mozlog.structured
 */
StructuredLogger.prototype.testStart = function (test) {
  this._runningTests.add(test);
  let data = {test: test};
  this._logData("test_start", data);
}

StructuredLogger.prototype.testStatus = function (test, subtest, status, expected="PASS",
                                                  message=null, stack=null, extra=null) {
  let data = {
    test: test,
    subtest: subtest,
    status: status,
  };

  if (expected != status && status != "SKIP") {
    data.expected = expected;
  }
  if (message !== null) {
    data.message = message;
  }
  if (stack !== null) {
    data.stack = stack;
  }
  if (extra !== null) {
    data.extra = extra;
  }

  this._logData("test_status", data);
}

StructuredLogger.prototype.testEnd = function (test, status, expected="OK", message=null,
                                               stack=null, extra=null) {
  let data = {test: test, status: status};

  if (expected != status && status != "SKIP") {
    data.expected = expected;
  }
  if (message !== null) {
    data.message = message;
  }
  if (stack !== null) {
    data.stack = stack;
  }
  if (extra !== null) {
    data.extra = extra;
  }

  if (!this._runningTests.has(test)) {
    this.error("Test \"" + test + "\" was ended more than once or never started. " +
               "Ended with this data: " + JSON.stringify(data));
    return;
  }

  this._runningTests.delete(test);
  this._logData("test_end", data);
}

StructuredLogger.prototype.suiteStart = function (tests, runinfo=null) {

  let data = {tests: tests};
  if (runinfo !== null) {
    data.runinfo = runinfo;
  }

  this._logData("suite_start", data);
};

StructuredLogger.prototype.suiteEnd = function () {
  this._logData("suite_end");
};

/**
 * Unstructured logging functions
 */
StructuredLogger.prototype.log = function (level, message, extra=null) {
  let data = {
    level: level,
    message: message,
  };

  if (extra !== null) {
    data.extra = extra;
  }

  this._logData("log", data);
}

StructuredLogger.prototype.debug = function (message, extra=null) {
  this.log("DEBUG", message, extra);
}

StructuredLogger.prototype.info = function (message, extra=null) {
  this.log("INFO", message, extra);
}

StructuredLogger.prototype.warning = function (message, extra=null) {
  this.log("WARNING", message, extra);
}

StructuredLogger.prototype.error = function (message, extra=null) {
  this.log("ERROR", message, extra);
}

StructuredLogger.prototype.critical = function (message, extra=null) {
  this.log("CRITICAL", message, extra);
}

StructuredLogger.prototype._logData = function (action, data={}) {
  let allData = {
    action: action,
    time: Date.now(),
    thread: null,
    pid: null,
    source: this.name
  };

  for (let field in data) {
    allData[field] = data[field];
  }

  for (let fun of this._mutatorFuns) {
    fun(allData);
  }

  this._dumpMessage(allData);
};

StructuredLogger.prototype._dumpMessage = function (message) {
  this._dumpFun(JSON.stringify(message));
}
