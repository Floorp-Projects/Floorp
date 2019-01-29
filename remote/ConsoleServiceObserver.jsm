/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ConsoleServiceObserver"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.ConsoleServiceObserver = class {
  constructor() {
    this.running = false;
  }

  start() {
    if (!this.running) {
      Services.console.registerListener(this);
      this.running = true;
    }
  }

  stop() {
    if (this.running) {
      Services.console.unregisterListener(this);
    }
  }

  // nsIConsoleListener

  /**
   * Takes all script error messages belonging to the current window
   * and emits a "console-service-message" event.
   *
   * @param {nsIConsoleMessage} message
   *     Message originating from the nsIConsoleService.
   */
  observe(message) {
    let entry;
    if (message instanceof Ci.nsIScriptError) {
      entry = fromScriptError(message);
    } else {
      entry = fromConsoleMessage(message);
    }

    // TODO(ato): This doesn't work for some reason:
    Services.mm.broadcastAsyncMessage("RemoteAgent:Log:OnConsoleServiceMessage", entry);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIConsoleListener]);
  }
};

function fromConsoleMessage(message) {
  const levels = {
    [Ci.nsIConsoleMessage.debug]: "verbose",
    [Ci.nsIConsoleMessage.info]: "info",
    [Ci.nsIConsoleMessage.warn]: "warning",
    [Ci.nsIConsoleMessage.error]: "error",
  };
  const level = levels[message.logLevel];

  return {
    source: "javascript",
    level,
    text: message.message,
    timestamp: Date.now(),
  };
}

function fromScriptError(error) {
  const {flags, errorMessage, sourceName, lineNumber, stack} = error;

  // lossy reduction from bitmask to CDP string level
  let level = "verbose";
  if ((flags & Ci.nsIScriptError.exceptionFlag) ||
      (flags & Ci.nsIScriptError.errorFlag)) {
    level = "error";
  } else if ((flags & Ci.nsIScriptError.warningFlag) ||
      (flags & Ci.nsIScriptError.strictFlag)) {
    level = "warning";
  } else if (flags & Ci.nsIScriptError.infoFlag) {
    level = "info";
  }

  return {
    source: "javascript",
    level,
    text: errorMessage,
    timestamp: Date.now(),
    url: sourceName,
    lineNumber,
    stackTrace: stack,
  };
}
