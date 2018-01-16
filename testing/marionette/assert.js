/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {
  InvalidArgumentError,
  InvalidSessionIDError,
  JavaScriptError,
  NoSuchWindowError,
  UnexpectedAlertOpenError,
  UnsupportedOperationError,
} = Cu.import("chrome://marionette/content/error.js", {});
const {pprint} = Cu.import("chrome://marionette/content/format.js", {});

XPCOMUtils.defineLazyGetter(this, "browser", () => {
  const {browser} = Cu.import("chrome://marionette/content/browser.js", {});
  return browser;
});

this.EXPORTED_SYMBOLS = ["assert"];

const isFennec = () => AppConstants.platform == "android";
const isFirefox = () =>
    Services.appinfo.ID == "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

/**
 * Shorthands for common assertions made in Marionette.
 *
 * @namespace
 */
this.assert = {};

/**
 * Asserts that an arbitrary object, <var>obj</var> is not acyclic.
 *
 * @param {*} obj
 *     Object test.  This assertion is only meaningful if passed
 *     an actual object or array.
 * @param {Error=} [error=JavaScriptError] error
 *     Error to throw if assertion fails.
 * @param {string=} message
 *     Message to use for <var>error</var> if assertion fails.  By default
 *     it will use the error message provided by
 *     <code>JSON.stringify</code>.
 *
 * @throws {JavaScriptError}
 *     If <var>obj</var> is cyclic.
 */
assert.acyclic = function(obj, msg = "", error = JavaScriptError) {
  try {
    JSON.stringify(obj);
  } catch (e) {
    throw new error(msg || e);
  }
};

/**
 * Asserts that Marionette has a session.
 *
 * @param {GeckoDriver} driver
 *     Marionette driver instance.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {string}
 *     Current session's ID.
 *
 * @throws {InvalidSessionIDError}
 *     If <var>driver</var> does not have a session ID.
 */
assert.session = function(driver, msg = "") {
  assert.that(sessionID => sessionID,
      msg, InvalidSessionIDError)(driver.sessionID);
  return driver.sessionID;
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
assert.fennec = function(msg = "") {
  msg = msg || "Only supported in Fennec";
  assert.that(isFennec, msg, UnsupportedOperationError)();
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
  assert.that(c => c.toString() == "content", msg, UnsupportedOperationError)(context);
};

/**
 * Asserts that the {@link ChromeWindow} is open or that the {@link
 * browser.Context} has a content browser attached.
 *
 * When passed in a {@link ChromeContext} this is equivalent to
 * testing that the associated <code>window</code> global is open,
 * and when given {@link browser.Context} it will test that the content
 * frame, represented by <code>&lt;xul:browser&gt;</code>, is
 * connected.
 *
 * @param {(ChromeWindow|browser.Context)} context
 *     Browsing context to test.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {(ChromeWindow|browser.Context)}
 *     <var>context</var> is returned unaltered.
 *
 * @throws {NoSuchWindowError}
 *     If <var>context</var>'s <code>window</code> has been closed.
 */
assert.open = function(context, msg = "") {
  // TODO: The contentBrowser uses a cached tab, which is only updated when
  // switchToTab is called. Because of that an additional check is needed to
  // make sure that the chrome window has not already been closed.
  if (context instanceof browser.Context) {
    assert.open(context.window);
  }

  msg = msg || "Browsing context has been discarded";
  return assert.that(ctx => ctx && !ctx.closed,
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
assert.noUserPrompt = function(dialog, msg = "") {
  assert.that(d => d === null || typeof d == "undefined",
      msg,
      UnexpectedAlertOpenError)(dialog);
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
  msg = msg || pprint`Expected ${obj} to be defined`;
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
  msg = msg || pprint`Expected ${obj} to be finite number`;
  return assert.that(Number.isFinite, msg)(obj);
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
  msg = msg || pprint`${obj} is not callable`;
  return assert.that(o => typeof o == "function", msg)(obj);
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
  msg = msg || pprint`Expected ${obj} to be an integer`;
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
  msg = msg || pprint`Expected ${obj} to be >= 0`;
  return assert.that(n => n >= 0, msg)(obj);
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
  msg = msg || pprint`Expected ${obj} to be boolean`;
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
  msg = msg || pprint`Expected ${obj} to be a string`;
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
  msg = msg || pprint`Expected ${obj} to be an object`;
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
 *     Own property to test if is in <var>obj</var>.
 * @param {?} obj
 *     Object.
 * @param {string=} msg
 *     Custom error message.
 *
 * @return {?}
 *     Value of <var>obj</var>'s own property <var>prop</var>.
 *
 * @throws {InvalidArgumentError}
 *     If <var>prop</var> is not in <var>obj</var>, or <var>obj</var>
 *     is not an object.
 */
assert.in = function(prop, obj, msg = "") {
  assert.object(obj, msg);
  msg = msg || pprint`Expected ${prop} in ${obj}`;
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
  msg = msg || pprint`Expected ${obj} to be an Array`;
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
    predicate, message = "", error = InvalidArgumentError) {
  return obj => {
    if (!predicate(obj)) {
      throw new error(message);
    }
    return obj;
  };
};
