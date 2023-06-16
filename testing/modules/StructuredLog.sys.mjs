/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TestLogger: Logger class generating messages compliant with the
 * structured logging protocol for tests exposed by mozlog
 *
 * @param {string} name
 *        The name of the logger to instantiate.
 * @param {function} [dumpFun]
 *        An underlying function to be used to log raw messages. This function
 *        will receive the complete serialized json string to log.
 * @param {object} [scope]
 *        The scope that the dumpFun is loaded in, so that messages are cloned
 *        into that scope before passing them.
 */
export class StructuredLogger {
  name = null;
  #dumpFun = null;
  #dumpScope = null;

  constructor(name, dumpFun = dump, scope = null) {
    this.name = name;
    this.#dumpFun = dumpFun;
    this.#dumpScope = scope;
  }

  testStart(test) {
    var data = { test: this.#testId(test) };
    this.logData("test_start", data);
  }

  testStatus(
    test,
    subtest,
    status,
    expected = "PASS",
    message = null,
    stack = null,
    extra = null
  ) {
    if (subtest === null || subtest === undefined) {
      // Fix for assertions that don't pass in a name
      subtest = "undefined assertion name";
    }

    var data = {
      test: this.#testId(test),
      subtest,
      status,
    };

    // handle case: known fail
    if (expected === status && status != "SKIP") {
      data.status = "PASS";
    }
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

    this.logData("test_status", data);
  }

  testEnd(
    test,
    status,
    expected = "OK",
    message = null,
    stack = null,
    extra = null
  ) {
    var data = { test: this.#testId(test), status };

    // handle case: known fail
    if (expected === status && status != "SKIP") {
      data.status = "OK";
    }
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

    this.logData("test_end", data);
  }

  assertionCount(test, count, minExpected = 0, maxExpected = 0) {
    var data = {
      test: this.#testId(test),
      min_expected: minExpected,
      max_expected: maxExpected,
      count,
    };

    this.logData("assertion_count", data);
  }

  suiteStart(
    ids,
    name = null,
    runinfo = null,
    versioninfo = null,
    deviceinfo = null,
    extra = null
  ) {
    Object.keys(ids).map(function (manifest) {
      ids[manifest] = ids[manifest].map(x => this.#testId(x));
    }, this);
    var data = { tests: ids };

    if (name !== null) {
      data.name = name;
    }

    if (runinfo !== null) {
      data.runinfo = runinfo;
    }

    if (versioninfo !== null) {
      data.versioninfo = versioninfo;
    }

    if (deviceinfo !== null) {
      data.deviceinfo = deviceinfo;
    }

    if (extra !== null) {
      data.extra = extra;
    }

    this.logData("suite_start", data);
  }

  suiteEnd(extra = null) {
    var data = {};

    if (extra !== null) {
      data.extra = extra;
    }

    this.logData("suite_end", data);
  }

  /**
   * Unstructured logging functions. The "extra" parameter can always by used to
   * log suite specific data. If a "stack" field is provided it is logged at the
   * top level of the data object for the benefit of mozlog's formatters.
   */
  log(level, message, extra = null) {
    var data = {
      level,
      message: String(message),
    };

    if (extra !== null) {
      data.extra = extra;
      if ("stack" in extra) {
        data.stack = extra.stack;
      }
    }

    this.logData("log", data);
  }

  debug(message, extra = null) {
    this.log("DEBUG", message, extra);
  }

  info(message, extra = null) {
    this.log("INFO", message, extra);
  }

  warning(message, extra = null) {
    this.log("WARNING", message, extra);
  }

  error(message, extra = null) {
    this.log("ERROR", message, extra);
  }

  critical(message, extra = null) {
    this.log("CRITICAL", message, extra);
  }

  processOutput(thread, message) {
    this.logData("process_output", {
      message,
      thread,
    });
  }

  logData(action, data = {}) {
    var allData = {
      action,
      time: Date.now(),
      thread: null,
      pid: null,
      source: this.name,
    };

    for (var field in data) {
      allData[field] = data[field];
    }

    if (this.#dumpScope) {
      this.#dumpFun(Cu.cloneInto(allData, this.#dumpScope));
    } else {
      this.#dumpFun(allData);
    }
  }

  #testId(test) {
    if (Array.isArray(test)) {
      return test.join(" ");
    }
    return test;
  }
}

/**
 * StructuredFormatter: Formatter class turning structured messages
 * into human-readable messages.
 */
export class StructuredFormatter {
  // The time at which the whole suite of tests started.
  #suiteStartTime = null;

  #testStartTimes = new Map();

  log(message) {
    return message.message;
  }

  suite_start(message) {
    this.#suiteStartTime = message.time;
    return "SUITE-START | Running " + message.tests.length + " tests";
  }

  test_start(message) {
    this.#testStartTimes.set(message.test, new Date().getTime());
    return "TEST-START | " + message.test;
  }

  test_status(message) {
    var statusInfo =
      message.test +
      " | " +
      message.subtest +
      (message.message ? " | " + message.message : "");
    if (message.expected) {
      return (
        "TEST-UNEXPECTED-" +
        message.status +
        " | " +
        statusInfo +
        " - expected: " +
        message.expected
      );
    }
    return "TEST-" + message.status + " | " + statusInfo;
  }

  test_end(message) {
    var startTime = this.#testStartTimes.get(message.test);
    this.#testStartTimes.delete(message.test);
    var statusInfo =
      message.test + (message.message ? " | " + String(message.message) : "");
    var result;
    if (message.expected) {
      result =
        "TEST-UNEXPECTED-" +
        message.status +
        " | " +
        statusInfo +
        " - expected: " +
        message.expected;
    } else {
      return "TEST-" + message.status + " | " + statusInfo;
    }
    result = result + " | took " + message.time - startTime + "ms";
    return result;
  }

  suite_end(message) {
    return "SUITE-END | took " + message.time - this.#suiteStartTime + "ms";
  }
}
