/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["error"];

const { RemoteError } = ChromeUtils.import(
  "chrome://remote/content/shared/RemoteError.jsm"
);

class MessageHandlerError extends RemoteError {
  /**
   * @param {(string|Error)=} x
   *     Optional string describing error situation or Error instance
   *     to propagate.
   */
  constructor(x) {
    super(x);
    this.name = this.constructor.name;
    this.status = "message handler error";

    // Error's ctor does not preserve x' stack
    if (typeof x?.stack !== "undefined") {
      this.stack = x.stack;
    }
  }

  get isMessageHandlerError() {
    return true;
  }

  /**
   * @return {Object.<string, string>}
   *     JSON serialisation of error prototype.
   */
  toJSON() {
    return {
      error: this.status,
      message: this.message || "",
      stacktrace: this.stack || "",
    };
  }

  /**
   * Unmarshals a JSON error representation to the appropriate MessageHandler
   * error type.
   *
   * @param {Object.<string, string>} json
   *     Error object.
   *
   * @return {Error}
   *     Error prototype.
   */
  static fromJSON(json) {
    if (typeof json.error == "undefined") {
      let s = JSON.stringify(json);
      throw new TypeError("Undeserialisable error type: " + s);
    }
    if (!STATUSES.has(json.error)) {
      throw new TypeError("Not of MessageHandlerError descent: " + json.error);
    }

    let cls = STATUSES.get(json.error);
    let err = new cls();
    if ("message" in json) {
      err.message = json.message;
    }
    if ("stacktrace" in json) {
      err.stack = json.stacktrace;
    }
    return err;
  }
}

/**
 * A command could not be handled by the message handler network.
 */
class UnsupportedCommandError extends MessageHandlerError {
  constructor(message) {
    super(message);
    this.status = "unsupported message handler command";
  }
}

const STATUSES = new Map([
  ["message handler error", MessageHandlerError],
  ["unsupported message handler command", UnsupportedCommandError],
]);

/** @namespace */
const error = {
  MessageHandlerError,
  UnsupportedCommandError,
};
