/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MessageHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
  MessageHandlerInfo:
    "chrome://remote/content/shared/messagehandler/MessageHandlerInfo.jsm",
  ModuleCache: "chrome://remote/content/shared/messagehandler/ModuleCache.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * MessageHandler instances are dedicated to handle both Commands and Events
 * to enable automation and introspection for remote control protocols.
 *
 * MessageHandler instances are designed to form a network, where each instance
 * should allow to inspect a specific context (eg. a BrowsingContext, a Worker,
 * etc). Those instances might live in different processes and threads but
 * should be linked together by the usage of a single sessionId, shared by all
 * the instances of a single MessageHandler network.
 *
 * MessageHandler instances will be dynamically spawned depending on which
 * Command or which Event needs to be processed and should therefore not be
 * explicitly created by consumers, nor used directly.
 *
 * The only exception is the ROOT MessageHandler. This MessageHandler will be
 * the entry point to send commands to the rest of the network. It will also
 * emit all the relevant events captured by the network.
 *
 * However, even to create this ROOT MessageHandler, consumers should use the
 * MessageHandlerRegistry. This singleton will ensure that MessageHandler
 * instances are properly registered and can be retrieved based on a given
 * session id as well as some other context information.
 */
class MessageHandler extends EventEmitter {
  /**
   * Create a new MessageHandler instance.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   * @param {Object} context
   *     The context linked to this MessageHandler instance.
   */
  constructor(sessionId, context) {
    super();

    this._messageHandlerInfo = new MessageHandlerInfo(
      sessionId,
      this.constructor.type,
      this.constructor.getIdFromContext(context)
    );

    this._moduleCache = new ModuleCache(this);
  }

  get contextId() {
    return this._messageHandlerInfo.contextId;
  }

  get key() {
    return this._messageHandlerInfo.key;
  }

  get sessionId() {
    return this._messageHandlerInfo.sessionId;
  }

  destroy() {
    logger.trace(
      `MessageHandler ${this.constructor.type} for session ${this.sessionId} is being destroyed`
    );
    this._moduleCache.destroy();

    // At least the MessageHandlerRegistry will be expecting this event in order
    // to remove the instance from the registry when destroyed.
    this.emit("message-handler-destroyed", this);
  }

  /**
   * Emit a message-handler-event. Such events should bubble up to the root of
   * a MessageHandler network.
   *
   * @param {String} method
   *     A string literal of the form [module name].[event name]. This is the
   *     event name.
   * @param {Object} params
   *     The event parameters.
   */
  emitMessageHandlerEvent(method, params) {
    this.emit("message-handler-event", {
      // TODO: The messageHandlerInfo needs to be wrapped in the event so
      // that consumers can check the type/context. Once MessageHandlerRegistry
      // becomes context-specific (Bug 1722659), only the sessionId will be
      // required.
      messageHandlerInfo: this._messageHandlerInfo,
      method,
      params,
    });
  }

  /**
   * @typedef {Object} CommandDestination
   * @property {String} type - One of MessageHandler.type.
   * @property {String} id - Unique context identifier, format depends on the
   *     type. For WINDOW_GLOBAL destinations, this is a browsing context id.
   */

  /**
   * @typedef {Object} Command
   * @property {String} commandName - The name of the command to execute.
   * @property {String} moduleName - The name of the module.
   * @property {CommandDestination} destination - The destination describing a
   *     debuggable context.
   * @property {Object} params - Optional arguments.
   */

  /**
   * Handle a command, either in one of the modules owned by this MessageHandler
   * or in a another MessageHandler after forwarding the command.
   *
   * @param {Command} command
   *     The command that should be either handled in this layer or forwarded to
   *     the next layer leading to the destination.
   * @return {Promise} A Promise that will resolve with the return value of the
   *     command once it has been executed.
   */
  handleCommand(command) {
    const { moduleName, commandName, destination, params } = command;
    logger.trace(
      `Received command ${moduleName}:${commandName} for destination ${destination.type}`
    );

    const mod = this._moduleCache.getModuleInstance(moduleName, destination);
    if (this._isCommandSupportedByModule(commandName, mod)) {
      return mod[commandName](params, destination);
    }

    return this.forwardCommand(command);
  }

  toString() {
    return `[object ${this.constructor.name} ${this.key}]`;
  }

  _isCommandSupportedByModule(commandName, mod) {
    // TODO: With the current implementation, all functions of a given module
    // are considered as valid commands.
    // This should probably be replaced by a more explicit declaration, via a
    // manifest for instance.
    return mod && typeof mod[commandName] === "function";
  }

  /**
   * Returns the module path corresponding to this MessageHandler class.
   *
   * Needs to be implemented in the sub class.
   */
  static get modulePath() {
    throw new Error("Not implemented");
  }

  /**
   * Returns the type corresponding to this MessageHandler class.
   *
   * Needs to be implemented in the sub class.
   */
  static get type() {
    throw new Error("Not implemented");
  }

  /**
   * Returns the id corresponding to a context compatible with this
   * MessageHandler class.
   *
   * Needs to be implemented in the sub class.
   */
  static getIdFromContext(context) {
    throw new Error("Not implemented");
  }

  /**
   * Forward a command to other MessageHandlers.
   *
   * Needs to be implemented in the sub class.
   */
  forwardCommand(command) {
    throw new Error("Not implemented");
  }
}
