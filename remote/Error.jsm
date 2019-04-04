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

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

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
    log.error(this.toString({stack: true}));
  }

  toString({stack = false} = {}) {
    return RemoteAgentError.format(this, {stack});
  }

  static format(e, {stack = false} = {}) {
    return formatError(e, {stack});
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
    log.fatal(this.toString({stack: true}));
  }

  quit(mode = Ci.nsIAppStartup.eForceQuit) {
    Services.startup.quit(mode);
  }
}

/** When an operation is not yet implemented. */
class UnsupportedError extends RemoteAgentError {}

/** The requested remote method does not exist. */
class UnknownMethodError extends RemoteAgentError {}

function formatError(error, {stack = false} = {}) {
  const ls = [];

  ls.push(`${error.name}: ${error.message ? `${error.message}:` : ""}`);

  if (stack && error.stack) {
    const stack = error.stack.trim().split("\n");
    ls.push(stack.map(line => `\t${line}`).join("\n"));

    if (error.cause) {
      ls.push("caused by: " + formatError(error.cause, {stack}));
    }
  }

  return ls.join("\n");
}
