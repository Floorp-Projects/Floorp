/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['Assert', 'Expect'];

var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

var broker = {}; Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var errors = {}; Cu.import('resource://mozmill/modules/errors.js', errors);
var stack = {}; Cu.import('resource://mozmill/modules/stack.js', stack);

/**
 * @name assertions
 * @namespace Defines expect and assert methods to be used for assertions.
 */

/**
 * The Assert class implements fatal assertions, and can be used in cases
 * when a failing test has to directly abort the current test function. All
 * remaining tasks will not be performed.
 *
 */
var Assert = function () {}

Assert.prototype = {

  // The following deepEquals implementation is from Narwhal under this license:

  // http://wiki.commonjs.org/wiki/Unit_Testing/1.0
  //
  // THIS IS NOT TESTED NOR LIKELY TO WORK OUTSIDE V8!
  //
  // Originally from narwhal.js (http://narwhaljs.org)
  // Copyright (c) 2009 Thomas Robinson <280north.com>
  //
  // Permission is hereby granted, free of charge, to any person obtaining a copy
  // of this software and associated documentation files (the 'Software'), to
  // deal in the Software without restriction, including without limitation the
  // rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  // sell copies of the Software, and to permit persons to whom the Software is
  // furnished to do so, subject to the following conditions:
  //
  // The above copyright notice and this permission notice shall be included in
  // all copies or substantial portions of the Software.
  //
  // THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  // AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  // ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  // WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  _deepEqual: function (actual, expected) {
    // 7.1. All identical values are equivalent, as determined by ===.
    if (actual === expected) {
      return true;

    // 7.2. If the expected value is a Date object, the actual value is
    // equivalent if it is also a Date object that refers to the same time.
    } else if (actual instanceof Date && expected instanceof Date) {
      return actual.getTime() === expected.getTime();

    // 7.3. Other pairs that do not both pass typeof value == 'object',
    // equivalence is determined by ==.
    } else if (typeof actual != 'object' && typeof expected != 'object') {
      return actual == expected;

    // 7.4. For all other Object pairs, including Array objects, equivalence is
    // determined by having the same number of owned properties (as verified
    // with Object.prototype.hasOwnProperty.call), the same set of keys
    // (although not necessarily the same order), equivalent values for every
    // corresponding key, and an identical 'prototype' property. Note: this
    // accounts for both named and indexed properties on Arrays.
    } else {
      return this._objEquiv(actual, expected);
    }
  },

  _objEquiv: function (a, b) {
    if (a == null || a == undefined || b == null || b == undefined)
      return false;
    // an identical 'prototype' property.
    if (a.prototype !== b.prototype) return false;

    function isArguments(object) {
      return Object.prototype.toString.call(object) == '[object Arguments]';
    }

    //~~~I've managed to break Object.keys through screwy arguments passing.
    // Converting to array solves the problem.
    if (isArguments(a)) {
      if (!isArguments(b)) {
        return false;
      }
      a = pSlice.call(a);
      b = pSlice.call(b);
      return _deepEqual(a, b);
    }
    try {
      var ka = Object.keys(a),
          kb = Object.keys(b),
          key, i;
    } catch (e) {//happens when one is a string literal and the other isn't
      return false;
    }
    // having the same number of owned properties (keys incorporates
    // hasOwnProperty)
    if (ka.length != kb.length)
      return false;
    //the same set of keys (although not necessarily the same order),
    ka.sort();
    kb.sort();
    //~~~cheap key test
    for (i = ka.length - 1; i >= 0; i--) {
      if (ka[i] != kb[i])
        return false;
    }
    //equivalent values for every corresponding key, and
    //~~~possibly expensive deep test
    for (i = ka.length - 1; i >= 0; i--) {
      key = ka[i];
      if (!this._deepEqual(a[key], b[key])) return false;
    }
    return true;
  },

  _expectedException : function Assert__expectedException(actual, expected) {
    if (!actual || !expected) {
      return false;
    }

    if (expected instanceof RegExp) {
      return expected.test(actual);
    } else if (actual instanceof expected) {
      return true;
    } else if (expected.call({}, actual) === true) {
      return true;
    } else if (actual.name === expected.name) {
      return true;
    }

    return false;
  },

  /**
   * Log a test as failing by throwing an AssertionException.
   *
   * @param {object} aResult
   *   Test result details used for reporting.
   *   <dl>
   *     <dd>fileName</dd>
   *     <dt>Name of the file in which the assertion failed.</dt>
   *     <dd>functionName</dd>
   *     <dt>Function in which the assertion failed.</dt>
   *     <dd>lineNumber</dd>
   *     <dt>Line number of the file in which the assertion failed.</dt>
   *     <dd>message</dd>
   *     <dt>Message why the assertion failed.</dt>
   *   </dl>
   * @throws {errors.AssertionError}
   *
   */
  _logFail: function Assert__logFail(aResult) {
    throw new errors.AssertionError(aResult.message,
                                    aResult.fileName,
                                    aResult.lineNumber,
                                    aResult.functionName,
                                    aResult.name);
  },

  /**
   * Log a test as passing by adding a pass frame.
   *
   * @param {object} aResult
   *   Test result details used for reporting.
   *   <dl>
   *     <dd>fileName</dd>
   *     <dt>Name of the file in which the assertion failed.</dt>
   *     <dd>functionName</dd>
   *     <dt>Function in which the assertion failed.</dt>
   *     <dd>lineNumber</dd>
   *     <dt>Line number of the file in which the assertion failed.</dt>
   *     <dd>message</dd>
   *     <dt>Message why the assertion failed.</dt>
   *   </dl>
   */
  _logPass: function Assert__logPass(aResult) {
    broker.pass({pass: aResult});
  },

  /**
   * Test the condition and mark test as passed or failed
   *
   * @param {boolean} aCondition
   *   Condition to test.
   * @param {string} aMessage
   *   Message to show for the test result
   * @param {string} aDiagnosis
   *   Diagnose message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  _test: function Assert__test(aCondition, aMessage, aDiagnosis) {
    let diagnosis = aDiagnosis || "";
    let message = aMessage || "";

    if (diagnosis)
      message = aMessage ? message + " - " + diagnosis : diagnosis;

    // Build result data
    let frame = stack.findCallerFrame(Components.stack);

    let result = {
      'fileName'     : frame.filename.replace(/(.*)-> /, ""),
      'functionName' : frame.name,
      'lineNumber'   : frame.lineNumber,
      'message'      : message
    };

    // Log test result
    if (aCondition) {
      this._logPass(result);
    }
    else {
      result.stack = Components.stack;
      this._logFail(result);
    }

    return aCondition;
  },

  /**
   * Perform an always passing test
   *
   * @param {string} aMessage
   *   Message to show for the test result.
   * @returns {boolean} Always returns true.
   */
  pass: function Assert_pass(aMessage) {
    return this._test(true, aMessage, undefined);
  },

  /**
   * Perform an always failing test
   *
   * @param {string} aMessage
   *   Message to show for the test result.
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Always returns false.
   */
  fail: function Assert_fail(aMessage) {
    return this._test(false, aMessage, undefined);
  },

  /**
   * Test if the value pass
   *
   * @param {boolean|string|number|object} aValue
   *   Value to test.
   * @param {string} aMessage
   *   Message to show for the test result.
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  ok: function Assert_ok(aValue, aMessage) {
    let condition = !!aValue;
    let diagnosis = "got '" + aValue + "'";

    return this._test(condition, aMessage, diagnosis);
  },

 /**
   * Test if both specified values are identical.
   *
   * @param {boolean|string|number|object} aValue
   *   Value to test.
   * @param {boolean|string|number|object} aExpected
   *   Value to strictly compare with.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  equal: function Assert_equal(aValue, aExpected, aMessage) {
    let condition = (aValue === aExpected);
    let diagnosis = "'" + aValue + "' should equal '" + aExpected + "'";

    return this._test(condition, aMessage, diagnosis);
  },

 /**
   * Test if both specified values are not identical.
   *
   * @param {boolean|string|number|object} aValue
   *   Value to test.
   * @param {boolean|string|number|object} aExpected
   *   Value to strictly compare with.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  notEqual: function Assert_notEqual(aValue, aExpected, aMessage) {
    let condition = (aValue !== aExpected);
    let diagnosis = "'" + aValue + "' should not equal '" + aExpected + "'";

    return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Test if an object equals another object
   *
   * @param {object} aValue
   *   The object to test.
   * @param {object} aExpected
   *   The object to strictly compare with.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  deepEqual: function equal(aValue, aExpected, aMessage) {
    let condition = this._deepEqual(aValue, aExpected);
    try {
      var aValueString = JSON.stringify(aValue);
    } catch (e) {
      var aValueString = String(aValue);
    }
    try {
      var aExpectedString = JSON.stringify(aExpected);
    } catch (e) {
      var aExpectedString = String(aExpected);
    }

    let diagnosis = "'" + aValueString + "' should equal '" +
                    aExpectedString + "'";

    return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Test if an object does not equal another object
   *
   * @param {object} aValue
   *   The object to test.
   * @param {object} aExpected
   *   The object to strictly compare with.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  notDeepEqual: function notEqual(aValue, aExpected, aMessage) {
     let condition = !this._deepEqual(aValue, aExpected);
     try {
       var aValueString = JSON.stringify(aValue);
     } catch (e) {
       var aValueString = String(aValue);
     }
     try {
       var aExpectedString = JSON.stringify(aExpected);
     } catch (e) {
       var aExpectedString = String(aExpected);
     }

     let diagnosis = "'" + aValueString + "' should not equal '" +
                     aExpectedString + "'";

     return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Test if the regular expression matches the string.
   *
   * @param {string} aString
   *   String to test.
   * @param {RegEx} aRegex
   *   Regular expression to use for testing that a match exists.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  match: function Assert_match(aString, aRegex, aMessage) {
    // XXX Bug 634948
    // Regex objects are transformed to strings when evaluated in a sandbox
    // For now lets re-create the regex from its string representation
    let pattern = flags = "";
    try {
      let matches = aRegex.toString().match(/\/(.*)\/(.*)/);

      pattern = matches[1];
      flags = matches[2];
    } catch (e) {
    }

    let regex = new RegExp(pattern, flags);
    let condition = (aString.match(regex) !== null);
    let diagnosis = "'" + regex + "' matches for '" + aString + "'";

    return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Test if the regular expression does not match the string.
   *
   * @param {string} aString
   *   String to test.
   * @param {RegEx} aRegex
   *   Regular expression to use for testing that a match does not exist.
   * @param {string} aMessage
   *   Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  notMatch: function Assert_notMatch(aString, aRegex, aMessage) {
    // XXX Bug 634948
    // Regex objects are transformed to strings when evaluated in a sandbox
    // For now lets re-create the regex from its string representation
    let pattern = flags = "";
    try {
      let matches = aRegex.toString().match(/\/(.*)\/(.*)/);

      pattern = matches[1];
      flags = matches[2];
    } catch (e) {
    }

    let regex = new RegExp(pattern, flags);
    let condition = (aString.match(regex) === null);
    let diagnosis = "'" + regex + "' doesn't match for '" + aString + "'";

    return this._test(condition, aMessage, diagnosis);
  },


  /**
   * Test if a code block throws an exception.
   *
   * @param {string} block
   *   function to call to test for exception
   * @param {RegEx} error
   *   the expected error class
   * @param {string} message
   *   message to present if assertion fails
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  throws : function Assert_throws(block, /*optional*/error, /*optional*/message) {
    return this._throws.apply(this, [true].concat(Array.prototype.slice.call(arguments)));
  },

  /**
   * Test if a code block doesn't throw an exception.
   *
   * @param {string} block
   *   function to call to test for exception
   * @param {RegEx} error
   *   the expected error class
   * @param {string} message
   *   message to present if assertion fails
   * @throws {errors.AssertionError}
   *
   * @returns {boolean} Result of the test.
   */
  doesNotThrow : function Assert_doesNotThrow(block, /*optional*/error, /*optional*/message) {
    return this._throws.apply(this, [false].concat(Array.prototype.slice.call(arguments)));
  },

  /* Tests whether a code block throws the expected exception
     class. helper for throws() and doesNotThrow()

     adapted from node.js's assert._throws()
     https://github.com/joyent/node/blob/master/lib/assert.js
  */
  _throws : function Assert__throws(shouldThrow, block, expected, message) {
    var actual;

    if (typeof expected === 'string') {
      message = expected;
      expected = null;
    }

    try {
      block();
    } catch (e) {
      actual = e;
    }

    message = (expected && expected.name ? ' (' + expected.name + ').' : '.') +
              (message ? ' ' + message : '.');

    if (shouldThrow && !actual) {
      return this._test(false, message, 'Missing expected exception');
    }

    if (!shouldThrow && this._expectedException(actual, expected)) {
      return this._test(false, message, 'Got unwanted exception');
    }

    if ((shouldThrow && actual && expected &&
        !this._expectedException(actual, expected)) || (!shouldThrow && actual)) {
      throw actual;
    }

    return this._test(true, message);
  },

  /**
   * Test if the string contains the pattern.
   *
   * @param {String} aString String to test.
   * @param {String} aPattern Pattern to look for in the string
   * @param {String} aMessage Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {Boolean} Result of the test.
   */
  contain: function Assert_contain(aString, aPattern, aMessage) {
    let condition = (aString.indexOf(aPattern) !== -1);
    let diagnosis = "'" + aString + "' should contain '" + aPattern + "'";

    return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Test if the string does not contain the pattern.
   *
   * @param {String} aString String to test.
   * @param {String} aPattern Pattern to look for in the string
   * @param {String} aMessage Message to show for the test result
   * @throws {errors.AssertionError}
   *
   * @returns {Boolean} Result of the test.
   */
  notContain: function Assert_notContain(aString, aPattern, aMessage) {
    let condition = (aString.indexOf(aPattern) === -1);
    let diagnosis = "'" + aString + "' should not contain '" + aPattern + "'";

    return this._test(condition, aMessage, diagnosis);
  },

  /**
   * Waits for the callback evaluates to true
   *
   * @param {Function} aCallback
   *        Callback for evaluation
   * @param {String} aMessage
   *        Message to show for result
   * @param {Number} aTimeout
   *        Timeout in waiting for evaluation
   * @param {Number} aInterval
   *        Interval between evaluation attempts
   * @param {Object} aThisObject
   *        this object
   * @throws {errors.AssertionError}
   *
   * @returns {Boolean} Result of the test.
   */
  waitFor: function Assert_waitFor(aCallback, aMessage, aTimeout, aInterval, aThisObject) {
    var timeout = aTimeout || 5000;
    var interval = aInterval || 100;

    var self = {
      timeIsUp: false,
      result: aCallback.call(aThisObject)
    };
    var deadline = Date.now() + timeout;

    function wait() {
      if (self.result !== true) {
        self.result = aCallback.call(aThisObject);
        self.timeIsUp = Date.now() > deadline;
      }
    }

    var hwindow = Services.appShell.hiddenDOMWindow;
    var timeoutInterval = hwindow.setInterval(wait, interval);
    var thread = Services.tm.currentThread;

    Services.tm.spinEventLoopUntil(() => {
      let type = typeof(self.result);
      if (type !== 'boolean') {
        throw TypeError("waitFor() callback has to return a boolean" +
                        " instead of '" + type + "'");
      }

      return self.result === true || self.timeIsUp;
    });

    hwindow.clearInterval(timeoutInterval);

    if (self.result !== true && self.timeIsUp) {
      aMessage = aMessage || arguments.callee.name + ": Timeout exceeded for '" + aCallback + "'";
      throw new errors.TimeoutError(aMessage);
    }

    broker.pass({'function':'assert.waitFor()'});
    return true;
  }
}

