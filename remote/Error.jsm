/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "FatalError",
  "UnsupportedError",
];

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

class RemoteAgentError extends Error {
  constructor(...args) {
    super(...args);
    this.notify();
  }

  notify() {
    Cu.reportError(this);
    log.error(formatError(this), this);
  }
}

/**
 * A fatal error that it is not possible to recover from
 * or send back to the client.
 *
 * Constructing this error will cause the application to quit.
 */
this.FatalError = class extends RemoteAgentError {
  constructor(...args) {
    super(...args);
    this.quit();
  }

  notify() {
    log.fatal(formatError(this, {stack: true}));
  }

  quit(mode = Ci.nsIAppStartup.eForceQuit) {
    Services.startup.quit(mode);
  }
};

this.UnsupportedError = class extends RemoteAgentError {};

function formatError(error, {stack = false} = {}) {
  const s = [];
  s.push(`${error.name}: ${error.message}`);
  s.push("");
  if (stack) {
    s.push("Stacktrace:");
  }
  s.push(error.stack);

  return s.join("\n");
}
