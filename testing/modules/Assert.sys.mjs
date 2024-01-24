/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

import { ObjectUtils } from "resource://gre/modules/ObjectUtils.sys.mjs";

/**
 * This module is based on the
 * `CommonJS spec <https://wiki.commonjs.org/wiki/Unit_Testing/1.0>`_
 *
 * When you see a jsdoc comment that contains a number, it's a reference to a
 * specific section of the CommonJS spec.
 *
 * 1. The assert module provides functions that throw AssertionError's when
 * particular conditions are not met.
 *
 * To use the module you may instantiate it first.
 *
 * @param {reporterFunc} reporterFunc
 *        Allows consumers to override reporting for this instance.
 * @param {boolean} isDefault
 *        Used by test suites to set ``reporterFunc`` as the default
 *        used by the global instance, which is called for example
 *        by other test-only modules. This is false when the
 *        reporter is set by content scripts, because they may still
 *        run in the parent process.
 *
 * @class
 */
export function Assert(reporterFunc, isDefault) {
  if (reporterFunc) {
    this.setReporter(reporterFunc);
  }
  if (isDefault) {
    Assert.setReporter(reporterFunc);
  }
}

// This allows using the Assert object as an additional global instance.
Object.setPrototypeOf(Assert, Assert.prototype);

function instanceOf(object, type) {
  return Object.prototype.toString.call(object) == "[object " + type + "]";
}

function replacer(key, value) {
  if (value === undefined) {
    return "" + value;
  }
  if (typeof value === "number" && (isNaN(value) || !isFinite(value))) {
    return value.toString();
  }
  if (typeof value === "function" || instanceOf(value, "RegExp")) {
    return value.toString();
  }
  return value;
}

const kTruncateLength = 128;

function truncate(text, newLength = kTruncateLength) {
  if (typeof text == "string") {
    return text.length < newLength ? text : text.slice(0, newLength);
  }
  return text;
}

function getMessage(error, prefix = "") {
  let actual, expected;
  // Wrap calls to JSON.stringify in try...catch blocks, as they may throw. If
  // so, fall back to toString().
  try {
    if (
      typeof error.actual == "object" &&
      error.actual !== null &&
      "QueryInterface" in error.actual
    ) {
      throw new Error("don't attempt to stringify this XPCOM object");
    }
    actual = JSON.stringify(error.actual, replacer);
  } catch (ex) {
    actual = Object.prototype.toString.call(error.actual);
  }
  try {
    if (
      typeof error.expected == "object" &&
      error.expected !== null &&
      "QueryInterface" in error.expected
    ) {
      throw new Error("don't attempt to stringify this XPCOM object");
    }
    expected = JSON.stringify(error.expected, replacer);
  } catch (ex) {
    expected = Object.prototype.toString.call(error.expected);
  }
  let message = prefix;
  if (error.operator) {
    let truncateLength = error.truncate ? kTruncateLength : Infinity;
    message +=
      (prefix ? " - " : "") +
      truncate(actual, truncateLength) +
      " " +
      error.operator +
      " " +
      truncate(expected, truncateLength);
  }
  return message;
}

/**
 * 2. The AssertionError is defined in assert.
 *
 * At present only the four keys mentioned below are used and
 * understood by the spec. Implementations or sub modules can pass
 * other keys to the AssertionError's constructor - they will be
 * ignored.
 *
 * @example
 *
 * new assert.AssertionError({
 *   message: message,
 *   actual: actual,
 *   expected: expected,
 *   operator: operator,
 *   truncate: truncate,
 *   stack: stack, // Optional, defaults to the current stack.
 * });
 *
 */
Assert.AssertionError = function (options) {
  this.name = "AssertionError";
  this.actual = options.actual;
  this.expected = options.expected;
  this.operator = options.operator;
  this.message = getMessage(this, options.message, options.truncate);
  // The part of the stack that comes from this module is not interesting.
  let stack = options.stack || Components.stack;
  do {
    stack = stack.asyncCaller || stack.caller;
  } while (
    stack &&
    stack.filename &&
    stack.filename.includes("Assert.sys.mjs")
  );
  this.stack = stack;
};

