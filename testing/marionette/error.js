/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

const ERRORS = new Set([
  "ElementClickInterceptedError",
  "ElementNotAccessibleError",
  "ElementNotInteractableError",
  "InsecureCertificateError",
  "InvalidArgumentError",
  "InvalidCookieDomainError",
  "InvalidElementStateError",
  "InvalidSelectorError",
  "InvalidSessionIDError",
  "JavaScriptError",
  "MoveTargetOutOfBoundsError",
  "NoAlertOpenError",
  "NoSuchElementError",
  "NoSuchFrameError",
  "NoSuchWindowError",
  "ScriptTimeoutError",
  "SessionNotCreatedError",
  "StaleElementReferenceError",
  "TimeoutError",
  "UnableToSetCookieError",
  "UnexpectedAlertOpenError",
  "UnknownCommandError",
  "UnknownError",
  "UnsupportedOperationError",
  "WebDriverError",
]);

const BUILTIN_ERRORS = new Set([
  "Error",
  "EvalError",
  "InternalError",
  "RangeError",
  "ReferenceError",
  "SyntaxError",
  "TypeError",
  "URIError",
]);

this.EXPORTED_SYMBOLS = ["error", "error.pprint"].concat(Array.from(ERRORS));

/** @namespace */
this.error = {};

/**
 * Check if |val| is an instance of the |Error| prototype.
 *
 * Because error objects may originate from different globals, comparing
 * the prototype of the left hand side with the prototype property from
 * the right hand side, which is what |instanceof| does, will not work.
 * If the LHS and RHS come from different globals, this check will always
 * fail because the two objects will not have the same identity.
 *
 * Therefore it is not safe to use |instanceof| in any multi-global
 * situation, e.g. in content across multiple Window objects or anywhere
 * in chrome scope.
 *
 * This function also contains a special check if |val| is an XPCOM
 * |nsIException| because they are special snowflakes and may indeed
 * cause Firefox to crash if used with |instanceof|.
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
  }

  // DOMRectList errors on string comparison
  try {
    let proto = Object.getPrototypeOf(val);
    return BUILTIN_ERRORS.has(proto.toString());
  } catch (e) {
    return false;
  }
};

/**
 * Checks if obj is an object in the WebDriverError prototypal chain.
 */
error.isWebDriverError = function(obj) {
  return error.isError(obj) &&
      ("name" in obj && ERRORS.has(obj.name));
};

/**
 * Ensures error instance is a WebDriverError.
 *
 * If the given error is already in the WebDriverError prototype
 * chain, |err| is returned unmodified.  If it is not, it is wrapped
 * in UnknownError.
 *
 * @param {Error} err
 *     Error to conditionally turn into a WebDriverError.
 *
 * @return {WebDriverError}
 *     If |err| is a WebDriverError, it is returned unmodified.
 *     Otherwise an UnknownError type is returned.
 */
error.wrap = function(err) {
  if (error.isWebDriverError(err)) {
    return err;
  }
  return new UnknownError(err);
};

/**
 * Unhandled error reporter.  Dumps the error and its stacktrace to console,
 * and reports error to the Browser Console.
 */
