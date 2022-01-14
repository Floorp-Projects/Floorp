/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FrameTransport"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  CONTEXT_DESCRIPTOR_TYPES:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  isBrowsingContextCompatible:
    "chrome://remote/content/shared/messagehandler/transports/FrameContextUtils.jsm",
  MessageHandlerFrameActor:
    "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameActor.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
});

/**
 * FrameTransport is intended to be used from a ROOT MessageHandler to communicate
 * with WINDOW_GLOBAL MessageHandlers via the MessageHandlerFrame JSWindow
 * actors.
 */
class FrameTransport {
  /**
   * @param {MessageHandler}
   *     The MessageHandler instance which owns this FrameTransport instance.
   */
  constructor(messageHandler) {
    this._messageHandler = messageHandler;

    // FrameTransport will rely on the MessageHandlerFrame JSWindow actors.
    // Make sure they are registered when instanciating a FrameTransport.
    MessageHandlerFrameActor.register();
  }

  /**
   * Forward the provided command to WINDOW_GLOBAL MessageHandlers via the
   * MessageHandlerFrame actors.
   *
   * @param {Command} command
   *     The command to forward. See type definition in MessageHandler.js
   * @return {Promise}
   *     Returns a promise that resolves with the result of the command after
   *     being processed by WINDOW_GLOBAL MessageHandlers.
   */
  forwardCommand(command) {
    if (command.destination.id && command.destination.contextDescriptor) {
      throw new Error(
        "Invalid command destination with both 'id' and 'contextDescriptor' properties"
      );
    }

    // With an id given forward the command to only this specific destination.
    if (command.destination.id) {
      const browsingContext = BrowsingContext.get(command.destination.id);
      if (!browsingContext) {
        throw new Error(
          "Unable to find a BrowsingContext for id " + command.destination.id
        );
      }
      return this._sendCommandToBrowsingContext(command, browsingContext);
    }

    // ... otherwise broadcast to destinations matching the contextDescriptor.
    if (command.destination.contextDescriptor) {
      return this._broadcastCommand(command);
    }

    throw new Error(
      "Unrecognized command destination, missing 'id' or 'contextDescriptor' properties"
    );
  }

  _broadcastCommand(command) {
    const { contextDescriptor } = command.destination;
    const browsingContexts = this._getBrowsingContextsForDescriptor(
      contextDescriptor
    );

    return Promise.all(
      browsingContexts.map(async browsingContext => {
        try {
          return await this._sendCommandToBrowsingContext(
            command,
            browsingContext
          );
        } catch (e) {
          console.error(
            `Failed to broadcast a command to browsingContext ${browsingContext.id}`,
            e
          );
          return null;
        }
      })
    );
  }

  _sendCommandToBrowsingContext(command, browsingContext) {
    return browsingContext.currentWindowGlobal
      .getActor("MessageHandlerFrame")
      .sendCommand(command, this._messageHandler.sessionId);
  }

  toString() {
    return `[object ${this.constructor.name} ${this._messageHandler.name}]`;
  }

  _getBrowsingContextsForDescriptor(contextDescriptor) {
    const { id, type } = contextDescriptor;
    if (type === CONTEXT_DESCRIPTOR_TYPES.ALL) {
      return this._getBrowsingContexts();
    }

    if (type === CONTEXT_DESCRIPTOR_TYPES.TOP_BROWSING_CONTEXT) {
      const { browserId } = TabManager.getBrowserById(id);
      return this._getBrowsingContexts({ browserId });
    }

    // TODO: Handle other types of context descriptors.
    throw new Error(
      `Unsupported contextDescriptor type for broadcasting: ${type}`
    );
  }

  /**
   * Get all browsing contexts, optionally matching the provided options.
   *
   * @param {Object} options
   * @param {String=} options.browserId
   *    The id of the browser to filter the browsing contexts by (optional).
   * @return {Array<BrowsingContext>}
   *    The browsing contexts matching the provided options or all browsing contexts
   *    if no options are provided.
   */
  _getBrowsingContexts(options = {}) {
    // extract browserId from options
    const { browserId } = options;
    let browsingContexts = [];
    // Fetch all top level window's browsing contexts
    // Note that getWindowEnumerator works from all processes, including the content process.
    // Looping on windows this way limits to desktop Firefox. See Bug 1723919.
    for (const win of Services.ww.getWindowEnumerator("navigator:browser")) {
      if (!win.gBrowser) {
        continue;
      }

      for (const { browsingContext } of win.gBrowser.browsers) {
        if (isBrowsingContextCompatible(browsingContext, { browserId })) {
          browsingContexts = browsingContexts.concat(
            browsingContext.getAllBrowsingContextsInSubtree()
          );
        }
      }
    }

    return browsingContexts;
  }
}
