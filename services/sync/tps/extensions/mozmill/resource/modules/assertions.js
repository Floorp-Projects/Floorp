/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <mail@hskupin.info> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Use the frame module of Mozmill to raise non-fatal failures
var mozmillFrame = {};
Cu.import('resource://mozmill/modules/frame.js', mozmillFrame);


/**
 * @name assertions
 * @namespace Defines expect and assert methods to be used for assertions.
 */
var assertions = exports;


/* non-fatal assertions */
var Expect = function() {}

Expect.prototype = {

  /**
   * Log a test as failing by adding a fail frame.
   *
   * @param {object} aResult
   *   Test result details used for reporting.
   *   <dl>
   *     <dd>fileName</dd>
   *     <dt>Name of the file in which the assertion failed.</dt>
   *     <dd>function</dd>
   *     <dt>Function in which the assertion failed.</dt>
   *     <dd>lineNumber</dd>
   *     <dt>Line number of the file in which the assertion failed.</dt>
   *     <dd>message</dd>
   *     <dt>Message why the assertion failed.</dt>
   *   </dl>
   */
  _logFail: function Expect__logFail(aResult) {
    mozmillFrame.events.fail({fail: aResult});
  },

  /**
   * Log a test as passing by adding a pass frame.
   *
   * @param {object} aResult
   *   Test result details used for reporting.
   *   <dl>
   *     <dd>fileName</dd>
   *     <dt>Name of the file in which the assertion failed.</dt>
   *     <dd>function</dd>
   *     <dt>Function in which the assertion failed.</dt>
   *     <dd>lineNumber</dd>
   *     <dt>Line number of the file in which the assertion failed.</dt>
   *     <dd>message</dd>
   *     <dt>Message why the assertion failed.</dt>
   *   </dl>
   */
  _logPass: function Expect__logPass(aResult) {
    mozmillFrame.events.pass({pass: aResult});
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
   * @returns {boolean} Result of the test.
   */
  _test: function Expect__test(aCondition, aMessage, aDiagnosis) {
    let diagnosis = aDiagnosis || "";
    let message = aMessage || "";

    if (diagnosis)
      message = aMessage ? message + " - " + diagnosis : diagnosis;

    // Build result data
    let frame = Components.stack;
    let result = {
      'fileName'   : frame.filename.replace(/(.*)-> /, ""),
      'function'   : frame.name,
      'lineNumber' : frame.lineNumber,
      'message'    : message
    };

    // Log test result
    if (aCondition)
      this._logPass(result);
    else
      this._logFail(result);

    return aCondition;
  },

  /**
   * Perform an always passing test
   *
   * @param {string} aMessage
   *   Message to show for the test result.
   * @returns {boolean} Always returns true.
   */
  pass: function Expect_pass(aMessage) {
    return this._test(true, aMessage, undefined);
  },

  /**
   * Perform an always failing test
   *
   * @param {string} aMessage
   *   Message to show for the test result.
   * @returns {boolean} Always returns false.
   */
  fail: function Expect_fail(aMessage) {
    return this._test(false, aMessage, undefined);
  },

  /**
   * Test if the value pass
   *
   * @param {boolean|string|number|object} aValue
   *   Value to test.
   * @param {string} aMessage
   *   Message to show for the test result.
   * @returns {boolean} Result of the test.
   */
  ok: function Expect_ok(aValue, aMessage) {
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
   * @returns {boolean} Result of the test.
   */
  equal: function Expect_equal(aValue, aExpected, aMessage) {
    let condition = (aValue === aExpected);
    let diagnosis = "got '" + aValue + "', expected '" + aExpected + "'";
  
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
   * @returns {boolean} Result of the test.
   */
  notEqual: function Expect_notEqual(aValue, aExpected, aMessage) {
    let condition = (aValue !== aExpected);
    let diagnosis = "got '" + aValue + "', not expected '" + aExpected + "'";

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
   * @returns {boolean} Result of the test.
   */
  match: function Expect_match(aString, aRegex, aMessage) {
    // XXX Bug 634948
    // Regex objects are transformed to strings when evaluated in a sandbox
    // For now lets re-create the regex from its string representation
    let pattern = flags = "";
    try {
      let matches = aRegex.toString().match(/\/(.*)\/(.*)/);

      pattern = matches[1];
      flags = matches[2];
    }
    catch (ex) {
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
   * @returns {boolean} Result of the test.
   */
  notMatch: function Expect_notMatch(aString, aRegex, aMessage) {
    // XXX Bug 634948
    // Regex objects are transformed to strings when evaluated in a sandbox
    // For now lets re-create the regex from its string representation
    let pattern = flags = "";
    try {
      let matches = aRegex.toString().match(/\/(.*)\/(.*)/);

      pattern = matches[1];
      flags = matches[2];
    }
    catch (ex) {
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
   * @returns {boolean} Result of the test.
   */
  throws : function Expect_throws(block, /*optional*/error, /*optional*/message) {
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
   * @returns {boolean} Result of the test.
   */
  doesNotThrow : function Expect_doesNotThrow(block, /*optional*/error, /*optional*/message) {
    return this._throws.apply(this, [false].concat(Array.prototype.slice.call(arguments)));
  },
  
  /* Tests whether a code block throws the expected exception
     class. helper for throws() and doesNotThrow()
     
     adapted from node.js's assert._throws()
     https://github.com/joyent/node/blob/master/lib/assert.js
  */
  _throws : function Expect__throws(shouldThrow, block, expected, message) {
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
  
  _expectedException : function Expect__expectedException(actual, expected) {
    if (!actual || !expected) {
      return false;
    }

    if (expected instanceof RegExp) {
      return expected.test(actual);
    } else if (actual instanceof expected) {
      return true;
    } else if (expected.call({}, actual) === true) {
      return true;
    }

    return false;
  }
}

/**
* AssertionError
*
* Error object thrown by failing assertions
*/
function AssertionError(message, fileName, lineNumber) {
  var err = new Error();
  if (err.stack) {
    this.stack = err.stack;
  }
  this.message = message === undefined ? err.message : message;
  this.fileName = fileName === undefined ? err.fileName : fileName;
  this.lineNumber = lineNumber === undefined ? err.lineNumber : lineNumber;
};
AssertionError.prototype = new Error();
AssertionError.prototype.constructor = AssertionError;
AssertionError.prototype.name = 'AssertionError';


var Assert = function() {}

Assert.prototype = new Expect();

Assert.prototype.AssertionError = AssertionError;

/**
 * The Assert class implements fatal assertions, and can be used in cases
 * when a failing test has to directly abort the current test function. All
 * remaining tasks will not be performed.
 *
 */

/**
 * Log a test as failing by throwing an AssertionException.
 *
 * @param {object} aResult
 *   Test result details used for reporting.
 *   <dl>
 *     <dd>fileName</dd>
 *     <dt>Name of the file in which the assertion failed.</dt>
 *     <dd>function</dd>
 *     <dt>Function in which the assertion failed.</dt>
 *     <dd>lineNumber</dd>
 *     <dt>Line number of the file in which the assertion failed.</dt>
 *     <dd>message</dd>
 *     <dt>Message why the assertion failed.</dt>
 *   </dl>
 * @throws {AssertionError }
 */
Assert.prototype._logFail = function Assert__logFail(aResult) {
  throw new AssertionError(aResult);
}


// Export of variables
assertions.Expect = Expect;
assertions.Assert = Assert;