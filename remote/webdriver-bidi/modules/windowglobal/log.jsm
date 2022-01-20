/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ConsoleAPIListener:
    "chrome://remote/content/shared/listeners/ConsoleAPIListener.jsm",
  ConsoleListener:
    "chrome://remote/content/shared/listeners/ConsoleListener.jsm",
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
});

class Log extends Module {
  #consoleAPIListener;
  #consoleMessageListener;

  constructor(messageHandler) {
    super(messageHandler);

    // Create the console-api listener and listen on "message" events.
    this.#consoleAPIListener = new ConsoleAPIListener(
      this.messageHandler.innerWindowId
    );
    this.#consoleAPIListener.on("message", this.#onConsoleAPIMessage);

    // Create the console listener and listen on error messages.
    this.#consoleMessageListener = new ConsoleListener(
      this.messageHandler.innerWindowId
    );
    this.#consoleMessageListener.on("error", this.#onJavaScriptError);
  }

  destroy() {
    this.#consoleAPIListener.off("message", this.#onConsoleAPIMessage);
    this.#consoleAPIListener.destroy();
    this.#consoleMessageListener.off("error", this.#onJavaScriptError);
    this.#consoleMessageListener.destroy();
  }

  /**
   * Private methods
   */

  _applySessionData(params) {
    // TODO: Bug 1741861. Move this logic to a shared module or the an abstract
    // class.
    const { category, added = [], removed = [] } = params;
    if (category === "event") {
      for (const event of added) {
        this._subscribeEvent(event);
      }
      for (const event of removed) {
        this._unsubscribeEvent(event);
      }
    }
  }

  _subscribeEvent(event) {
    if (event === "log.entryAdded") {
      this.#consoleAPIListener.startListening();
      this.#consoleMessageListener.startListening();
    }
  }

  _unsubscribeEvent(event) {
    if (event === "log.entryAdded") {
      this.#consoleAPIListener.stopListening();
      this.#consoleMessageListener.stopListening();
    }
  }

  #onConsoleAPIMessage = (eventName, data = {}) => {
    // `arguments` cannot be used as variable name in functions
    // `level` corresponds to the console method used
    const { arguments: messageArguments, level: method, timeStamp } = data;

    // Step numbers below refer to the specifications at
    //   https://w3c.github.io/webdriver-bidi/#event-log-entryAdded

    // 1. Translate the console message method to a log.LogEntry level
    const logEntrylevel = this._getLogEntryLevelFromConsoleMethod(method);

    // 2. Use the message's timeStamp or fallback on the current time value.
    const timestamp = timeStamp || Date.now();

    // 3. Start assembling the text representation of the message.
    let text = "";

    // 4. Formatters have already been applied at this points.
    // message.arguments corresponds to the "formatted args" from the
    // specifications.

    // 5. Concatenate all formatted arguments in text
    // TODO: For m1 we only support string arguments, so we rely on the builtin
    // toString for each argument which will be available in message.arguments.
    const args = messageArguments || [];
    text += args.map(String).join(" ");

    // Step 6 and 7: Serialize each arg as remote value.
    const serializedArgs = [];
    for (const arg of args) {
      serializedArgs.push(serialize(arg /*, null, true, new Set() */));
    }

    // 8. TODO: set realm to the current realm id.

    // 9. TODO: set stack to the current stack trace.

    // 10. Build the ConsoleLogEntry
    const entry = {
      type: "console",
      level: logEntrylevel,
      text,
      timestamp,
      method,
      realm: null,
      args: serializedArgs,
    };

    // TODO: steps 11. to 15. Those steps relate to:
    // - emitting associated BrowsingContext. See log.entryAdded full support
    //   in https://bugzilla.mozilla.org/show_bug.cgi?id=1724669#c0
    // - handling cases where session doesn't exist or the event is not
    //   monitored. The implementation differs from the spec here because we
    //   only react to events if there is a session & if the session subscribed
    //   to those events.

    this.messageHandler.emitMessageHandlerEvent("log.entryAdded", entry);
  };

  #onJavaScriptError = (eventName, data = {}) => {
    const { level, message, timeStamp } = data;

    const entry = {
      type: "javascript",
      level,
      text: message,
      timestamp: timeStamp || Date.now(),
      // TODO: Bug 1731553
      stackTrace: undefined,
    };

    this.messageHandler.emitMessageHandlerEvent("log.entryAdded", entry);
  };

  _getLogEntryLevelFromConsoleMethod(method) {
    switch (method) {
      case "assert":
      case "error":
        return "error";
      case "debug":
      case "trace":
        return "debug";
      case "warn":
      case "warning":
        return "warning";
      default:
        return "info";
    }
  }
}

const log = Log;
