/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  isBrowsingContextCompatible:
    "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs",
  MessageHandlerRegistry:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * Map from MessageHandlerRegistry to MessageHandlerFrameChild actor. This will
 * allow a WindowGlobalMessageHandler to find the JSWindowActorChild instance to
 * use to send commands.
 */
const registryToActor = new WeakMap();

/**
 * Retrieve the MessageHandlerFrameChild which is linked to the provided
 * WindowGlobalMessageHandler instance.
 *
 * @param {WindowGlobalMessageHandler} messageHandler
 *     The WindowGlobalMessageHandler for which to get the JSWindowActor.
 * @returns {MessageHandlerFrameChild}
 *     The corresponding MessageHandlerFrameChild instance.
 */
export function getMessageHandlerFrameChildActor(messageHandler) {
  return registryToActor.get(messageHandler.registry);
}

/**
 * Child actor for the MessageHandlerFrame JSWindowActor. The
 * MessageHandlerFrame actor is used by RootTransport to communicate between
 * ROOT MessageHandlers and WINDOW_GLOBAL MessageHandlers.
 */
export class MessageHandlerFrameChild extends JSWindowActorChild {
  actorCreated() {
    this.type = lazy.WindowGlobalMessageHandler.type;
    this.context = this.manager.browsingContext;

    this._registry = new lazy.MessageHandlerRegistry(this.type, this.context);
    registryToActor.set(this._registry, this);

    this._onRegistryEvent = this._onRegistryEvent.bind(this);

    // MessageHandlerFrameChild is responsible for forwarding events from
    // WindowGlobalMessageHandler to the parent process.
    // Such events are re-emitted on the MessageHandlerRegistry to avoid
    // setting up listeners on individual MessageHandler instances.
    this._registry.on("message-handler-registry-event", this._onRegistryEvent);
  }

  handleEvent({ persisted, type }) {
    if (type == "DOMWindowCreated" || (type == "pageshow" && persisted)) {
      // When the window is created or is retrieved from BFCache, instantiate
      // a MessageHandler for all sessions which might need it.
      if (lazy.isBrowsingContextCompatible(this.manager.browsingContext)) {
        this._registry.createAllMessageHandlers();
      }
    } else if (type == "pagehide" && persisted) {
      // When the page is moved to BFCache, all the currently created message
      // handlers should be destroyed.
      this._registry.destroy();
    }
  }

  async receiveMessage(message) {
    if (message.name === "MessageHandlerFrameParent:sendCommand") {
      const { sessionId, command } = message.data;
      const messageHandler =
        this._registry.getOrCreateMessageHandler(sessionId);
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

  sendCommand(command, sessionId) {
    return this.sendQuery("MessageHandlerFrameChild:sendCommand", {
      command,
      sessionId,
    });
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