error.report = function(err) {
  let msg = "Marionette threw an error: " + error.stringify(err);
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
 * Pretty-print values passed to template strings.
 *
 * Usage:
 *
 *     const {pprint} = Cu.import("chrome://marionette/content/error.js", {});
 *     let bool = {value: true};
 *     pprint`Expected boolean, got ${bool}`;
 *     => 'Expected boolean, got [object Object] {"value": true}'
 *
 *     let htmlElement = document.querySelector("input#foo");
 *     pprint`Expected element ${htmlElement}`;
 *     => 'Expected element <input id="foo" class="bar baz">'
 */
error.pprint = function(ss, ...values) {
  function prettyObject(obj) {
    let proto = Object.prototype.toString.call(obj);
    let s = "";
    try {
      s = JSON.stringify(obj);
    } catch (e) {
      if (e instanceof TypeError) {
        s = `<${e.message}>`;
      } else {
        throw e;
      }
    }
    return proto + " " + s;
  }

  function prettyElement(el) {
    let ident = [];
    if (el.id) {
      ident.push(`id="${el.id}"`);
    }
    if (el.classList.length > 0) {
      ident.push(`class="${el.className}"`);
    }

    let idents = "";
    if (ident.length > 0) {
      idents = " " + ident.join(" ");
    }

    return `<${el.localName}${idents}>`;
  }

  let res = [];
  for (let i = 0; i < ss.length; i++) {
    res.push(ss[i]);
    if (i < values.length) {
      let val = values[i];
      let s;
      try {
        if (val && val.nodeType === 1) {
          s = prettyElement(val);
        } else {
          s = prettyObject(val);
        }
      } catch (e) {
        s = typeof val;
      }
      res.push(s);
    }
  }
  return res.join("");
};

/**
 * WebDriverError is the prototypal parent of all WebDriver errors.
 * It should not be used directly, as it does not correspond to a real
 * error in the specification.
 */
class WebDriverError extends Error {
  /**
   * @param {(string|Error)=} x
   *     Optional string describing error situation or Error instance
   *     to propagate.
   */
  constructor(x) {
    super(x);
    this.name = this.constructor.name;
    this.status = "webdriver error";

    // Error's ctor does not preserve x' stack
    if (error.isError(x)) {
      this.stack = x.stack;
    }
  }

  /**
   * @return {Object.<string, string>}
   *     JSON serialisation of error prototype.
   */
  toJSON() {
    return {
      error: this.status,
      message: this.message || "",
      stacktrace: this.stack || "",
    }
  }

  /**
   * Unmarshals a JSON error representation to the appropriate Marionette
   * error type.
   *
   * @param {Object.<string, string>} json
   *     Error object.
   *
   * @return {Error}
   *     Error prototype.
   */
  static fromJSON(json) {
    if (typeof json.error == "undefined") {
      let s = JSON.stringify(json);
      throw new TypeError("Undeserialisable error type: " + s);
    }
    if (!STATUSES.has(json.error)) {
      throw new TypeError("Not of WebDriverError descent: " + json.error);
    }

    let cls = STATUSES.get(json.error);
    let err = new cls();
    if ("message" in json) {
      err.message = json.message;
    }
    if ("stacktrace" in json) {
      err.stack = json.stacktrace;
    }
    return err;
  }
}

/** The Gecko a11y API indicates that the element is not accessible. */
class ElementNotAccessibleError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "element not accessible";
  }
}

/**
 * An element click could not be completed because the element receiving
 * the events is obscuring the element that was requested clicked.
 *
 * @param {Element=} obscuredEl
 *     Element obscuring the element receiving the click.  Providing this
 *     is not required, but will produce a nicer error message.
 * @param {Map.<string, number>} coords
 *     Original click location.  Providing this is not required, but
 *     will produce a nicer error message.
 */
class ElementClickInterceptedError extends WebDriverError {
  constructor(obscuredEl = undefined, coords = undefined) {
    let msg = "";
    if (obscuredEl && coords) {
      const doc = obscuredEl.ownerDocument;
      const overlayingEl = doc.elementFromPoint(coords.x, coords.y);

      switch (obscuredEl.style.pointerEvents) {
        case "none":
          msg = error.pprint`Element ${obscuredEl} is not clickable ` +
              `at point (${coords.x},${coords.y}) ` +
              `because it does not have pointer events enabled, ` +
              error.pprint`and element ${overlayingEl} ` +
              `would receive the click instead`;
          break;

        default:
          msg = error.pprint`Element ${obscuredEl} is not clickable ` +
              `at point (${coords.x},${coords.y}) ` +
              error.pprint`because another element ${overlayingEl} ` +
              `obscures it`;
          break;
      }
    }

    super(msg);
    this.status = "element click intercepted";
  }
}

/**
 * A command could not be completed because the element is not pointer-
 * or keyboard interactable.
 */
class ElementNotInteractableError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "element not interactable";
  }
}

/**
 * Navigation caused the user agent to hit a certificate warning, which
 * is usually the result of an expired or invalid TLS certificate.
 */
class InsecureCertificateError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "insecure certificate";
  }
}

