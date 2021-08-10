/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["getMessageHandlerClass", "MessageHandlerRegistry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
  MessageHandlerInfo:
    "chrome://remote/content/shared/messagehandler/MessageHandlerInfo.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * Map of MessageHandler type to MessageHandler subclass.
 */
XPCOMUtils.defineLazyGetter(
  this,
  "MessageHandlerClasses",
  () =>
    new Map([
      [RootMessageHandler.type, RootMessageHandler],
      [WindowGlobalMessageHandler.type, WindowGlobalMessageHandler],
    ])
);

/**
 * Get the MessageHandler subclass corresponding to the provided type.

 * @param {String} type
 *     MessageHandler type, one of MessageHandler.type.
 * @return {Class}
 *     A MessageHandler subclass
 * @throws {Error}
 *      Throws if no MessageHandler subclass is found for the provided type.
 */
function getMessageHandlerClass(type) {
  if (!MessageHandlerClasses.has(type)) {
    throw new Error(`No MessageHandler class available for type "${type}"`);
  }
  return MessageHandlerClasses.get(type);
}

/**
 * The MessageHandlerRegistry is a singleton (per process) which allows to
 * create and retrieve MessageHandler instances.
 *
 * Such a singleton is useful to retrieve the appropriate MessageHandler
 * instance after crossing a technical boundary (eg process, thread...).
 *
 * Note: this is still created as a class, but exposed as a singleton.
 */
class MessageHandlerRegistryClass extends EventEmitter {
  constructor() {
    super();

    /**
     * Map of all message handlers registered in this process.
     * Keys are based on session id, message handler type and message handler
     * context id.
     * MessageHandlers for various contexts and sessions might be instantiated
     * in the same process and might therefore be stored together in this map.
     */
    this._messageHandlersMap = new Map();

    this._onMessageHandlerDestroyed = this._onMessageHandlerDestroyed.bind(
      this
    );
    this._onMessageHandlerEvent = this._onMessageHandlerEvent.bind(this);
  }

  /**
   * Retrieve an existing MessageHandler instance matching the provided
   * arguments. Returns null if no MessageHandler was found.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   * @param {Object=} context
   *     The context object, which depends on the type. Can be null for ROOT
   *     type MessageHandlers.
   * @return {MessageHandler=}
   *     A MessageHandler instance, null if not found.
   */
  getExistingMessageHandler(sessionId, type, context) {
    const messageHandlerInfo = new MessageHandlerInfo(
      sessionId,
      type,
      getMessageHandlerClass(type).getIdFromContext(context)
    );

    return this._messageHandlersMap.get(messageHandlerInfo.key);
  }

  /**
   * Notify the registry that the provided context of the provided type has been
   * destroyed, so that all corresponding MessageHandlers can be properly
   * destroyed.
   *
   * @param {Object} context
   *     The context object, which depends on the type.
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   */
  contextDestroyed(context, type) {
    const MessageHandlerClass = getMessageHandlerClass(type);
    const contextId = MessageHandlerClass.getIdFromContext(context);

    // Destroy all MessageHandlers matching the provided type and contextId.
    return this._messageHandlersMap.forEach(messageHandler => {
      if (
        messageHandler.constructor.type === type &&
        messageHandler.contextId === contextId
      ) {
        messageHandler.destroy();
      }
    });
  }

  /**
   * Retrieve an already registered MessageHandler instance matching the
   * provided parameters.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   * @param {Object=} context
   *     The context object, which depends on the type. Can be null for ROOT
   *     type MessageHandlers.
   * @return {MessageHandler}
   *     A MessageHandler instance.
   */
  getOrCreateMessageHandler(sessionId, type, context) {
    let messageHandler = this.getExistingMessageHandler(
      sessionId,
      type,
      context
    );

    if (!messageHandler) {
      messageHandler = this._createMessageHandler(sessionId, type, context);
    }

    return messageHandler;
  }

  /**
   * Retrieve an already registered RootMessageHandler instance matching the
   * provided sessionId.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   * @return {RootMessageHandler}
   *     A RootMessageHandler instance.
   * @throws {Error}
   *     If no root MessageHandler can be found for the provided session id.
   */
  getRootMessageHandler(sessionId) {
    const rootMessageHandler = this.getExistingMessageHandler(
      sessionId,
      RootMessageHandler.type
    );
    if (!rootMessageHandler) {
      throw new Error(
        `Unable to find a root MessageHandler for session id ${sessionId}`
      );
    }
    return rootMessageHandler;
  }

  toString() {
    return `[object ${this.constructor.name}]`;
  }

  /**
   * Create a new MessageHandler instance.
   *
   * @param {String} sessionId
   *     ID of the session the handler will be used for.
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   * @param {Object=} context
   *     The context object, which depends on the type. Can be null for ROOT
   *     type MessageHandlers.
   * @return {MessageHandler}
   *     A new MessageHandler instance.
   */
  _createMessageHandler(sessionId, type, context) {
    const messageHandlerClass = getMessageHandlerClass(type);
    const messageHandler = new messageHandlerClass(sessionId, context);
    this._registerMessageHandler(messageHandler);

    logger.trace(`Created MessageHandler ${type} for session ${sessionId}`);

    messageHandler.on(
      "message-handler-destroyed",
      this._onMessageHandlerDestroyed
    );
    messageHandler.on("message-handler-event", this._onMessageHandlerEvent);
    return messageHandler;
  }

  _registerMessageHandler(messageHandler) {
    this._messageHandlersMap.set(messageHandler.key, messageHandler);
  }

  _unregisterMessageHandler(messageHandler) {
    this._messageHandlersMap.delete(messageHandler.key);
    logger.trace(
      `Unregistered MessageHandler ${messageHandler.type} for session ${messageHandler.sessionId}`
    );
  }

  // Event handlers

  _onMessageHandlerDestroyed(eventName, messageHandler) {
    messageHandler.off(
      "message-handler-destroyed",
      this._onMessageHandlerDestroyed
    );
    messageHandler.off("message-handler-event", this._onMessageHandlerEvent);
    this._unregisterMessageHandler(messageHandler);
  }

  _onMessageHandlerEvent(eventName, messageHandlerEvent) {
    // The registry simply re-emits MessageHandler events so that consumers
    // don't have to attach listeners to individual MessageHandler instances.
    this.emit("message-handler-registry-event", messageHandlerEvent);
  }
}

const MessageHandlerRegistry = new MessageHandlerRegistryClass();