// assert.AssertionError instanceof Error
Assert.AssertionError.prototype = Object.create(Error.prototype, {
  constructor: {
    value: Assert.AssertionError,
    enumerable: false,
    writable: true,
    configurable: true,
  },
});

Assert.prototype._reporter = null;

/**
 * This callback type is used for custom assertion report handling.
 *
 * @callback reporterFunc
 * @param {AssertionError|null} err
 *        An error object when the assertion failed, or null when it passed.
 * @param {String} message
 *        Message describing the assertion.
 * @param {Stack} stack
 *        Stack trace of the assertion function.
 */

/**
 * Set a custom assertion report handler function.
 *
 * @example
 *
 * Assert.setReporter(function customReporter(err, message, stack) {
 *   if (err) {
 *     do_report_result(false, err.message, err.stack);
 *   } else {
 *     do_report_result(true, message, stack);
 *   }
 * });
 *
 * @param {reporterFunc} reporterFunc
 *        Report handler function.
 */
Assert.prototype.setReporter = function (reporterFunc) {
  this._reporter = reporterFunc;
};

/**
 * 3. All of the following functions must throw an AssertionError when a
 * corresponding condition is not met, with a message that may be undefined if
 * not provided.  All assertion methods provide both the actual and expected
 * values to the assertion error for display purposes.
 *
 * This report method only throws errors on assertion failures, as per spec,
 * but consumers of this module (think: xpcshell-test, mochitest) may want to
 * override this default implementation.
 *
 * @example
 *
 * // The following will report an assertion failure.
 * this.report(1 != 2, 1, 2, "testing JS number math!", "==");
 *
 * @param {boolean} failed
 *        Indicates if the assertion failed or not.
 * @param {*} actual
 *        The result of evaluating the assertion.
 * @param {*} [expected]
 *        Expected result from the test author.
 * @param {String} [message]
 *        Short explanation of the expected result.
 * @param {String} [operator]
 *        Operation qualifier used by the assertion method (ex: '==').
 * @param {boolean} [truncate=true]
 *        Whether or not ``actual`` and ``expected`` should be truncated when printing.
 * @param {nsIStackFrame} [stack]
 *        The stack trace including the caller of the assertion method,
 *        if this cannot be inferred automatically (e.g. due to async callbacks).
 */
Assert.prototype.report = function (
  failed,
  actual,
  expected,
  message,
  operator,
  truncate = true,
  stack = null // Defaults to Components.stack in AssertionError.
) {
  // Although not ideal, we allow a "null" message due to the way some of the extension tests
  // work.
  if (message !== undefined && message !== null && typeof message != "string") {
    this.ok(
      false,
      `Expected a string or undefined for the error message to Assert.*, got ${typeof message}`
    );
  }
  let err = new Assert.AssertionError({
    message,
    actual,
    expected,
    operator,
    truncate,
    stack,
  });
  if (!this._reporter) {
    // If no custom reporter is set, throw the error.
    if (failed) {
      throw err;
    }
  } else {
    this._reporter(failed ? err : null, err.message, err.stack);
  }
};

