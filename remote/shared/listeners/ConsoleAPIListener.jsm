/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ConsoleAPIListener"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * The ConsoleAPIListener can be used to listen for messages coming from console
 * API usage in a given windowGlobal, eg. console.log, console.error, ...
 *
 * Example:
 * ```
 * const listener = new ConsoleAPIListener(innerWindowId);
 * listener.on("message", onConsoleAPIMessage);
 * listener.startListening();
 *
 * const onConsoleAPIMessage = (eventName, data = {}) => {
 *   const { arguments: msgArguments, level, stacktrace, timeStamp } = data;
 *   ...
 * };
 * ```
 *
 * @emits message
 *    The ConsoleAPIListener emits "message" events, with the following object as
 *    payload:
 *      - {Array<Object>} arguments - Arguments as passed-in when the method was called.
 *      - {String} level - Importance, one of `info`, `warning`, `error`, `debug`, `trace`.
 *      - {Array<Object>} stacktrace - List of stack frames, starting from most recent.
 *      - {Number} timeStamp - Timestamp when the method was called.
 */
class ConsoleAPIListener {
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
    EventEmitter.decorate(this);

    this.#emittedMessages = new Set();
    this.#innerWindowId = innerWindowId;
    this.#listening = false;
  }

  destroy() {
    this.stopListening();
    this.#emittedMessages = null;
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(
      this.#onConsoleAPIMessage,
      "console-api-log-event"
    );

    // Emit cached messages after registering the observer, to make sure we
    // don't miss any message.
    this.#emitCachedMessages();

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(
      this.#onConsoleAPIMessage,
      "console-api-log-event"
    );
    this.#listening = false;
  }

  #emitCachedMessages() {
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);

    const cachedMessages = ConsoleAPIStorage.getEvents(this.#innerWindowId);
    for (const message of cachedMessages) {
      this.#onConsoleAPIMessage(message);
    }
  }

  #onConsoleAPIMessage = message => {
    const messageObject = message.wrappedJSObject;

    // Bail if this message was already emitted, useful to filter out cached
    // messages already received by the consumer.
    if (this.#emittedMessages.has(messageObject)) {
      return;
    }

    this.#emittedMessages.add(messageObject);

    if (messageObject.innerID !== this.#innerWindowId) {
      // If the message doesn't match the innerWindowId of the current context
      // ignore it.
      return;
    }

    this.emit("message", {
      arguments: messageObject.arguments,
      level: messageObject.level,
      stacktrace: messageObject.stacktrace,
      timeStamp: messageObject.timeStamp,
    });
  };
}
