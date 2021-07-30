/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageHandlerFrameChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  MessageHandlerRegistry:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

/**
 * Child actor for the MessageHandlerFrame JSWindowActor. The
 * MessageHandlerFrame actor is used by FrameTransport to communicate between
 * ROOT MessageHandlers and WINDOW_GLOBAL MessageHandlers.
 */
class MessageHandlerFrameChild extends JSWindowActorChild {
  actorCreated() {
    this.type = WindowGlobalMessageHandler.type;
    this.context = this.manager.browsingContext;
  }

  receiveMessage(message) {
    if (message.name === "MessageHandlerFrameParent:sendCommand") {
      const { sessionId, command } = message.data;
      const messageHandler = this._getMessageHandler(sessionId);
      return messageHandler.handleCommand(command);
    }

    return null;
  }

  /**
   * Get or create a MessageHandler for the provided sessionId and for the
   * browsing context corresponding to this JSWindow actor.
   *
   * @param {String} sessionId
   *     ID of the session of the handler to get/create.
   */
  _getMessageHandler(sessionId) {
    return MessageHandlerRegistry.getOrCreateMessageHandler(
      sessionId,
      this.type,
      this.context
    );
  }

  didDestroy() {
    MessageHandlerRegistry.contextDestroyed(this.context, this.type);
  }
}