/** The arguments passed to a command are either invalid or malformed. */
class InvalidArgumentError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid argument";
  }
}

class InvalidCookieDomainError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid cookie domain";
  }
}

class InvalidElementStateError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid element state";
  }
}

class InvalidSelectorError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid selector";
  }
}

class InvalidSessionIDError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid session id";
  }
}

/**
 * Creates a richly annotated error for an error situation that occurred
 * whilst evaluating injected scripts.
 */
class JavaScriptError extends WebDriverError {
  /**
   * @param {(string|Error)} x
   *     An Error object instance or a string describing the error
   *     situation.
   * @param {string=} fnName
   *     Name of the function to use in the stack trace message.
   * @param {string=} file
   *     Filename of the test file on the client.
   * @param {number=} line
   *     Line number of |file|.
   * @param {string=} script
   *     Script being executed, in text form.
   */
  constructor(x,
      {fnName = null, file = null, line = null, script = null} = {}) {
    let msg = String(x);
    let trace = "";

    if (fnName !== null) {
      trace += fnName;
      if (file !== null) {
        trace += ` @${file}`;
        if (line !== null) {
          trace += `, line ${line}`;
        }
      }
    }

    if (error.isError(x)) {
      let jsStack = x.stack.split("\n");
      let match = jsStack[0].match(/:(\d+):\d+$/);
      let jsLine = match ? parseInt(match[1]) : 0;
      if (script !== null) {
        let src = script.split("\n")[jsLine];
        trace += "\n" +
          `inline javascript, line ${jsLine}\n` +
          `src: "${src}"`;
      }
      trace += "\nStack:\n" + x.stack;
    }

    super(msg);
    this.status = "javascript error";
    this.stack = trace;
  }
}

class MoveTargetOutOfBoundsError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "move target out of bounds";
  }
}

class NoAlertOpenError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such alert";
  }
}

class NoSuchElementError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such element";
  }
}

class NoSuchFrameError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such frame";
  }
}

class NoSuchWindowError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such window";
  }
}

class ScriptTimeoutError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "script timeout";
  }
}

class SessionNotCreatedError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "session not created";
  }
}

class StaleElementReferenceError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "stale element reference";
  }
}

class TimeoutError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "timeout";
  }
}

class UnableToSetCookieError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unable to set cookie";
  }
}

class UnexpectedAlertOpenError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unexpected alert open";
  }
}

class UnknownCommandError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unknown command";
  }
}

class UnknownError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unknown error";
  }
}

class UnsupportedOperationError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unsupported operation";
  }
}

const STATUSES = new Map([
  ["element click intercepted", ElementClickInterceptedError],
  ["element not accessible", ElementNotAccessibleError],
  ["element not interactable", ElementNotInteractableError],
  ["insecure certificate", InsecureCertificateError],
  ["invalid argument", InvalidArgumentError],
  ["invalid cookie domain", InvalidCookieDomainError],
  ["invalid element state", InvalidElementStateError],
  ["invalid selector", InvalidSelectorError],
  ["invalid session id", InvalidSessionIDError],
  ["javascript error", JavaScriptError],
  ["move target out of bounds", MoveTargetOutOfBoundsError],
  ["no alert open", NoAlertOpenError],
  ["no such element", NoSuchElementError],
  ["no such frame", NoSuchFrameError],
  ["no such window", NoSuchWindowError],
  ["script timeout", ScriptTimeoutError],
  ["session not created", SessionNotCreatedError],
  ["stale element reference", StaleElementReferenceError],
  ["timeout", TimeoutError],
  ["unable to set cookie", UnableToSetCookieError],
  ["unexpected alert open", UnexpectedAlertOpenError],
  ["unknown command", UnknownCommandError],
  ["unknown error", UnknownError],
  ["unsupported operation", UnsupportedOperationError],
  ["webdriver error", WebDriverError],
]);

// Errors must be expored on the local this scope so that the
// EXPORTED_SYMBOLS and the Cu.import("foo", {}) machinery sees them.
// We could assign each error definition directly to |this|, but
// because they are Error prototypes this would mess up their names.
for (let cls of STATUSES.values()) {
  this[cls.name] = cls;
}
