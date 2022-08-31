/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ConsoleAPIListener:
    "chrome://remote/content/shared/listeners/ConsoleAPIListener.jsm",
  ConsoleListener:
    "chrome://remote/content/shared/listeners/ConsoleListener.jsm",
  isChromeFrame: "chrome://remote/content/shared/Stack.jsm",
  OwnershipModel: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
});

class LogModule extends Module {
  #consoleAPIListener;
  #consoleMessageListener;

  constructor(messageHandler) {
    super(messageHandler);

    // Create the console-api listener and listen on "message" events.
    this.#consoleAPIListener = new lazy.ConsoleAPIListener(
      this.messageHandler.innerWindowId
    );
    this.#consoleAPIListener.on("message", this.#onConsoleAPIMessage);

    // Create the console listener and listen on error messages.
    this.#consoleMessageListener = new lazy.ConsoleListener(
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

  #buildSource() {
    return {
      realm: this.messageHandler.window.windowGlobalChild?.innerWindowId.toString(),
      context: this.messageHandler.context,
    };
  }

  /**
   * Map the internal stacktrace representation to a WebDriver BiDi
   * compatible one.
   *
   * Currently chrome frames will be filtered out until chrome scope
   * is supported (bug 1722679).
   *
   * @param {Array<StackFrame>=} stackTrace
   *     Stack frames to process.
   *
   * @returns {Object=} Object, containing the list of frames as `callFrames`.
   */
  #buildStackTrace(stackTrace) {
    if (stackTrace == undefined) {
      return undefined;
    }

    const callFrames = stackTrace
      .filter(frame => !lazy.isChromeFrame(frame))
      .map(frame => {
        return {
          columnNumber: frame.columnNumber,
          functionName: frame.functionName,
          // Match WebDriver BiDi and convert Firefox's one-based line number.
          lineNumber: frame.lineNumber - 1,
          url: frame.filename,
        };
      });

    return { callFrames };
  }

  #getLogEntryLevelFromConsoleMethod(method) {
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

  #onConsoleAPIMessage = (eventName, data = {}) => {
    const {
      // `arguments` cannot be used as variable name in functions
      arguments: messageArguments,
      // `level` corresponds to the console method used
      level: method,
      stacktrace,
      timeStamp,
    } = data;

    // Step numbers below refer to the specifications at
    //   https://w3c.github.io/webdriver-bidi/#event-log-entryAdded

    // Translate the console message method to a log.LogEntry level
    const logEntrylevel = this.#getLogEntryLevelFromConsoleMethod(method);

    // Use the message's timeStamp or fallback on the current time value.
    const timestamp = timeStamp || Date.now();

    // Start assembling the text representation of the message.
    let text = "";

    // Formatters have already been applied at this points.
    // message.arguments corresponds to the "formatted args" from the
    // specifications.

    // Concatenate all formatted arguments in text
    // TODO: For m1 we only support string arguments, so we rely on the builtin
    // toString for each argument which will be available in message.arguments.
    const args = messageArguments || [];
    text += args.map(String).join(" ");

    // Serialize each arg as remote value.
    const serializedArgs = [];
    for (const arg of args) {
      // Note that we can pass a `null` realm for now since realms are only
      // involved when creating object references, which will not happen with
      // OwnershipModel.None. This will be revisited in Bug 1731589.
      serializedArgs.push(
        lazy.serialize(arg, 1, lazy.OwnershipModel.None, new Map(), null)
      );
    }

    // Set source to an object which contains realm and browsing context.
    const source = this.#buildSource();

    // Set stack trace only for certain methods.
    let stackTrace;
    if (["assert", "error", "trace", "warn"].includes(method)) {
      stackTrace = this.#buildStackTrace(stacktrace);
    }

    // Build the ConsoleLogEntry
    const entry = {
      type: "console",
      method,
      source,
      args: serializedArgs,
      level: logEntrylevel,
      text,
      timestamp,
      stackTrace,
    };

    // TODO: Those steps relate to:
    // - emitting associated BrowsingContext. See log.entryAdded full support
    //   in https://bugzilla.mozilla.org/show_bug.cgi?id=1724669#c0
    // - handling cases where session doesn't exist or the event is not
    //   monitored. The implementation differs from the spec here because we
    //   only react to events if there is a session & if the session subscribed
    //   to those events.

    this.emitEvent("log.entryAdded", entry);
  };

  #onJavaScriptError = (eventName, data = {}) => {
    const { level, message, stacktrace, timeStamp } = data;

    // Build the JavascriptLogEntry
    const entry = {
      type: "javascript",
      level,
      source: this.#buildSource(),
      text: message,
      timestamp: timeStamp || Date.now(),
      stackTrace: this.#buildStackTrace(stacktrace),
    };

    this.emitEvent("log.entryAdded", entry);
  };

  #subscribeEvent(event) {
    if (event === "log.entryAdded") {
      this.#consoleAPIListener.startListening();
      this.#consoleMessageListener.startListening();
    }
  }

  #unsubscribeEvent(event) {
    if (event === "log.entryAdded") {
      this.#consoleAPIListener.stopListening();
      this.#consoleMessageListener.stopListening();
    }
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category, added = [], removed = [] } = params;
    if (category === "event") {
      for (const event of added) {
        this.#subscribeEvent(event);
      }
      for (const event of removed) {
        this.#unsubscribeEvent(event);
      }
    }
  }
}

const log = LogModule;
