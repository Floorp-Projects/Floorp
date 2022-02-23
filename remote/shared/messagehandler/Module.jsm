/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Module"];

class Module {
  #messageHandler;

  /**
   * Create a new module instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this Module instance.
   */
  constructor(messageHandler) {
    this.#messageHandler = messageHandler;
  }

  /**
   * Clean-up the module instance.
   *
   * It's required to be implemented in the sub class.
   */
  destroy() {
    throw new Error("Not implemented");
  }

  /**
   * Emit a message handler event.
   *
   * Such events should bubble up to the root of a MessageHandler network.
   *
   * @param {String} name
   *     Name of the event. Protocol level events should be of the
   *     form [module name].[event name].
   * @param {Object} data
   *     The event's data.
   */
  emitEvent(name, data) {
    this.messageHandler.emitEvent(name, data, { isProtocolEvent: false });
  }

  /**
   * Emit a protocol specific message handler event.
   *
   * Such events should bubble up to the root of a MessageHandler network.
   *
   * @param {String} name
   *     Name of the event. Protocol level events should be of the
   *     form [module name].[event name].
   * @param {Object} data
   *     The event's data.
   */
  emitProtocolEvent(name, data) {
    this.messageHandler.emitEvent(name, data, {
      isProtocolEvent: true,
    });
  }

  /**
   * Instance shortcut for supportsMethod to avoid reaching the constructor for
   * consumers which directly deal with an instance.
   */
  supportsMethod(methodName) {
    return this.constructor.supportsMethod(methodName);
  }

  get messageHandler() {
    return this.#messageHandler;
  }

  static get supportedEvents() {
    return [];
  }

  static supportsEvent(event) {
    return (
      this.supportsMethod("_subscribeEvent") &&
      this.supportedEvents.includes(event)
    );
  }

  static supportsMethod(methodName) {
    return typeof this.prototype[methodName] === "function";
  }
}
