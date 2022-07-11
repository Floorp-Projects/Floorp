/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ConsoleListener"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",

  getFramesFromStack: "chrome://remote/content/shared/Stack.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * The ConsoleListener can be used to listen for console messages related to
 * Javascript errors, certain warnings which all happen within a specific
 * windowGlobal. Consumers can listen for the message types "error",
 * "warning" and "info".
 *
 * Example:
 * ```
 * const onJavascriptError = (eventName, data = {}) => {
 *   const { level, message, stacktrace, timestamp } = data;
 *   ...
 * };
 *
 * const listener = new ConsoleListener(innerWindowId);
 * listener.on("error", onJavascriptError);
 * listener.startListening();
 * ...
 * listener.stopListening();
 * ```
 *
 * @emits message
 *    The ConsoleListener emits "error", "warning" and "info" events, with the
 *    following object as payload:
 *      - {String} level - Importance, one of `info`, `warning`, `error`,
 *        `debug`, `trace`.
 *      - {String} message - Actual message from the console entry.
 *      - {Array<StackFrame>} stacktrace - List of stack frames,
 *        starting from most recent.
 *      - {Number} timeStamp - Timestamp when the method was called.
 */
class ConsoleListener {
  #emittedMessages;
  #innerWindowId;
  #listening;

  /**
   * Create a new ConsolerListener instance.
   *
   * @param {Number} innerWindowId
   *     The inner window id to filter the messages for.
   */
  constructor(innerWindowId) {
    lazy.EventEmitter.decorate(this);

    this.#emittedMessages = new Set();
    this.#innerWindowId = innerWindowId;
    this.#listening = false;
  }

  get listening() {
    return this.#listening;
  }

  destroy() {
    this.stopListening();
    this.#emittedMessages = null;
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.console.registerListener(this.#onConsoleMessage);

    // Emit cached messages after registering the listener, to make sure we
    // don't miss any message.
    this.#emitCachedMessages();

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.console.unregisterListener(this.#onConsoleMessage);
    this.#listening = false;
  }

  #emitCachedMessages() {
    const cachedMessages = Services.console.getMessageArray() || [];

    for (const message of cachedMessages) {
      this.#onConsoleMessage(message);
    }
  }

  #onConsoleMessage = message => {
    if (!(message instanceof Ci.nsIScriptError)) {
      // For now ignore basic nsIConsoleMessage instances, which are only
      // relevant to Chrome code and do not have a valid window reference.
      return;
    }

    // Bail if this message was already emitted, useful to filter out cached
    // messages already received by the consumer.
    if (this.#emittedMessages.has(message)) {
      return;
    }

    this.#emittedMessages.add(message);

    if (message.innerWindowID !== this.#innerWindowId) {
      // If the message doesn't match the innerWindowId of the current context
      // ignore it.
      return;
    }

    const { errorFlag, warningFlag, infoFlag } = Ci.nsIScriptError;
    let level;

    if ((message.flags & warningFlag) == warningFlag) {
      level = "warning";
    } else if ((message.flags & infoFlag) == infoFlag) {
      level = "info";
    } else if ((message.flags & errorFlag) == errorFlag) {
      level = "error";
    } else {
      lazy.logger.warn(
        `Not able to process console message with unknown flags ${message.flags}`
      );
      return;
    }

    // Send event when actively listening.
    this.emit(level, {
      level,
      message: message.errorMessage,
      stacktrace: lazy.getFramesFromStack(message.stack),
      timeStamp: message.timeStamp,
    });
  };

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIConsoleListener"]);
  }
}
