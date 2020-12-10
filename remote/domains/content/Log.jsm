/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CONSOLE_MESSAGE_LEVEL_MAP = {
  [Ci.nsIConsoleMessage.debug]: "verbose",
  [Ci.nsIConsoleMessage.info]: "info",
  [Ci.nsIConsoleMessage.warn]: "warning",
  [Ci.nsIConsoleMessage.error]: "error",
};

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
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;

      Services.console.unregisterListener(this);
    }
  }

  // nsIObserver

  /**
   * Takes all script error messages belonging to the current window.
   * and emits a "Log.entryAdded" event.
   *
   * @param {nsIConsoleMessage} message
   *     Message originating from the nsIConsoleService.
   */
  observe(message) {
    // Console messages will be handled via Runtime.consoleAPICalled
    if (
      message instanceof Ci.nsIScriptError &&
      message.flags == Ci.nsIScriptError.errorFlag
    ) {
      const entry = {
        source: "javascript",
        level: CONSOLE_MESSAGE_LEVEL_MAP[message.flags],
        lineNumber: message.lineNumber,
        stacktrace: message.stack,
        text: message.errorMessage,
        timestamp: message.timeStamp,
        url: message.sourceName,
      };

      this.emit("Log.entryAdded", { entry });
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIConsoleListener"]);
  }
}
