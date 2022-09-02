/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ContextDescriptorType", "MessageHandler"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  error: "chrome://remote/content/shared/messagehandler/Errors.jsm",
  EventsDispatcher:
    "chrome://remote/content/shared/messagehandler/EventsDispatcher.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  ModuleCache: "chrome://remote/content/shared/messagehandler/ModuleCache.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * A ContextDescriptor object provides information to decide if a broadcast or
 * a session data item should be applied to a specific MessageHandler context.
 *
 * @typedef {Object} ContextDescriptor
 * @property {ContextDescriptorType} type
 *     The type of context
 * @property {String=} id
 *     Unique id of a given context for the provided type.
 *     For ContextDescriptorType.All, id can be ommitted.
 *     For ContextDescriptorType.TopBrowsingContext, the id should be the
 *     browserId corresponding to a top-level browsing context.
 */

/**
 * Enum of ContextDescriptor types.
 *
 * @enum {string}
 */
const ContextDescriptorType = {
  All: "All",
  TopBrowsingContext: "TopBrowsingContext",
};

/**
 * A ContextInfo identifies a given context that can be linked to a MessageHandler
 * instance. It should be used to identify events coming from this context.
 *
 * It can either be provided by the MessageHandler itself, when the event is
 * emitted from the context it relates to.
 *
 * Or it can be assembled manually, for instance when emitting an event which
 * relates to a window global from the root layer (eg browsingContext.contextCreated).
 *
 * @typedef {Object} ContextInfo
 * @property {String} contextId
 *     Unique id of the MessageHandler corresponding to this context.
 * @property {String} type
 *     One of MessageHandler.type.
 */

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
  #context;
  #contextId;
  #eventsDispatcher;
  #moduleCache;
  #sessionId;

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

    this.#moduleCache = new lazy.ModuleCache(this);

    this.#sessionId = sessionId;
    this.#context = context;
    this.#contextId = this.constructor.getIdFromContext(context);
    this.#eventsDispatcher = new lazy.EventsDispatcher(this);
  }

  get context() {
    return this.#context;
  }

  get contextId() {
    return this.#contextId;
  }

  get eventsDispatcher() {
    return this.#eventsDispatcher;
  }

  get moduleCache() {
    return this.#moduleCache;
  }

  get name() {
    return [this.sessionId, this.constructor.type, this.contextId].join("-");
  }

  get sessionId() {
    return this.#sessionId;
  }

  destroy() {
    lazy.logger.trace(
      `MessageHandler ${this.constructor.type} for session ${this.sessionId} is being destroyed`
    );
    this.#eventsDispatcher.destroy();
    this.#moduleCache.destroy();

    // At least the MessageHandlerRegistry will be expecting this event in order
    // to remove the instance from the registry when destroyed.
    this.emit("message-handler-destroyed", this);
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
   *     The event's context info, used to identify the origin of the event.
   *     If not provided, the context info of the current MessageHandler will be
   *     used.
   */
  emitEvent(name, data, contextInfo) {
    // If no contextInfo field is provided on the event, extract it from the
    // MessageHandler instance.
    contextInfo = contextInfo || this.#getContextInfo();

    // Events are emitted both under their own name for consumers listening to
    // a specific and as `message-handler-event` for consumers which need to
    // catch all events.
    this.emit(name, data, contextInfo);
    this.emit("message-handler-event", {
      name,
      contextInfo,
      data,
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
   * @property {boolean=} retryOnAbort
   *     Optional. When true, commands will be retried upon AbortError, which
   *     can occur when the underlying JSWindowActor pair is destroyed.
   *     Defaults to `false`.
   */

  /**
   * Retrieve all module classes matching the moduleName and destination.
   * See `getAllModuleClasses` (ModuleCache.jsm) for more details.
   *
   * @param {String} moduleName
   *     The name of the module.
   * @param {Destination} destination
   *     The destination.
   * @return {Array.<class<Module>=>}
   *     An array of Module classes.
   */
  getAllModuleClasses(moduleName, destination) {
    return this.#moduleCache.getAllModuleClasses(moduleName, destination);
  }

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
    lazy.logger.trace(
      `Received command ${moduleName}.${commandName} for destination ${destination.type}`
    );

    if (!this.supportsCommand(moduleName, commandName, destination)) {
      throw new lazy.error.UnsupportedCommandError(
        `${moduleName}.${commandName} not supported for destination ${destination?.type}`
      );
    }

    const module = this.#moduleCache.getModuleInstance(moduleName, destination);
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

  /**
   * Check if the given command is supported in the module
   * for the destination
   *
   * @param {String} moduleName
   *     The name of the module.
   * @param {String} commandName
   *     The name of the command.
   * @param {Destination} destination
   *     The destination.
   * @return {Boolean}
   *     True if the command is supported.
   */
  supportsCommand(moduleName, commandName, destination) {
    return this.getAllModuleClasses(moduleName, destination).some(cls =>
      cls.supportsMethod(commandName)
    );
  }

  /**
   * Return the context information for this MessageHandler instance, which
   * can be used to identify the origin of an event.
   *
   * @return {ContextInfo}
   *     The context information for this MessageHandler.
   */
  #getContextInfo() {
    return {
      contextId: this.contextId,
      type: this.constructor.type,
    };
  }
}
