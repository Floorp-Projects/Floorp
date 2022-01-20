/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ConsoleListener"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Services: "resource://gre/modules/Services.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * The ConsoleListener can be used to listen for console messages related to
 * Javascript errors, certain warnings which all happen within a specific
 * windowGlobal. Consumers can listen for the message types "error",
 * "warning" and "info".
 *
 * Example:
 * ```
 * const onJavascriptError = (eventName, data = {}) => {
 *   const { level, message, timestamp } = data;
 *   ...
 * };
 *
 * const listener = new ConsoleListener(innerWindowId);
 * listener.on("error", onJavascriptError);
 * listener.startListening();
 * ...
 * listener.stopListening();
 * ```
 */
class ConsoleListener {
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

  get listening() {
    return this.#listening;
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
    Services.console.registerListener(this.#onConsoleMessage);
    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.console.unregisterListener(this.#onConsoleMessage);
    this.#listening = false;
  }

  #onConsoleMessage = message => {
    if (!(message instanceof Ci.nsIScriptError)) {
      // For now ignore basic nsIConsoleMessage instances, which are only
      // relevant to Chrome code and do not have a valid window reference.
      return;
    }

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
      logger.warn(
        `Not able to process console message with unknown flags ${message.flags}`
      );
      return;
    }

    // Send event when actively listening.
    this.emit(level, {
      level,
      message: message.errorMessage,
      rawMessage: message,
      timeStamp: message.timeStamp,
    });
  };

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIConsoleListener"]);
  }
}
