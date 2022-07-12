/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["error"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { RemoteError } = ChromeUtils.import(
  "chrome://remote/content/shared/RemoteError.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  pprint: "chrome://remote/content/shared/Format.jsm",
});

const ERRORS = new Set([
  "DetachedShadowRootError",
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
  "NoSuchAlertError",
  "NoSuchElementError",
  "NoSuchFrameError",
  "NoSuchShadowRootError",
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

/** @namespace */
const error = {
  /**
   * Check if ``val`` is an instance of the ``Error`` prototype.
   *
   * Because error objects may originate from different globals, comparing
   * the prototype of the left hand side with the prototype property from
   * the right hand side, which is what ``instanceof`` does, will not work.
   * If the LHS and RHS come from different globals, this check will always
   * fail because the two objects will not have the same identity.
   *
   * Therefore it is not safe to use ``instanceof`` in any multi-global
   * situation, e.g. in content across multiple ``Window`` objects or anywhere
   * in chrome scope.
   *
   * This function also contains a special check if ``val`` is an XPCOM
   * ``nsIException`` because they are special snowflakes and may indeed
   * cause Firefox to crash if used with ``instanceof``.
   *
   * @param {*} val
   *     Any value that should be undergo the test for errorness.
   * @return {boolean}
   *     True if error, false otherwise.
   */
  isError(val) {
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
  },

  /**
   * Checks if ``obj`` is an object in the :js:class:`WebDriverError`
   * prototypal chain.
   *
   * @param {*} obj
   *     Arbitrary object to test.
   *
   * @return {boolean}
   *     True if ``obj`` is of the WebDriverError prototype chain,
   *     false otherwise.
   */
  isWebDriverError(obj) {
    // Don't use "instanceof" to compare error objects because of possible
    // problems when the other instance was created in a different global and
    // as such won't have the same prototype object.
    return error.isError(obj) && "name" in obj && ERRORS.has(obj.name);
  },

  /**
   * Ensures error instance is a :js:class:`WebDriverError`.
   *
   * If the given error is already in the WebDriverError prototype
   * chain, ``err`` is returned unmodified.  If it is not, it is wrapped
   * in :js:class:`UnknownError`.
   *
   * @param {Error} err
   *     Error to conditionally turn into a WebDriverError.
   *
   * @return {WebDriverError}
   *     If ``err`` is a WebDriverError, it is returned unmodified.
   *     Otherwise an UnknownError type is returned.
   */
  wrap(err) {
    if (error.isWebDriverError(err)) {
      return err;
    }
    return new UnknownError(err);
  },

  /**
   * Unhandled error reporter.  Dumps the error and its stacktrace to console,
   * and reports error to the Browser Console.
   */
  report(err) {
    let msg = "Marionette threw an error: " + error.stringify(err);
    dump(msg + "\n");
    if (Cu.reportError) {
      Cu.reportError(msg);
    }
  },

  /**
   * Prettifies an instance of Error and its stacktrace to a string.
   */
  stringify(err) {
    try {
      let s = err.toString();
      if ("stack" in err) {
        s += "\n" + err.stack;
      }
      return s;
    } catch (e) {
      return "<unprintable error>";
    }
  },

  /** Create a stacktrace to the current line in the program. */
  stack() {
    let trace = new Error().stack;
    let sa = trace.split("\n");
    sa = sa.slice(1);
    let rv = "stacktrace:\n" + sa.join("\n");
    return rv.trimEnd();
  },
};

/**
 * WebDriverError is the prototypal parent of all WebDriver errors.
 * It should not be used directly, as it does not correspond to a real
 * error in the specification.
 */
class WebDriverError extends RemoteError {
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
    };
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
          msg =
            lazy.pprint`Element ${obscuredEl} is not clickable ` +
            `at point (${coords.x},${coords.y}) ` +
            `because it does not have pointer events enabled, ` +
            lazy.pprint`and element ${overlayingEl} ` +
            `would receive the click instead`;
          break;

        default:
          msg =
            lazy.pprint`Element ${obscuredEl} is not clickable ` +
            `at point (${coords.x},${coords.y}) ` +
            lazy.pprint`because another element ${overlayingEl} ` +
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

/**
 * An illegal attempt was made to set a cookie under a different
 * domain than the current page.
 */
class InvalidCookieDomainError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid cookie domain";
  }
}

/**
 * A command could not be completed because the element is in an
 * invalid state, e.g. attempting to clear an element that isn't both
 * editable and resettable.
 */
class InvalidElementStateError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid element state";
  }
}

/** Argument was an invalid selector. */
class InvalidSelectorError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid selector";
  }
}

/**
 * Occurs if the given session ID is not in the list of active sessions,
 * meaning the session either does not exist or that it's not active.
 */
class InvalidSessionIDError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "invalid session id";
  }
}

/** An error occurred whilst executing JavaScript supplied by the user. */
class JavaScriptError extends WebDriverError {
  constructor(x) {
    super(x);
    this.status = "javascript error";
  }
}

/**
 * The target for mouse interaction is not in the browser's viewport
 * and cannot be brought into that viewport.
 */
class MoveTargetOutOfBoundsError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "move target out of bounds";
  }
}

/**
 * An attempt was made to operate on a modal dialog when one was
 * not open.
 */
class NoSuchAlertError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such alert";
  }
}

/**
 * An element could not be located on the page using the given
 * search parameters.
 */
class NoSuchElementError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such element";
  }
}

/**
 * A shadow root was not attached to the element.
 */
class NoSuchShadowRootError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such shadow root";
  }
}

/**
 * A shadow root is no longer attached to the document
 */
class DetachedShadowRootError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "detached shadow root";
  }
}

/**
 * A command to switch to a frame could not be satisfied because
 * the frame could not be found.
 */
class NoSuchFrameError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such frame";
  }
}

/**
 * A command to switch to a window could not be satisfied because
 * the window could not be found.
 */
class NoSuchWindowError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "no such window";
  }
}

/** A script did not complete before its timeout expired. */
class ScriptTimeoutError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "script timeout";
  }
}

/** A new session could not be created. */
class SessionNotCreatedError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "session not created";
  }
}

/**
 * A command failed because the referenced element is no longer
 * attached to the DOM.
 */
class StaleElementReferenceError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "stale element reference";
  }
}

/** An operation did not complete before its timeout expired. */
class TimeoutError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "timeout";
  }
}

/** A command to set a cookie's value could not be satisfied. */
class UnableToSetCookieError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unable to set cookie";
  }
}

/** A modal dialog was open, blocking this operation. */
class UnexpectedAlertOpenError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unexpected alert open";
  }
}

/**
 * A command could not be executed because the remote end is not
 * aware of it.
 */
class UnknownCommandError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unknown command";
  }
}

/**
 * An unknown error occurred in the remote end while processing
 * the command.
 */
class UnknownError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unknown error";
  }
}

/**
 * Indicates that a command that should have executed properly
 * cannot be supported for some reason.
 */
class UnsupportedOperationError extends WebDriverError {
  constructor(message) {
    super(message);
    this.status = "unsupported operation";
  }
}

const STATUSES = new Map([
  ["detached shadow root", DetachedShadowRootError],
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
  ["no such alert", NoSuchAlertError],
  ["no such element", NoSuchElementError],
  ["no such frame", NoSuchFrameError],
  ["no such shadow root", NoSuchShadowRootError],
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
// EXPORTED_SYMBOLS and the ChromeUtils.import("foo") machinery sees them.
// We could assign each error definition directly to |this|, but
// because they are Error prototypes this would mess up their names.
for (let cls of STATUSES.values()) {
  error[cls.name] = cls;
}