/**
 * 4. Pure assertion tests whether a value is truthy, as determined by !!guard.
 * ``assert.ok(guard, message_opt);``
 * This statement is equivalent to ``assert.equal(true, !!guard, message_opt);``.
 * To test strictly for the value true, use ``assert.strictEqual(true, guard,
 * message_opt);``.
 *
 * @param {*} value
 *        Test subject to be evaluated as truthy.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.ok = function (value, message) {
  if (arguments.length > 2) {
    this.report(
      true,
      false,
      true,
      "Too many arguments passed to `Assert.ok()`",
      "=="
    );
  } else {
    this.report(!value, value, true, message, "==");
  }
};

/**
 * 5. The equality assertion tests shallow, coercive equality with ==.
 * ``assert.equal(actual, expected, message_opt);``
 *
 * @param {*} actual
 *        Test subject to be evaluated as equivalent to ``expected``.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.equal = function equal(actual, expected, message) {
  this.report(actual != expected, actual, expected, message, "==");
};

/**
 * 6. The non-equality assertion tests for whether two objects are not equal
 * with ``!=``
 *
 * @example
 * assert.notEqual(actual, expected, message_opt);
 *
 * @param {*} actual
 *        Test subject to be evaluated as NOT equivalent to ``expected``.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.notEqual = function notEqual(actual, expected, message) {
  this.report(actual == expected, actual, expected, message, "!=");
};

/**
 * 7. The equivalence assertion tests a deep equality relation.
 * assert.deepEqual(actual, expected, message_opt);
 *
 * We check using the most exact approximation of equality between two objects
 * to keep the chance of false positives to a minimum.
 * `JSON.stringify` is not designed to be used for this purpose; objects may
 * have ambiguous `toJSON()` implementations that would influence the test.
 *
 * @param {*} actual
 *        Test subject to be evaluated as equivalent to ``expected``, including nested properties.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.deepEqual = function deepEqual(actual, expected, message) {
  this.report(
    !ObjectUtils.deepEqual(actual, expected),
    actual,
    expected,
    message,
    "deepEqual",
    false
  );
};

/**
 * 8. The non-equivalence assertion tests for any deep inequality.
 * assert.notDeepEqual(actual, expected, message_opt);
 *
 * @param {*} actual
 *        Test subject to be evaluated as NOT equivalent to ``expected``, including nested
 *        properties.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.notDeepEqual = function notDeepEqual(
  actual,
  expected,
  message
) {
  this.report(
    ObjectUtils.deepEqual(actual, expected),
    actual,
    expected,
    message,
    "notDeepEqual",
    false
  );
};

/**
 * 9. The strict equality assertion tests strict equality, as determined by ===.
 * ``assert.strictEqual(actual, expected, message_opt);``
 *
 * @param {*} actual
 *        Test subject to be evaluated as strictly equivalent to ``expected``.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.strictEqual = function strictEqual(actual, expected, message) {
  this.report(actual !== expected, actual, expected, message, "===");
};

/**
 * 10. The strict non-equality assertion tests for strict inequality, as
 * determined by !==. ``assert.notStrictEqual(actual, expected, message_opt);``
 *
 * @param {*} actual
 *        Test subject to be evaluated as NOT strictly equivalent to ``expected``.
 * @param {*} expected
 *        Test reference to evaluate against ``actual``.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.notStrictEqual = function notStrictEqual(
  actual,
  expected,
  message
) {
  this.report(actual === expected, actual, expected, message, "!==");
};

function checkExpectedArgument(instance, funcName, expected) {
  if (!expected) {
    instance.ok(
      false,
      `Error: The 'expected' argument was not supplied to Assert.${funcName}()`
    );
  }

  if (
    !instanceOf(expected, "RegExp") &&
    typeof expected !== "function" &&
    typeof expected !== "object"
  ) {
    instance.ok(
      false,
      `Error: The 'expected' argument to Assert.${funcName}() must be a RegExp, function or an object`
    );
  }
}

function expectedException(actual, expected) {
  if (!actual || !expected) {
    return false;
  }

  if (instanceOf(expected, "RegExp")) {
    return expected.test(actual);
    // We need to guard against the right hand parameter of "instanceof" lacking
    // the "prototype" property, which is true of arrow functions in particular.
  } else if (
    !(typeof expected === "function" && !expected.prototype) &&
    actual instanceof expected
  ) {
    return true;
  } else if (expected.call({}, actual) === true) {
    return true;
  }

  return false;
}

/**
 * 11. Expected to throw an error:
 * assert.throws(block, Error_opt, message_opt);
 *
 * Example:
 * ```js
 * // The following will verify that an error of type TypeError was thrown:
 * Assert.throws(() => testBody(), TypeError);
 * // The following will verify that an error was thrown with an error message matching "hello":
 * Assert.throws(() => testBody(), /hello/);
 * ```
 *
 * @param {Function} block
 *        Function to evaluate and catch eventual thrown errors.
 * @param {RegExp|Function} expected
 *        This parameter can be either a RegExp or a function. The function is
 *        either the error type's constructor, or it's a method that returns
 *        a boolean that describes the test outcome.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.throws = function (block, expected, message) {
  checkExpectedArgument(this, "throws", expected);

  // `true` if we realize that we have added an
  // error to `ChromeUtils.recentJSDevError` and
  // that we probably need to clean it up.
  let cleanupRecentJSDevError = false;
  if ("recentJSDevError" in ChromeUtils) {
    // Check that we're in a build of Firefox that supports
    // the `recentJSDevError` mechanism (i.e. Nightly build).
    if (ChromeUtils.recentJSDevError === undefined) {
      // There was no previous error, so if we throw
      // an error here, we may need to clean it up.
      cleanupRecentJSDevError = true;
    }
  }

  let actual;

  try {
    block();
  } catch (e) {
    actual = e;
  }

  message =
    (expected.name ? " (" + expected.name + ")." : ".") +
    (message ? " " + message : ".");

  if (!actual) {
    this.report(true, actual, expected, "Missing expected exception" + message);
  }

  if (actual && !expectedException(actual, expected)) {
    throw actual;
  }

  this.report(false, expected, expected, message);

  // Make sure that we don't cause failures for JS Dev Errors that
  // were expected, typically for tests that attempt to check
  // that we react properly to TypeError, ReferenceError, SyntaxError.
  if (cleanupRecentJSDevError) {
    let recentJSDevError = ChromeUtils.recentJSDevError;
    if (recentJSDevError) {
      if (expectedException(recentJSDevError)) {
        ChromeUtils.clearRecentJSDevError();
      }
    }
  }
};

/**
 * A promise that is expected to reject:
 * assert.rejects(promise, expected, message);
 *
 * @param {Promise} promise
 *        A promise that is expected to reject.
 * @param {?} [expected]
 *        Test reference to evaluate against the rejection result.
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.rejects = function (promise, expected, message) {
  checkExpectedArgument(this, "rejects", expected);
  const operator = undefined; // Should we use "rejects" here?
  const stack = Components.stack;
  return new Promise((resolve, reject) => {
    return promise
      .then(
        () => {
          this.report(
            true,
            null,
            expected,
            "Missing expected exception " + message,
            operator,
            true,
            stack
          );
          // this.report() above should raise an AssertionError. If _reporter
          // has been overridden and doesn't throw an error, just resolve.
          // Otherwise we'll have a never-resolving promise that got stuck.
          resolve();
        },
        err => {
          if (!expectedException(err, expected)) {
            // TODO bug 1480075: Should report error instead of rejecting.
            reject(err);
            return;
          }
          this.report(false, err, expected, message, operator, truncate, stack);
          resolve();
        }
      )
      .catch(reject);
  });
};

function compareNumbers(expression, lhs, rhs, message, operator) {
  let lhsIsNumber = typeof lhs == "number" && !Number.isNaN(lhs);
  let rhsIsNumber = typeof rhs == "number" && !Number.isNaN(rhs);

  if (lhsIsNumber && rhsIsNumber) {
    this.report(expression, lhs, rhs, message, operator);
    return;
  }
  let lhsIsDate =
    typeof lhs == "object" && lhs.constructor.name == "Date" && !isNaN(lhs);
  let rhsIsDate =
    typeof rhs == "object" && rhs.constructor.name == "Date" && !isNaN(rhs);
  if (lhsIsDate && rhsIsDate) {
    this.report(expression, lhs, rhs, message, operator);
    return;
  }

  let errorMessage;
  if (!lhsIsNumber && !rhsIsNumber && !lhsIsDate && !rhsIsDate) {
    errorMessage = `Neither '${lhs}' nor '${rhs}' are numbers or dates.`;
  } else if ((lhsIsNumber && rhsIsDate) || (lhsIsDate && rhsIsNumber)) {
    errorMessage = `'${lhsIsNumber ? lhs : rhs}' is a number and '${
      rhsIsDate ? rhs : lhs
    }' is a date.`;
  } else {
    errorMessage = `'${
      lhsIsNumber || lhsIsDate ? rhs : lhs
    }' is not a number or date.`;
  }
  this.report(true, lhs, rhs, errorMessage);
}

/**
 * The lhs must be greater than the rhs.
 * assert.greater(lhs, rhs, message_opt);
 *
 * @param {Number} lhs
 *        The left-hand side value.
 * @param {Number} rhs
 *        The right-hand side value.
 * @param {String} [message]
 *        Short explanation of the comparison result.
 */
