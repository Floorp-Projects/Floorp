/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["assert"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/marionette/appinfo.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  pprint: "chrome://remote/content/shared/Format.jsm",
});

/**
 * Shorthands for common assertions made in WebDriver.
 *
 * @namespace
 */
const assert = {};

/**
 * Asserts that WebDriver has an active session.
 *
 * @param {WebDriverSession} session
 *     WebDriver session instance.
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {InvalidSessionIDError}
 *     If session does not exist, or has an invalid id.
 */
assert.session = function(session, msg = "") {
  msg = msg || "WebDriver session does not exist, or is not active";
  assert.that(
    session => session && typeof session.id == "string",
    msg,
    lazy.error.InvalidSessionIDError
  )(session);
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
assert.firefox = function(msg = "") {
  msg = msg || "Only supported in Firefox";
  assert.that(
    isFirefox => isFirefox,
    msg,
    lazy.error.UnsupportedOperationError
  )(lazy.AppInfo.isFirefox);
};

/**
 * Asserts that the current application is Firefox Desktop or Thunderbird.
 *
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {UnsupportedOperationError}
 *     If current application is not running on desktop.
 */
assert.desktop = function(msg = "") {
  msg = msg || "Only supported in desktop applications";
  assert.that(
    isDesktop => isDesktop,
    msg,
    lazy.error.UnsupportedOperationError
  )(!lazy.AppInfo.isAndroid);
};

/**
 * Asserts that the current application runs on Android.
 *
 * @param {string=} msg
 *     Custom error message.
 *
 * @throws {UnsupportedOperationError}
 *     If current application is not running on Android.
 */
assert.mobile = function(msg = "") {
  msg = msg || "Only supported on Android";
  assert.that(
    isAndroid => isAndroid,
    msg,
    lazy.error.UnsupportedOperationError
  )(lazy.AppInfo.isAndroid);
};

/**
 * Asserts that the current <var>context</var> is content.
 *
 * @param {string} context
 *     Context to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     <var>context</var> is returned unaltered.
 *
 * @throws {UnsupportedOperationError}
 *     If <var>context</var> is not content.
 */
assert.content = function(context, msg = "") {
  msg = msg || "Only supported in content context";
  assert.that(
    c => c.toString() == "content",
    msg,
    lazy.error.UnsupportedOperationError
  )(context);
};

/**
 * Asserts that the {@link CanonicalBrowsingContext} is open.
 *
 * @param {CanonicalBrowsingContext} browsingContext
 *     Canonical browsing context to check.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {CanonicalBrowsingContext}
 *     <var>browsingContext</var> is returned unaltered.
 *
 * @throws {NoSuchWindowError}
 *     If <var>browsingContext</var> is no longer open.
 */
assert.open = function(browsingContext, msg = "") {
  msg = msg || "Browsing context has been discarded";
  return assert.that(
    browsingContext => {
      if (!browsingContext?.currentWindowGlobal) {
        return false;
      }

      if (browsingContext.isContent && !browsingContext.top.embedderElement) {
        return false;
      }

      return true;
    },
    msg,
    lazy.error.NoSuchWindowError
  )(browsingContext);
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
assert.noUserPrompt = function(dialog, msg = "") {
  assert.that(
    d => d === null || typeof d == "undefined",
    msg,
    lazy.error.UnexpectedAlertOpenError
  )(dialog);
};

/**
 * Asserts that <var>obj</var> is defined.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {?}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not defined.
 */
assert.defined = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be defined`;
  return assert.that(o => typeof o != "undefined", msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a finite number.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a number.
 */
assert.number = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be finite number`;
  return assert.that(Number.isFinite, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a positive number.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a positive integer.
 */
assert.positiveNumber = function(obj, msg = "") {
  assert.number(obj, msg);
  msg = msg || lazy.pprint`Expected ${obj} to be >= 0`;
  return assert.that(n => n >= 0, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a number in the inclusive range <var>lower</var> to <var>upper</var>.
 *
 * @param {?} obj
 *     Value to test.
 * @param {Array<number>} Range
 *     Array range [lower, upper]
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a number in the specified range.
 */
assert.numberInRange = function(obj, range, msg = "") {
  const [lower, upper] = range;
  assert.number(obj, msg);
  msg = msg || lazy.pprint`Expected ${obj} to be >= ${lower} and <= ${upper}`;
  return assert.that(n => n >= lower && n <= upper, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is callable.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Function}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not callable.
 */
assert.callable = function(obj, msg = "") {
  msg = msg || lazy.pprint`${obj} is not callable`;
  return assert.that(o => typeof o == "function", msg)(obj);
};

/**
 * Asserts that <var>obj</var> is an unsigned short number.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not an unsigned short.
 */
assert.unsignedShort = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be >= 0 and < 65536`;
  return assert.that(n => n >= 0 && n < 65536, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is an integer.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not an integer.
 */
assert.integer = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be an integer`;
  return assert.that(Number.isSafeInteger, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a positive integer.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a positive integer.
 */
assert.positiveInteger = function(obj, msg = "") {
  assert.integer(obj, msg);
  msg = msg || lazy.pprint`Expected ${obj} to be >= 0`;
  return assert.that(n => n >= 0, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is an integer in the inclusive range <var>lower</var> to <var>upper</var>.
 *
 * @param {?} obj
 *     Value to test.
 * @param {Array<number>} Range
 *     Array range [lower, upper]
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {number}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a number in the specified range.
 */
assert.integerInRange = function(obj, range, msg = "") {
  const [lower, upper] = range;
  assert.integer(obj, msg);
  msg = msg || lazy.pprint`Expected ${obj} to be >= ${lower} and <= ${upper}`;
  return assert.that(n => n >= lower && n <= upper, msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a boolean.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {boolean}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a boolean.
 */
assert.boolean = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be boolean`;
  return assert.that(b => typeof b == "boolean", msg)(obj);
};

/**
 * Asserts that <var>obj</var> is a string.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not a string.
 */
assert.string = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be a string`;
  return assert.that(s => typeof s == "string", msg)(obj);
};

/**
 * Asserts that <var>obj</var> is an object.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Object}
 *     obj| is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not an object.
 */
assert.object = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be an object`;
  return assert.that(o => {
    // unable to use instanceof because LHS and RHS may come from
    // different globals
    let s = Object.prototype.toString.call(o);
    return s == "[object Object]" || s == "[object nsJSIID]";
  }, msg)(obj);
};

/**
 * Asserts that <var>prop</var> is in <var>obj</var>.
 *
 * @param {?} prop
 *     An array element or own property to test if is in <var>obj</var>.
 * @param {?} obj
 *     An array or an Object that is being tested.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {?}
 *     The array element, or the value of <var>obj</var>'s own property
 *     <var>prop</var>.
 *
 * @throws {InvalidArgumentError}
 *     If the <var>obj</var> was an array and did not contain <var>prop</var>.
 *     Otherwise if <var>prop</var> is not in <var>obj</var>, or <var>obj</var>
 *     is not an object.
 */
assert.in = function(prop, obj, msg = "") {
  if (Array.isArray(obj)) {
    assert.that(p => obj.includes(p), msg)(prop);
    return prop;
  }
  assert.object(obj, msg);
  msg = msg || lazy.pprint`Expected ${prop} in ${obj}`;
  assert.that(p => obj.hasOwnProperty(p), msg)(prop);
  return obj[prop];
};

/**
 * Asserts that <var>obj</var> is an Array.
 *
 * @param {?} obj
 *     Value to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {Object}
 *     <var>obj</var> is returned unaltered.
 *
 * @throws {InvalidArgumentError}
 *     If <var>obj</var> is not an Array.
 */
assert.array = function(obj, msg = "") {
  msg = msg || lazy.pprint`Expected ${obj} to be an Array`;
  return assert.that(Array.isArray, msg)(obj);
};

/**
 * Returns a function that is used to assert the |predicate|.
 *
 * @param {function(?): boolean} predicate
 *     Evaluated on calling the return value of this function.  If its
 *     return value of the inner function is false, <var>error</var>
 *     is thrown with <var>message</var>.
 * @param {string=} message
 *     Custom error message.
 * @param {Error=} error
 *     Custom error type by its class.
 *
 * @return {function(?): ?}
 *     Function that takes and returns the passed in value unaltered,
 *     and which may throw <var>error</var> with <var>message</var>
 *     if <var>predicate</var> evaluates to false.
 */
assert.that = function(
  predicate,
  message = "",
  err = lazy.error.InvalidArgumentError
) {
  return obj => {
    if (!predicate(obj)) {
      throw new err(message);
    }
    return obj;
  };
};
