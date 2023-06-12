/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ConsoleAPIListener:
    "chrome://remote/content/shared/listeners/ConsoleAPIListener.sys.mjs",
  ConsoleListener:
    "chrome://remote/content/shared/listeners/ConsoleListener.sys.mjs",
  isChromeFrame: "chrome://remote/content/shared/Stack.sys.mjs",
  OwnershipModel: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  setDefaultSerializationOptions:
    "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
});

class LogModule extends WindowGlobalBiDiModule {
  #consoleAPIListener;
  #consoleMessageListener;
  #subscribedEvents;

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

    // Set of event names which have active subscriptions.
    this.#subscribedEvents = new Set();
  }

  destroy() {
    this.#consoleAPIListener.off("message", this.#onConsoleAPIMessage);
    this.#consoleAPIListener.destroy();
    this.#consoleMessageListener.off("error", this.#onJavaScriptError);
    this.#consoleMessageListener.destroy();

    this.#subscribedEvents = null;
  }

  #buildSource(realm) {
    return {
      realm: realm.id,
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
   * @returns {object=} Object, containing the list of frames as `callFrames`.
   */
  #buildStackTrace(stackTrace) {
    if (stackTrace == undefined) {
      return undefined;
    }

    const callFrames = stackTrace
      .filter(frame => !lazy.isChromeFrame(frame))
      .map(frame => {
        return {
          columnNumber: frame.columnNumber - 1,
          functionName: frame.functionName,
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
        return "warn";
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

    const defaultRealm = this.messageHandler.getRealm();
    const serializedArgs = [];
    const seenNodeIds = new Map();

    // Serialize each arg as remote value.
    for (const arg of args) {
      // Note that we can pass a default realm for now since realms are only
      // involved when creating object references, which will not happen with
      // OwnershipModel.None. This will be revisited in Bug 1742589.
      serializedArgs.push(
        this.serialize(
          Cu.waiveXrays(arg),
          lazy.setDefaultSerializationOptions(),
          lazy.OwnershipModel.None,
          defaultRealm,
          { seenNodeIds }
        )
      );
    }

    // Set source to an object which contains realm and browsing context.
    // TODO: Bug 1742589. Use an actual realm from which the event came from.
    const source = this.#buildSource(defaultRealm);

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
      _extraData: { seenNodeIds },
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
    const defaultRealm = this.messageHandler.getRealm();

    // Build the JavascriptLogEntry
    const entry = {
      type: "javascript",
      level,
      // TODO: Bug 1742589. Use an actual realm from which the event came from.
      source: this.#buildSource(defaultRealm),
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
      this.#subscribedEvents.add(event);
    }
  }

  #unsubscribeEvent(event) {
    if (event === "log.entryAdded") {
      this.#consoleAPIListener.stopListening();
      this.#consoleMessageListener.stopListening();
      this.#subscribedEvents.delete(event);
    }
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData.
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }
}

export const log = LogModule;