Assert.prototype.greater = function greater(lhs, rhs, message) {
  compareNumbers.call(this, lhs <= rhs, lhs, rhs, message, ">");
};

/**
 * The lhs must be greater than or equal to the rhs.
 * assert.greaterOrEqual(lhs, rhs, message_opt);
 *
 * @param {Number} [lhs]
 *        The left-hand side value.
 * @param {Number} [rhs]
 *        The right-hand side value.
 * @param {String} [message]
 *        Short explanation of the comparison result.
 */
Assert.prototype.greaterOrEqual = function greaterOrEqual(lhs, rhs, message) {
  compareNumbers.call(this, lhs < rhs, lhs, rhs, message, ">=");
};

/**
 * The lhs must be less than the rhs.
 * assert.less(lhs, rhs, message_opt);
 *
 * @param {Number} [lhs]
 *        The left-hand side value.
 * @param {Number} [rhs]
 *        The right-hand side value.
 * @param {String} [message]
 *        Short explanation of the comparison result.
 */
Assert.prototype.less = function less(lhs, rhs, message) {
  compareNumbers.call(this, lhs >= rhs, lhs, rhs, message, "<");
};

/**
 * The lhs must be less than or equal to the rhs.
 * assert.lessOrEqual(lhs, rhs, message_opt);
 *
 * @param {Number} [lhs]
 *        The left-hand side value.
 * @param {Number} [rhs]
 *        The right-hand side value.
 * @param {String} [message]
 *        Short explanation of the comparison result.
 */
