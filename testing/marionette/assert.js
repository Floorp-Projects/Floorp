/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["assert"];

const isFennec = () => AppConstants.platform == "android";
const isFirefox = () => Services.appinfo.name == "Firefox";

/** Shorthands for common assertions made in Marionette. */
this.assert = {};

/**
 * Asserts that Marionette has a session.
 *
 * @param {GeckoDriver} driver
 *     Marionette driver instance.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     Session ID.
 *
 * @throws {InvalidSessionIDError}
 *     If |driver| does not have a session ID.
 */
assert.session = function (driver, msg = "") {
  assert.that(sessionID => sessionID,
      msg, InvalidSessionIDError)(driver.sessionId);
  return driver.sessionId;
};

/**
 * Asserts that the current browser is Firefox Desktop.
 *
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {UnsupportedOperationError}
 *     If current browser is not Firefox.
 */
assert.firefox = function (msg = "") {
  msg = msg || "Only supported in Firefox";
  assert.that(isFirefox, msg, UnsupportedOperationError)();
};

/**
 * Asserts that the current browser is Fennec, or Firefox for Android.
 *
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {UnsupportedOperationError}
 *     If current browser is not Fennec.
 */
assert.fennec = function (msg = "") {
  msg = msg || "Only supported in Fennec";
  assert.that(isFennec, msg, UnsupportedOperationError)();
};

/**
 * Asserts that the current |context| is content.
 *
 * @param {string} context
 *     Context to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     |context| is returned unaltered.
 *
 * @throws {UnsupportedOperationError}
 *     If |context| is not content.
 */
assert.content = function (context, msg = "") {
  msg = msg || "Only supported in content context";
  assert.that(c => c.toString() == "content", msg, UnsupportedOperationError)(context);
};

/**
 * Asserts that |win| is open.
 *
 * @param {ChromeWindow} win
 *     Chrome window to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {ChromeWindow}
 *     |win| is returned unaltered.
 *
 * @throws {NoSuchWindowError}
 *     If |win| has been closed.
 */
assert.window = function (win, msg = "") {
  msg = msg || "Unable to locate window";
  return assert.that(w => w && !w.closed,
      msg,
      NoSuchWindowError)(win);
};

/**
 * Asserts that |context| is a valid browsing context.
 *
 * @param {browser.Context} context
 *     Browsing context to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {NoSuchWindowError}
 *     If |context| is invalid.
 */
assert.contentBrowser = function (context, msg = "") {
  // TODO: The contentBrowser uses a cached tab, which is only updated when
  // switchToTab is called. Because of that an additional check is needed to
  // make sure that the chrome window has not already been closed.
  assert.window(context && context.window);

  msg = msg || "Current window does not have a content browser";
  assert.that(c => c.contentBrowser,
      msg,
      NoSuchWindowError)(context);
};

/**
 * Asserts that there is no current user prompt.
 *
 * @param {modal.Dialog} dialog
 *     Reference to current dialogue.
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {UnexpectedAlertOpenError}
 *     If there is a user prompt.
 */
assert.noUserPrompt = function (dialog, msg = "") {
  assert.that(d => d === null || typeof d == "undefined",
      msg,
      UnexpectedAlertOpenError)(dialog);
};

/**
 * Asserts that |obj| is defined.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {?}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not defined.
 */
assert.defined = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be defined`;
  return assert.that(o => typeof o != "undefined", msg)(obj);
};

/**
 * Asserts that |obj| is a finite number.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not a number.
 */
assert.number = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be finite number`;
  return assert.that(Number.isFinite, msg)(obj);
};

/**
 * Asserts that |obj| is callable.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Function}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not callable.
 */
assert.callable = function (obj, msg = "") {
  msg = msg || error.pprint`${obj} is not callable`;
  return assert.that(o => typeof o == "function", msg)(obj);
};

/**
 * Asserts that |obj| is an integer.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not an integer.
 */
assert.integer = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be an integer`;
  return assert.that(Number.isInteger, msg)(obj);
};

/**
 * Asserts that |obj| is a positive integer.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not a positive integer.
 */
assert.positiveInteger = function (obj, msg = "") {
  assert.integer(obj, msg);
  msg = msg || error.pprint`Expected ${obj} to be >= 0`;
  return assert.that(n => n >= 0, msg)(obj);
};

/**
 * Asserts that |obj| is a boolean.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {boolean}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not a boolean.
 */
assert.boolean = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be boolean`;
  return assert.that(b => typeof b == "boolean", msg)(obj);
};

/**
 * Asserts that |obj| is a string.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not a string.
 */
assert.string = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be a string`;
  return assert.that(s => typeof s == "string", msg)(obj);
};

/**
 * Asserts that |obj| is an object.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Object}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not an object.
 */
assert.object = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be an object`;
  return assert.that(o => {
    // unable to use instanceof because LHS and RHS may come from
    // different globals
    let s = Object.prototype.toString.call(o);
    return s == "[object Object]" || s == "[object nsJSIID]";
  })(obj);
};

/**
 * Asserts that |prop| is in |obj|.
 *
 * @param {?} prop
 *     Own property to test if is in |obj|.
 * @param {?} obj
 *     Object.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {?}
 *     Value of |obj|'s own property |prop|.
 *
 * @throws {InvalidArgumentError}
 *     If |prop| is not in |obj|, or |obj| is not an object.
 */
assert.in = function (prop, obj, msg = "") {
  assert.object(obj, msg);
  msg = msg || error.pprint`Expected ${prop} in ${obj}`;
  assert.that(p => obj.hasOwnProperty(p), msg)(prop);
  return obj[prop];
};

/**
 * Asserts that |obj| is an Array.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Object}
 *     |obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If |obj| is not an Array.
 */
assert.array = function (obj, msg = "") {
  msg = msg || error.pprint`Expected ${obj} to be an Array`;
  return assert.that(Array.isArray, msg)(obj);
};

/**
 * Returns a function that is used to assert the |predicate|.
 *
 * @param {function(?): boolean} predicate
 *     Evaluated on calling the return value of this function.  If its
 *     return value of the inner function is false, |error| is thrown
 *     with |message|.
 * @param {string=} message
 *     Custom error message.
 * @param {Error=} error
 *     Custom error type by its class.
 *
 * @return {function(?): ?}
 *     Function that takes and returns the passed in value unaltered, and
 *     which may throw |error| with |message| if |predicate| evaluates
 *     to false.
 */
assert.that = function (
    predicate, message = "", error = InvalidArgumentError) {
  return obj => {
    if (!predicate(obj)) {
      throw new error(message);
    }
    return obj;
  };
};
