/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

const ERRORS = [
  "ElementNotAccessibleError",
  "ElementNotVisibleError",
  "InvalidArgumentError",
  "InvalidElementStateError",
  "InvalidSelectorError",
  "InvalidSessionIdError",
  "JavaScriptError",
  "NoAlertOpenError",
  "NoSuchElementError",
  "NoSuchFrameError",
  "NoSuchWindowError",
  "ScriptTimeoutError",
  "SessionNotCreatedError",
  "StaleElementReferenceError",
  "TimeoutError",
  "UnableToSetCookieError",
  "UnknownCommandError",
  "UnknownError",
  "UnsupportedOperationError",
  "WebDriverError",
];

this.EXPORTED_SYMBOLS = ["error"].concat(ERRORS);

this.error = {};

error.BuiltinErrors = {
  Error: 0,
  EvalError: 1,
  InternalError: 2,
  RangeError: 3,
  ReferenceError: 4,
  SyntaxError: 5,
  TypeError: 6,
  URIError: 7,
};

/**
 * Checks if obj is an instance of the Error prototype in a safe manner.
 * Prefer using this over using instanceof since the Error prototype
 * isn't unique across browsers, and XPCOM nsIException's are special
 * snowflakes.
 *
 * @param {*} val
 *     Any value that should be undergo the test for errorness.
 * @return {boolean}
 *     True if error, false otherwise.
 */
error.isError = function(val) {
  if (val === null || typeof val != "object") {
    return false;
  } else if (val instanceof Ci.nsIException) {
    return true;
  } else {
    return Object.getPrototypeOf(val) in error.BuiltinErrors;
  }
};

/**
 * Checks if obj is an object in the WebDriverError prototypal chain.
 */
error.isWebDriverError = function(obj) {
  return error.isError(obj) &&
      ("name" in obj && ERRORS.indexOf(obj.name) >= 0);
};

/**
 * Wraps an Error prototype in a WebDriverError.  If the given error is
 * already a WebDriverError, this is effectively a no-op.
 */
error.wrap = function(err) {
  if (error.isWebDriverError(err)) {
    return err;
  }
  return new WebDriverError(`${err.name}: ${err.message}`, err.stack);
};

/**
 * Wraps an Error as a WebDriverError type.  If the given error is already
 * in the WebDriverError prototype chain, this function acts as a no-op.
 */
error.wrap = function(err) {
  if (error.isWebDriverError(err)) {
    return err;
  }
  return new WebDriverError(err.message, err.stacktrace);
};

/**
 * Unhandled error reporter.  Dumps the error and its stacktrace to console,
 * and reports error to the Browser Console.
 */
error.report = function(err) {
  let msg = `Marionette threw an error: ${error.stringify(err)}`;
  dump(msg + "\n");
  if (Cu.reportError) {
    Cu.reportError(msg);
  }
};

/**
 * Prettifies an instance of Error and its stacktrace to a string.
 */
error.stringify = function(err) {
  try {
    let s = err.toString();
    if ("stack" in err) {
      s += "\n" + err.stack;
    }
    return s;
  } catch (e) {
    return "<unprintable error>";
  }
};

/**
 * Marshal a WebDriverError prototype to a JSON dictionary.
 *
 * @param {WebDriverError} err
 *     Error to serialise.
 *
 * @return {Object.<string, Object>}
 *     JSON dictionary with the keys "error", "message", and "stacktrace".
 * @throws {TypeError}
 *     If error type is not serialisable.
 */
error.toJson = function(err) {
  if (!error.isWebDriverError(err)) {
    throw new TypeError(`Unserialisable error type: ${err}`);
  }

  let json = {
    error: err.status,
    message: err.message || "",
    stacktrace: err.stack || "",
  };
  return json;
};

/**
 * Unmarshal a JSON dictionary to a WebDriverError prototype.
 *
 * @param {Object.<string, string>} json
 *     JSON dictionary with the keys "error", "message", and "stacktrace".
 *
 * @return {WebDriverError}
 *     Deserialised error prototype.
 */
error.fromJson = function(json) {
  if (!statusLookup.has(json.error)) {
    throw new TypeError(`Undeserialisable error type: ${json.error}`);
  }

  let errCls = statusLookup.get(json.error);
  let err = new errCls(json.message);
  if ("stacktrace" in json) {
    err.stack = json.stacktrace;
  }
  return err;
};

/**
 * WebDriverError is the prototypal parent of all WebDriver errors.
 * It should not be used directly, as it does not correspond to a real
 * error in the specification.
 */
this.WebDriverError = function(msg, stack = undefined) {
  Error.call(this, msg);
  this.name = "WebDriverError";
  this.message = msg;
  this.stack = stack;
  this.status = "webdriver error";
  this.stack = stack;
};
WebDriverError.prototype = Object.create(Error.prototype);

