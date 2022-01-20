/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["CONTEXT_DESCRIPTOR_TYPES", "MessageHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",

  error: "chrome://remote/content/shared/messagehandler/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  ModuleCache: "chrome://remote/content/shared/messagehandler/ModuleCache.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * A ContextDescriptor object provides information to decide if a broadcast or
 * a session data item should be applied to a specific MessageHandler context.
 *
 * TODO: At the moment we only support one value: { type: "all", id: "all" },
 * but the format of the ContextDescriptor object is designed to fit more
 * complex values.
 * As soon as we start supporting broadcasts targeting only a part of the
 * context tree, we will add additional context types. This work will begin with
 * Bug 1725111, where we will support filtering on a single navigable. It will
 * be later expanded to filter on a worker, a webextension, a process etc...
 *
 * @typedef {Object} ContextDescriptor
 * @property {String} type
 *     The type of context, one of CONTEXT_DESCRIPTOR_TYPES
 * @property {String=} id
 *     Unique id of a given context for the provided type.
 *     For CONTEXT_DESCRIPTOR_TYPES.ALL, id can be ommitted.
 *     For CONTEXT_DESCRIPTOR_TYPES.TOP_BROWSING_CONTEXT, the id should be a
 *     browserId.
 */

// Enum of ContextDescriptor types.
const CONTEXT_DESCRIPTOR_TYPES = {
  ALL: "all",
  TOP_BROWSING_CONTEXT: "top-browsing-context",
};

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
 * RootMessageHandlerRegistry. This singleton will ensure that MessageHandler
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

    this._moduleCache = new ModuleCache(this);

    this._sessionId = sessionId;
    this._context = context;
    this._contextId = this.constructor.getIdFromContext(context);
  }

  get contextId() {
    return this._contextId;
  }

  get name() {
    return [this.sessionId, this.constructor.type, this.contextId].join("-");
  }

  get sessionId() {
    return this._sessionId;
  }

  /**
   * Check if the command can be handled from this MessageHandler node.
   *
   * @param {Command} command
   *     The command to check. See type definition in MessageHandler.js
   * @throws {Error}
   *     Throws if there is no module supporting the provided command on the
   *     path to the command's destination
   */
  checkCommand(command) {
    const { commandName, destination, moduleName } = command;

    // Retrieve all the modules classes which can be used to reach the
    // command's destination.
    const moduleClasses = this._moduleCache.getAllModuleClasses(
      moduleName,
      destination
    );

    if (!moduleClasses.some(cls => cls.supportsMethod(commandName))) {
      throw new error.UnsupportedCommandError(
        `${moduleName}.${commandName} not supported for destination ${destination?.type}`
      );
    }
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
      method,
      params,
      sessionId: this.sessionId,
    });
  }

  /**
   * @typedef {Object} CommandDestination
   * @property {String} type
   *     One of MessageHandler.type.
   * @property {String=} id
   *     Unique context identifier. The format depends on the type.
   *     For WINDOW_GLOBAL destinations, this is a browsing context id.
   *     Optional, should only be provided if `contextDescriptor` is missing.
   * @property {ContextDescriptor=} contextDescriptor
   *     Descriptor used to match several contexts, which will all receive the
   *     command.
   *     Optional, should only be provided if `id` is missing.
   */

  /**
   * @typedef {Object} Command
   * @property {String} commandName
   *     The name of the command to execute.
   * @property {String} moduleName
   *     The name of the module.
   * @property {Object} params
   *     Optional command parameters.
   * @property {CommandDestination} destination
   *     The destination describing a debuggable context.
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
    const { moduleName, commandName, params, destination } = command;
    logger.trace(
      `Received command ${moduleName}.${commandName} for destination ${destination.type}`
    );

    this.checkCommand(command);

    const module = this._moduleCache.getModuleInstance(moduleName, destination);
    if (module && module.supportsMethod(commandName)) {
      return module[commandName](params, destination);
    }

    return this.forwardCommand(command);
  }

  toString() {
    return `[object ${this.constructor.name} ${this.name}]`;
  }

  /**
   * Apply the initial session data items provided to this MessageHandler on
   * startup. Implementation is specific to each MessageHandler class.
   *
   * By default the implementation is a no-op.
   *
   * @param {Array<SessionDataItem>} sessionDataItems
   *     Initial session data items for this MessageHandler.
   */
  async applyInitialSessionDataItems(sessionDataItems) {}

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
