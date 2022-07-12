/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageHandlerFrameChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  isBrowsingContextCompatible:
    "chrome://remote/content/shared/messagehandler/transports/FrameContextUtils.jsm",
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
    this.type = lazy.WindowGlobalMessageHandler.type;
    this.context = this.manager.browsingContext;

    this._registry = new lazy.MessageHandlerRegistry(this.type, this.context);
    this._onRegistryEvent = this._onRegistryEvent.bind(this);

    // MessageHandlerFrameChild is responsible for forwarding events from
    // WindowGlobalMessageHandler to the parent process.
    // Such events are re-emitted on the MessageHandlerRegistry to avoid
    // setting up listeners on individual MessageHandler instances.
    this._registry.on("message-handler-registry-event", this._onRegistryEvent);
  }

  handleEvent({ type }) {
    if (type == "DOMWindowCreated") {
      if (lazy.isBrowsingContextCompatible(this.manager.browsingContext)) {
        this._registry.createAllMessageHandlers();
      }
    }
  }

  async receiveMessage(message) {
    if (message.name === "MessageHandlerFrameParent:sendCommand") {
      const { sessionId, command } = message.data;
      const messageHandler = this._registry.getOrCreateMessageHandler(
        sessionId
      );
      try {
        return await messageHandler.handleCommand(command);
      } catch (e) {
        if (e?.isRemoteError) {
          return {
            error: e.toJSON(),
            isMessageHandlerError: e.isMessageHandlerError,
          };
        }
        throw e;
      }
    }

    return null;
  }

  _onRegistryEvent(eventName, wrappedEvent) {
    this.sendAsyncMessage(
      "MessageHandlerFrameChild:messageHandlerEvent",
      wrappedEvent
    );
  }

  didDestroy() {
    this._registry.off("message-handler-registry-event", this._onRegistryEvent);
    this._registry.destroy();
  }
}