this.ElementNotAccessibleError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ElementNotAccessibleError";
  this.status = "element not accessible";
};
ElementNotAccessibleError.prototype = Object.create(WebDriverError.prototype);

this.ElementNotVisibleError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ElementNotVisibleError";
  this.status = "element not visible";
};
ElementNotVisibleError.prototype = Object.create(WebDriverError.prototype);

this.InvalidArgumentError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidArgumentError";
  this.status = "invalid argument";
};
InvalidArgumentError.prototype = Object.create(WebDriverError.prototype);

this.InvalidElementStateError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidElementStateError";
  this.status = "invalid element state";
};
InvalidElementStateError.prototype = Object.create(WebDriverError.prototype);

this.InvalidSelectorError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidSelectorError";
  this.status = "invalid selector";
};
InvalidSelectorError.prototype = Object.create(WebDriverError.prototype);

this.InvalidSessionIdError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidSessionIdError";
  this.status = "invalid session id";
};
InvalidSessionIdError.prototype = Object.create(WebDriverError.prototype);

/**
 * Creates an error message for a JavaScript error thrown during
 * executeScript or executeAsyncScript.
 *
 * @param {Error} err
 *     An Error object passed to a catch block or a message.
 * @param {string=} fnName
 *     The name of the function to use in the stack trace message
 *     (e.g. execute_script).
 * @param {string=} file
 *     The filename of the test file containing the Marionette
 *     command that caused this error to occur.
 * @param {number=} line
 *     The line number of the above test file.
 * @param {string=} script
 *     The JS script being executed in text form.
 */
this.JavaScriptError = function(
    err, fnName = null, file = null, line = null, script = null) {
  let msg = String(err);
  let trace = "";

  if (fnName) {
    trace += fnName;
    if (file) {
      trace += ` @${file}`;
      if (line) {
        trace += `, line ${line}`;
      }
    }
  }

  if (typeof err == "object" && "name" in err && "stack" in err) {
    let jsStack = err.stack.split("\n");
    let match = jsStack[0].match(/:(\d+):\d+$/);
    let jsLine = match ? parseInt(match[1]) : 0;
    if (script) {
      let src = script.split("\n")[jsLine];
      trace += "\n" +
        "inline javascript, line " + jsLine + "\n" +
        "src: \"" + src + "\"";
    }
    trace += "\nStack:\n" + String(err.stack);
  }

  WebDriverError.call(this, msg);
  this.name = "JavaScriptError";
  this.status = "javascript error";
  this.stack = trace;
};
JavaScriptError.prototype = Object.create(WebDriverError.prototype);

this.NoAlertOpenError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoAlertOpenError";
  this.status = "no such alert";
};
NoAlertOpenError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchElementError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchElementError";
  this.status = "no such element";
};
NoSuchElementError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchFrameError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchFrameError";
  this.status = "no such frame";
};
NoSuchFrameError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchWindowError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchWindowError";
  this.status = "no such window";
};
NoSuchWindowError.prototype = Object.create(WebDriverError.prototype);

this.ScriptTimeoutError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ScriptTimeoutError";
  this.status = "script timeout";
};
ScriptTimeoutError.prototype = Object.create(WebDriverError.prototype);

this.SessionNotCreatedError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "SessionNotCreatedError";
  this.status = "session not created";
};
SessionNotCreatedError.prototype = Object.create(WebDriverError.prototype);

this.StaleElementReferenceError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "StaleElementReferenceError";
  this.status = "stale element reference";
};
StaleElementReferenceError.prototype = Object.create(WebDriverError.prototype);

this.TimeoutError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "TimeoutError";
  this.status = "timeout";
};
TimeoutError.prototype = Object.create(WebDriverError.prototype);

this.UnableToSetCookieError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnableToSetCookieError";
  this.status = "unable to set cookie";
};
UnableToSetCookieError.prototype = Object.create(WebDriverError.prototype);

this.UnknownCommandError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnknownCommandError";
  this.status = "unknown command";
};
UnknownCommandError.prototype = Object.create(WebDriverError.prototype);

this.UnknownError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnknownError";
  this.status = "unknown error";
};
UnknownError.prototype = Object.create(WebDriverError.prototype);

this.UnsupportedOperationError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnsupportedOperationError";
  this.status = "unsupported operation";
};
UnsupportedOperationError.prototype = Object.create(WebDriverError.prototype);

const nameLookup = new Map();
const statusLookup = new Map();
for (let s of ERRORS) {
  let cls = this[s];
  let inst = new cls();
  nameLookup.set(inst.name, cls);
  statusLookup.set(inst.status, cls);
};
