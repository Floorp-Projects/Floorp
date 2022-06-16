/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["getMessageHandlerClass", "MessageHandlerRegistry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
  readSessionData:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionDataReader.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * Map of MessageHandler type to MessageHandler subclass.
 */
XPCOMUtils.defineLazyGetter(
  lazy,
  "MessageHandlerClasses",
  () =>
    new Map([
      [lazy.RootMessageHandler.type, lazy.RootMessageHandler],
      [lazy.WindowGlobalMessageHandler.type, lazy.WindowGlobalMessageHandler],
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
  if (!lazy.MessageHandlerClasses.has(type)) {
    throw new Error(`No MessageHandler class available for type "${type}"`);
  }
  return lazy.MessageHandlerClasses.get(type);
}

/**
 * The MessageHandlerRegistry allows to create and retrieve MessageHandler
 * instances for different session ids.
 *
 * A MessageHandlerRegistry instance is bound to a specific MessageHandler type
 * and context. All MessageHandler instances created by the same registry will
 * use the type and context of the registry, but each will be associated to a
 * different session id.
 *
 * The registry is useful to retrieve the appropriate MessageHandler instance
 * after crossing a technical boundary (eg process, thread...).
 */
class MessageHandlerRegistry extends EventEmitter {
  /*
   * @param {String} type
   *     MessageHandler type, one of MessageHandler.type.
   * @param {Object} context
   *     The context object, which depends on the type.
   */
  constructor(type, context) {
    super();

    this._messageHandlerClass = getMessageHandlerClass(type);
    this._context = context;
    this._type = type;

    /**
     * Map of session id to MessageHandler instance
     */
    this._messageHandlersMap = new Map();

    this._onMessageHandlerDestroyed = this._onMessageHandlerDestroyed.bind(
      this
    );
    this._onMessageHandlerEvent = this._onMessageHandlerEvent.bind(this);
  }

  /**
   * Create all message handlers for the current context, based on the content
   * of the session data.
   * This should typically be called when the context is ready to be used and
   * to receive/send commands.
   */
  createAllMessageHandlers() {
    const data = lazy.readSessionData();
    for (const [sessionId, sessionDataItems] of data) {
      // Create a message handler for this context for each active message
      // handler session.
      // TODO: In the future, to support debugging use cases we might want to
      // only create a message handler if there is relevant data.
      // For automation scenarios, this is less critical.
      this._createMessageHandler(sessionId, sessionDataItems);
    }
  }

  destroy() {
    this._messageHandlersMap.forEach(messageHandler => {
      messageHandler.destroy();
    });
  }

  /**
   * Retrieve all MessageHandler instances held in this registry, for all
   * session IDs.
   *
   * @return {Iterable.<MessageHandler>}
   *     Iterator of MessageHandler instances
   */
  getAllMessageHandlers() {
    return this._messageHandlersMap.values();
  }

  /**
   * Retrieve an existing MessageHandler instance matching the provided session
   * id. Returns null if no MessageHandler was found.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   * @return {MessageHandler=}
   *     A MessageHandler instance, null if not found.
   */
  getExistingMessageHandler(sessionId) {
    return this._messageHandlersMap.get(sessionId);
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
  getOrCreateMessageHandler(sessionId) {
    let messageHandler = this.getExistingMessageHandler(sessionId);
    if (!messageHandler) {
      messageHandler = this._createMessageHandler(sessionId);
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
      lazy.RootMessageHandler.type
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
   * @param {Array<SessionDataItem>=} sessionDataItems
   *     Optional array of session data items to be applied automatically to the
   *     MessageHandler.
   * @return {MessageHandler}
   *     A new MessageHandler instance.
   */
  _createMessageHandler(sessionId, sessionDataItems) {
    const messageHandler = new this._messageHandlerClass(
      sessionId,
      this._context
    );

    messageHandler.on(
      "message-handler-destroyed",
      this._onMessageHandlerDestroyed
    );
    messageHandler.on("message-handler-event", this._onMessageHandlerEvent);

    messageHandler.applyInitialSessionDataItems(sessionDataItems);

    this._messageHandlersMap.set(sessionId, messageHandler);

    lazy.logger.trace(
      `Created MessageHandler ${this._type} for session ${sessionId}`
    );

    return messageHandler;
  }

  // Event handlers

  _onMessageHandlerDestroyed(eventName, messageHandler) {
    messageHandler.off(
      "message-handler-destroyed",
      this._onMessageHandlerDestroyed
    );
    messageHandler.off("message-handler-event", this._onMessageHandlerEvent);
    this._messageHandlersMap.delete(messageHandler.sessionId);

    lazy.logger.trace(
      `Unregistered MessageHandler ${messageHandler.constructor.type} for session ${messageHandler.sessionId}`
    );
  }

  _onMessageHandlerEvent(eventName, messageHandlerEvent) {
    // The registry simply re-emits MessageHandler events so that consumers
    // don't have to attach listeners to individual MessageHandler instances.
    this.emit("message-handler-registry-event", messageHandlerEvent);
  }
}
