/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class Log extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;
  }

  destructor() {
    this.disable();

    super.destructor();
  }

  enable() {
    if (!this.enabled) {
      this.enabled = true;

      Services.console.registerListener(this);
      Services.obs.addObserver(this, "console-api-log-event");
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
      Services.console.unregisterListener(this);
      Services.obs.removeObserver(this, "console-api-log-event");
    }
  }

  // nsIObserver

  /**
   * Takes all script error messages belonging to the current window
   * and emits a "console-service-message" event.
   *
   * @param {nsIConsoleMessage} message
   *     Message originating from the nsIConsoleService.
   */
  observe(message, topic) {
    let entry;
    if (message instanceof Ci.nsIScriptError) {
      entry = fromScriptError(message);
    } else if (message instanceof Ci.nsIConsoleMessage) {
      entry = fromConsoleMessage(message);
    } else if (topic == "console-api-log-event") {
      entry = fromConsoleAPI(message.wrappedJSObject);
    }

    this.emit("Log.entryAdded", { entry });
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIConsoleListener]);
  }
}

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

function fromConsoleAPI(message) {
  // message is a ConsoleEvent instance:
  // https://searchfox.org/mozilla-central/rev/00c0d068ece99717bea7475f7dc07e61f7f35984/dom/webidl/Console.webidl#83-107

  // A couple of possible level are defined here:
  // https://searchfox.org/mozilla-central/rev/00c0d068ece99717bea7475f7dc07e61f7f35984/dom/console/Console.cpp#1086-1100
  const levels = {
    log: "verbose",
    info: "info",
    warn: "warning",
    error: "error",
    exception: "error",
  };
  const level = levels[message.level] || "info";

  return {
    source: "javascript",
    level,
    text: message.arguments,
    url: message.filename,
    lineNumber: message.lineNumber,
    stackTrace: message.stacktrace,
    timestamp: Date.now(),
  };
}

function fromScriptError(error) {
  const { flags, errorMessage, sourceName, lineNumber, stack } = error;

  // lossy reduction from bitmask to CDP string level
  let level = "verbose";
  if (
    flags & Ci.nsIScriptError.exceptionFlag ||
    flags & Ci.nsIScriptError.errorFlag
  ) {
    level = "error";
  } else if (
    flags & Ci.nsIScriptError.warningFlag ||
    flags & Ci.nsIScriptError.strictFlag
  ) {
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
