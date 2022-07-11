/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm"
);

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

  _getLogCategory(category) {
    if (category.startsWith("CORS")) {
      return "network";
    } else if (category.includes("javascript")) {
      return "javascript";
    }

    return "other";
  }

  // nsIObserver

  /**
   * Takes all script error messages that do not have an exception attached,
   * and emits a "Log.entryAdded" event.
   *
   * @param {nsIConsoleMessage} message
   *     Message originating from the nsIConsoleService.
   */
  observe(message) {
    if (message instanceof Ci.nsIScriptError && !message.hasException) {
      let url;
      if (message.sourceName !== "debugger eval code") {
        url = message.sourceName;
      }

      const entry = {
        source: this._getLogCategory(message.category),
        level: CONSOLE_MESSAGE_LEVEL_MAP[message.logLevel],
        text: message.errorMessage,
        timestamp: message.timeStamp,
        url,
        lineNumber: message.lineNumber,
      };

      this.emit("Log.entryAdded", { entry });
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIConsoleListener"]);
  }
}
