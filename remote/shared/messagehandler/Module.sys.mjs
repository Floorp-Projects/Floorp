/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "disabledExperimentalAPI", () => {
  return !Services.prefs.getBoolPref("remote.experimental.enabled");
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class Module {
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
   */
  destroy() {
    lazy.logger.warn(
      `Module ${this.constructor.name} is missing a destroy method`
    );
  }

  /**
   * Emit a message handler event.
   *
   * Such events should bubble up to the root of a MessageHandler network.
   *
   * @param {string} name
   *     Name of the event. Protocol level events should be of the
   *     form [module name].[event name].
   * @param {object} data
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
   * @param {object} payload
   *    The event's payload.
   * @returns {object}
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
   * @param {string} methodName
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
