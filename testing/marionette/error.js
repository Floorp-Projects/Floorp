/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

const errors = [
  "ElementNotAccessibleError",
  "ElementNotVisibleError",
  "FrameSendFailureError",
  "FrameSendNotInitializedError",
  "IllegalArgumentError",
  "InvalidElementStateError",
  "InvalidSelectorError",
  "JavaScriptError",
  "NoAlertOpenError",
  "NoSuchElementError",
  "NoSuchFrameError",
  "NoSuchWindowError",
  "ScriptTimeoutError",
  "SessionNotCreatedError",
  "StaleElementReferenceError",
  "TimeoutError",
  "UnknownCommandError",
  "UnknownError",
  "UnsupportedOperationError",
  "WebDriverError",
];

this.EXPORTED_SYMBOLS = ["error"].concat(errors);

this.error = {};

error.toJSON = function(err) {
  return {
    message: err.message,
    stacktrace: err.stack || null,
    status: err.code
  };
};

/**
 * Determines if the given status code is successful.
 */
error.isSuccess = code => code === 0;

/**
 * Old-style errors are objects that has all of the properties
 * "message", "code", and "stack".
 *
 * When listener.js starts forwarding real errors by CPOW
 * we can remove this.
 */
let isOldStyleError = function(obj) {
  return typeof obj == "object" &&
    ["message", "code", "stack"].every(c => obj.hasOwnProperty(c));
}

/**
 * Checks if obj is an instance of the Error prototype in a safe manner.
 * Prefer using this over using instanceof since the Error prototype
 * isn't unique across browsers, and XPCOM exceptions are special
 * snowflakes.
 */
error.isError = function(obj) {
  if (obj === null || typeof obj != "object") {
    return false;
  // XPCOM exception.
  // Object.getPrototypeOf(obj).result throws error,
  // consequently we must do the check for properties in its
  // prototypal chain (using in, instead of obj.hasOwnProperty) here.
  } else if ("result" in obj) {
    return true;
  } else {
    return Object.getPrototypeOf(obj) == "Error" || isOldStyleError(obj);
  }
};

/**
 * Checks if obj is an object in the WebDriverError prototypal chain.
 */
error.isWebDriverError = function(obj) {
  return error.isError(obj) &&
    (("name" in obj && errors.indexOf(obj.name) > 0) ||
      isOldStyleError(obj));
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
  this.code = 500;  // overridden
};
WebDriverError.prototype = Object.create(Error.prototype);

this.ElementNotAccessibleError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ElementNotAccessibleError";
  this.status = "element not accessible";
  this.code = 56;
};
ElementNotAccessibleError.prototype = Object.create(WebDriverError.prototype);

this.ElementNotVisibleError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ElementNotVisibleError";
  this.status = "element not visible";
  this.code = 11;
};
ElementNotVisibleError.prototype = Object.create(WebDriverError.prototype);

this.FrameSendFailureError = function(frame) {
  this.message = "Error sending message to frame (NS_ERROR_FAILURE)";
  WebDriverError.call(this, this.message);
  this.name = "FrameSendFailureError";
  this.status = "frame send failure error";
  this.code = 55;
  this.frame = frame;
  this.errMsg = `${this.message} ${this.frame}; frame not responding.`;
};
FrameSendFailureError.prototype = Object.create(WebDriverError.prototype);

this.FrameSendNotInitializedError = function(frame) {
  this.message = "Error sending message to frame (NS_ERROR_NOT_INITIALIZED)";
  WebDriverError.call(this, this.message);
  this.name = "FrameSendNotInitializedError";
  this.status = "frame send not initialized error";
  this.code = 54;
  this.frame = frame;
  this.errMsg = `${this.message} ${this.frame}; frame has closed.`;
};
FrameSendNotInitializedError.prototype = Object.create(WebDriverError.prototype);

this.IllegalArgumentError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "IllegalArgumentError";
  this.status = "illegal argument";
  this.code = 13;  // unknown error
};
IllegalArgumentError.prototype = Object.create(WebDriverError.prototype);

this.InvalidElementStateError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidElementStateError";
  this.status = "invalid element state";
  this.code = 12;
};
InvalidElementStateError.prototype = Object.create(WebDriverError.prototype);

this.InvalidSelectorError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "InvalidSelectorError";
  this.status = "invalid selector";
  this.code = 32;
};
InvalidSelectorError.prototype = Object.create(WebDriverError.prototype);

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
  }

  WebDriverError.call(this, msg);
  this.name = "JavaScriptError";
  this.status = "javascript error";
  this.code = 17;
  this.stack = trace;
};
JavaScriptError.prototype = Object.create(WebDriverError.prototype);

this.NoAlertOpenError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoAlertOpenError";
  this.status = "no such alert";
  this.code = 27;
}
NoAlertOpenError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchElementError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchElementError";
  this.status = "no such element";
  this.code = 7;
};
NoSuchElementError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchFrameError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchFrameError";
  this.status = "no such frame";
  this.code = 8;
};
NoSuchFrameError.prototype = Object.create(WebDriverError.prototype);

this.NoSuchWindowError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "NoSuchWindowError";
  this.status = "no such window";
  this.code = 23;
};
NoSuchWindowError.prototype = Object.create(WebDriverError.prototype);

this.ScriptTimeoutError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "ScriptTimeoutError";
  this.status = "script timeout";
  this.code = 28;
};
ScriptTimeoutError.prototype = Object.create(WebDriverError.prototype);

this.SessionNotCreatedError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "SessionNotCreatedError";
  this.status = "session not created";
  // should be 33 to match Selenium
  this.code = 71;
};
SessionNotCreatedError.prototype = Object.create(WebDriverError.prototype);

this.StaleElementReferenceError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "StaleElementReferenceError";
  this.status = "stale element reference";
  this.code = 10;
};
StaleElementReferenceError.prototype = Object.create(WebDriverError.prototype);

this.TimeoutError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "TimeoutError";
  this.status = "timeout";
  this.code = 21;
};
TimeoutError.prototype = Object.create(WebDriverError.prototype);

this.UnknownCommandError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnknownCommandError";
  this.status = "unknown command";
  this.code = 9;
};
UnknownCommandError.prototype = Object.create(WebDriverError.prototype);

this.UnknownError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnknownError";
  this.status = "unknown error";
  this.code = 13;
};
UnknownError.prototype = Object.create(WebDriverError.prototype);

this.UnsupportedOperationError = function(msg) {
  WebDriverError.call(this, msg);
  this.name = "UnsupportedOperationError";
  this.status = "unsupported operation";
  this.code = 405;
};
UnsupportedOperationError.prototype = Object.create(WebDriverError.prototype);
