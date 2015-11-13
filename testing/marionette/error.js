/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {results: Cr, utils: Cu} = Components;

const errors = [
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

this.EXPORTED_SYMBOLS = ["error"].concat(errors);

// Because XPCOM is a cesspool of undocumented odd behaviour,
// Object.getPrototypeOf(err) causes another exception if err is an XPCOM
// exception, and cannot be used to determine if err is a prototypal Error.
//
// Consequently we need to check for properties in its prototypal chain
// (using in, instead of err.hasOwnProperty because that causes other
// issues).
//
// Since the input is arbitrary it might _not_ be an Error, and can as
// such be an object with a "result" property without it being considered to
// be an exception.  The solution is to build a lookup table of XPCOM
// exceptions from Components.results and check if the value of err#results
// is in that table.
const XPCOM_EXCEPTIONS = [];
{
  for (let prop in Cr) {
    XPCOM_EXCEPTIONS.push(Cr[prop]);
  }
}

this.error = {};

/**
 * Determines if the given status is successful.
 */
error.isSuccess = status => status === "success";

/**
 * Checks if obj is an instance of the Error prototype in a safe manner.
 * Prefer using this over using instanceof since the Error prototype
 * isn't unique across browsers, and XPCOM exceptions are special
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
  } else if ("result" in val && val.result in XPCOM_EXCEPTIONS) {
    return true;
  } else {
    return Object.getPrototypeOf(val) == "Error";
  }
};

/**
 * Checks if obj is an object in the WebDriverError prototypal chain.
 */
error.isWebDriverError = function(obj) {
  return error.isError(obj) &&
      ("name" in obj && errors.indexOf(obj.name) >= 0);
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
 * WebDriverError is the prototypal parent of all WebDriver errors.
 * It should not be used directly, as it does not correspond to a real
 * error in the specification.
 */
this.WebDriverError = function(msg) {
  Error.call(this, msg);
  this.name = "WebDriverError";
  this.message = msg;
  this.status = "webdriver error";
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
 * @param {string} fnName
 *     The name of the function to use in the stack trace message
 *     (e.g. execute_script).
 * @param {string} file
 *     The filename of the test file containing the Marionette
 *     command that caused this error to occur.
 * @param {number} line
 *     The line number of the above test file.
 * @param {string=} script
 *     The JS script being executed in text form.
 */
this.JavaScriptError = function(err, fnName, file, line, script) {
  let msg = String(err);
  let trace = "";

  if (fnName && line) {
    trace += `${fnName} @${file}`;
    if (line) {
      trace += `, line ${line}`;
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
}
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