/* non-fatal assertions */
var Expect = function () {}

Expect.prototype = new Assert();

/**
 * Log a test as failing by adding a fail frame.
 *
 * @param {object} aResult
 *   Test result details used for reporting.
 *   <dl>
 *     <dd>fileName</dd>
 *     <dt>Name of the file in which the assertion failed.</dt>
 *     <dd>functionName</dd>
 *     <dt>Function in which the assertion failed.</dt>
 *     <dd>lineNumber</dd>
 *     <dt>Line number of the file in which the assertion failed.</dt>
 *     <dd>message</dd>
 *     <dt>Message why the assertion failed.</dt>
 *   </dl>
 */
Expect.prototype._logFail = function Expect__logFail(aResult) {
  broker.fail({fail: aResult});
}

/**
 * Waits for the callback evaluates to true
 *
 * @param {Function} aCallback
 *        Callback for evaluation
 * @param {String} aMessage
 *        Message to show for result
 * @param {Number} aTimeout
 *        Timeout in waiting for evaluation
 * @param {Number} aInterval
 *        Interval between evaluation attempts
 * @param {Object} aThisObject
 *        this object
 */
Expect.prototype.waitFor = function Expect_waitFor(aCallback, aMessage, aTimeout, aInterval, aThisObject) {
  let condition = true;
  let message = aMessage;

  try {
    Assert.prototype.waitFor.apply(this, arguments);
  }
  catch (ex) {
    if (!(ex instanceof errors.AssertionError)) {
      throw ex;
    }
    message = ex.message;
    condition = false;
  }

  return this._test(condition, message);
}
