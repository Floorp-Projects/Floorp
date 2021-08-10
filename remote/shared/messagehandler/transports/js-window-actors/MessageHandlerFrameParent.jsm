/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageHandlerFrameParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  MessageHandlerRegistry:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm",
});

/**
 * Parent actor for the MessageHandlerFrame JSWindowActor. The
 * MessageHandlerFrame actor is used by FrameTransport to communicate between
 * ROOT MessageHandlers and WINDOW_GLOBAL MessageHandlers.
 */
class MessageHandlerFrameParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "MessageHandlerFrameChild:messageHandlerEvent":
        const { method, params, sessionId } = message.data;

        const messageHandler = MessageHandlerRegistry.getRootMessageHandler(
          sessionId
        );

        // Re-emit the event on the RootMessageHandler.
        messageHandler.emitMessageHandlerEvent(method, params);

        break;
      default:
        throw new Error("Unsupported message:" + message.name);
    }
  }

  /**
   * Send a command to the corresponding MessageHandlerFrameChild actor via a
   * JSWindowActor query.
   *
   * @param {Command} command
   *     The command to forward. See type definition in MessageHandler.js
   * @param {String} sessionId
   *     ID of the session that sent the command.
   * @return {Promise}
   *     Promise that will resolve with the result of query sent to the
   *     MessageHandlerFrameChild actor.
   */
  sendCommand(command, sessionId) {
    return this.sendQuery("MessageHandlerFrameParent:sendCommand", {
      command,
      sessionId,
    });
  }
}
