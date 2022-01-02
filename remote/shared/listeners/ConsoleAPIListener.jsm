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
 *   const { arguments: msgArguments, level, rawMessage, timeStamp } = data;
 *   ...
 * };
 * ```
 *
 * @emits message
 *    The ConsoleAPIListener emits "message" events, with the following object as
 *    payload:
 *      - `message` property pointing to the wrappedJSObject of the raw message
 *      - `rawMessage` property pointing to the raw message
 */
class ConsoleAPIListener {
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

    this.#listening = false;
    this.#innerWindowId = innerWindowId;
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    // Bug 1731574: Retrieve cached messages first before registering the
    // listener to avoid duplicated messages.
    Services.obs.addObserver(
      this.#onConsoleAPIMessage,
      "console-api-log-event"
    );
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

  #onConsoleAPIMessage = message => {
    const messageObject = message.wrappedJSObject;

    if (messageObject.innerID !== this.#innerWindowId) {
      // If the message doesn't match the innerWindowId of the current context
      // ignore it.
      return;
    }

    this.emit("message", {
      arguments: messageObject.arguments,
      level: messageObject.level,
      rawMessage: message,
      timeStamp: messageObject.timeStamp,
    });
  };
}