Assert.prototype.lessOrEqual = function lessOrEqual(lhs, rhs, message) {
  compareNumbers.call(this, lhs > rhs, lhs, rhs, message, "<=");
};

/**
 * The lhs must be a string that matches the rhs regular expression.
 * rhs can be specified either as a string or a RegExp object. If specified as a
 * string it will be interpreted as a regular expression so take care to escape
 * special characters such as "?" or "(" if you need the actual characters.
 *
 * @param {String} lhs
 *        The string to be tested.
 * @param {String|RegExp} rhs
 *        The regular expression that the string will be tested with.
 *        Note that if passed as a string, this will be interpreted.
 *        as a regular expression.
 * @param {String} [message]
 *        Short explanation of the comparison result.
 */
Assert.prototype.stringMatches = function stringMatches(lhs, rhs, message) {
  if (typeof rhs != "string" && !instanceOf(rhs, "RegExp")) {
    this.report(
      true,
      lhs,
      String(rhs),
      `Expected a string or a RegExp for rhs, but "${rhs}" isn't a string or a RegExp object.`
    );
    return;
  }

  if (typeof lhs != "string") {
    this.report(
      true,
      lhs,
      String(rhs),
      `Expected a string for lhs, but "${lhs}" isn't a string.`
    );
    return;
  }

  if (typeof rhs == "string") {
    try {
      rhs = new RegExp(rhs);
    } catch {
      this.report(
        true,
        lhs,
        rhs,
        `Expected a valid regular expression for rhs, but "${rhs}" isn't one.`
      );
      return;
    }
  }

  const isCorrect = rhs.test(lhs);
  this.report(!isCorrect, lhs, rhs.toString(), message, "matches");
};

/**
 * The lhs must be a string that contains the rhs string.
 *
 * @param {String} lhs
 *        The string to be tested (haystack).
 * @param {String} rhs
 *        The string to be found (needle).
 * @param {String} [message]
 *        Short explanation of the expected result.
 */
Assert.prototype.stringContains = function stringContains(lhs, rhs, message) {
  if (typeof lhs != "string" || typeof rhs != "string") {
    this.report(
      true,
      lhs,
      rhs,
      `Expected a string for both lhs and rhs, but either "${lhs}" or "${rhs}" is not a string.`
    );
  }

  const isCorrect = lhs.includes(rhs);
  this.report(!isCorrect, lhs, rhs, message, "includes");
};
