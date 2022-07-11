/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "FatalError",
  "RemoteAgentError",
  "UnknownMethodError",
  "UnsupportedError",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.CDP)
);

class RemoteAgentError extends Error {
  constructor(message = "", cause = undefined) {
    cause = cause || message;
    super(cause);

    this.name = this.constructor.name;
    this.message = message;
    this.cause = cause;

    this.notify();
  }

  notify() {
    Cu.reportError(this);
    lazy.logger.error(this.toString({ stack: true }));
  }

  toString({ stack = false } = {}) {
    return RemoteAgentError.format(this, { stack });
  }

  static format(e, { stack = false } = {}) {
    return formatError(e, { stack });
  }

  /**
   * Takes a serialised CDP error and reconstructs it
   * as a RemoteAgentError.
   *
   * The error must be of this form:
   *
   *     {"message": "TypeError: foo is not a function\n
   *     execute@chrome://remote/content/cdp/sessions/Session.jsm:73:39\n
   *     onMessage@chrome://remote/content/cdp/sessions/TabSession.jsm:65:20"}
   *
   * This approach has the notable deficiency that it cannot deal
   * with causes to errors because of the unstructured nature of CDP
   * errors.  A possible future improvement would be to extend the
   * error serialisation to include discrete fields for each data
   * property.
   *
   * @param {Object} json
   *     CDP error encoded as a JSON object, which must have a
   *     "message" field, where the first line will make out the error
   *     message and the subsequent lines the stacktrace.
   *
   * @return {RemoteAgentError}
   */
  static fromJSON(json) {
    const [message, ...stack] = json.message.split("\n");
    const err = new RemoteAgentError();
    err.message = message.slice(0, -1);
    err.stack = stack.map(s => s.trim()).join("\n");
    err.cause = null;
    return err;
  }
}

/**
 * A fatal error that it is not possible to recover from
 * or send back to the client.
 *
 * Constructing this error will force the application to quit.
 */
class FatalError extends RemoteAgentError {
  constructor(...args) {
    super(...args);
    this.quit();
  }

  notify() {
    lazy.logger.fatal(this.toString({ stack: true }));
  }

  quit(mode = Ci.nsIAppStartup.eForceQuit) {
    Services.startup.quit(mode);
  }
}

/** When an operation is not yet implemented. */
class UnsupportedError extends RemoteAgentError {}

/** The requested remote method does not exist. */
class UnknownMethodError extends RemoteAgentError {
  constructor(domain, command = null) {
    if (command) {
      super(`${domain}.${command}`);
    } else {
      super(domain);
    }
  }
}

function formatError(error, { stack = false } = {}) {
  const els = [];

  els.push(error.name);
  if (error.message) {
    els.push(": ");
    els.push(error.message);
  }

  if (stack && error.stack) {
    els.push(":\n");

    const stack = error.stack.trim().split("\n");
    els.push(stack.map(line => `\t${line}`).join("\n"));

    if (error.cause) {
      els.push("\n");
      els.push("caused by: " + formatError(error.cause, { stack }));
    }
  }

  return els.join("");
}
