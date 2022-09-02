/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Module"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "disabledExperimentalAPI", () => {
  return !Services.prefs.getBoolPref("remote.experimental.enabled");
});

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
   * Add session data for a given module and event.
   *
   * @param {string} moduleName
   *     Name of the module.
   * @param {string} event
   *     Name of the event.
   */
  addEventSessionData(moduleName, event) {
    return this.messageHandler.addSessionData({
      moduleName,
      category: "event",
      contextDescriptor: {
        type: lazy.ContextDescriptorType.All,
      },
      values: [event],
    });
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
   * @param {ContextInfo=} contextInfo
   *     The event's context info, see MessageHandler:emitEvent. Optional.
   */
  emitEvent(name, data, contextInfo) {
    this.messageHandler.emitEvent(name, data, contextInfo);
  }

  /**
   * Intercept an event and modify the payload.
   *
   * It's required to be implemented in windowglobal-in-root modules.
   *
   * @param {string} name
   *     Name of the event.
   * @param {Object} payload
   *    The event's payload.
   * @returns {Object}
   *     The modified event payload.
   */
  interceptEvent(name, payload) {
    throw new Error(
      `Could not intercept event ${name}, interceptEvent is not implemented in windowglobal-in-root module`
    );
  }

  /**
   * Assert if experimental commands are enabled.
   *
   * @param {String} methodName
   *     Name of the command.
   *
   * @throws {UnknownCommandError}
   *     If experimental commands are disabled.
   */
  assertExperimentalCommandsEnabled(methodName) {
    // TODO: 1778987. Move it to a BiDi specific place.
    if (lazy.disabledExperimentalAPI) {
      throw new lazy.error.UnknownCommandError(methodName);
    }
  }

  /**
   * Assert if experimental events are enabled.
   *
   * @param {string} moduleName
   *     Name of the module.
   *
   * @param {string} event
   *     Name of the event.
   *
   * @throws {InvalidArgumentError}
   *     If experimental events are disabled.
   */
  assertExperimentalEventsEnabled(moduleName, event) {
    // TODO: 1778987. Move it to a BiDi specific place.
    if (lazy.disabledExperimentalAPI) {
      throw new lazy.error.InvalidArgumentError(
        `Module ${moduleName} does not support event ${event}`
      );
    }
  }

  /**
   * Remove session data for a given module and event.
   *
   * @param {string} moduleName
   *     Name of the module.
   * @param {string} event
   *     Name of the event.
   */
  removeEventSessionData(moduleName, event) {
    return this.messageHandler.removeSessionData({
      moduleName,
      category: "event",
      contextDescriptor: {
        type: lazy.ContextDescriptorType.All,
      },
      values: [event],
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
    return this.supportedEvents.includes(event);
  }

  static supportsMethod(methodName) {
    return typeof this.prototype[methodName] === "function";
  }
}
