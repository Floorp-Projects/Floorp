/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FrameTransport"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  isBrowsingContextCompatible:
    "chrome://remote/content/shared/messagehandler/transports/FrameContextUtils.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  MessageHandlerFrameActor:
    "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameActor.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

const MAX_RETRY_ATTEMPTS = 10;

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
    lazy.MessageHandlerFrameActor.register();
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

  async _sendCommandToBrowsingContext(command, browsingContext) {
    const name = `${command.moduleName}.${command.commandName}`;

    // The browsing context might be destroyed by a navigation. Keep a reference
    // to the webProgress, which will persist, and always use it to retrieve the
    // currently valid browsing context.
    const webProgress = browsingContext.webProgress;

    const { retryOnAbort = false } = command;

    let attempts = 0;
    while (true) {
      try {
        return await webProgress.browsingContext.currentWindowGlobal
          .getActor("MessageHandlerFrame")
          .sendCommand(command, this._messageHandler.sessionId);
      } catch (e) {
        if (!retryOnAbort || e.name != "AbortError") {
          // Only retry if the command supports retryOnAbort and when the
          // JSWindowActor pair gets destroyed.
          throw e;
        }

        if (++attempts > MAX_RETRY_ATTEMPTS) {
          lazy.logger.trace(
            `FrameTransport reached the limit of retry attempts (${MAX_RETRY_ATTEMPTS})` +
              ` for command ${name} and browsing context ${webProgress.browsingContext.id}.`
          );
          throw e;
        }

        lazy.logger.trace(
          `FrameTransport retrying command ${name} for ` +
            `browsing context ${webProgress.browsingContext.id}, attempt: ${attempts}.`
        );
        await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
      }
    }
  }

  toString() {
    return `[object ${this.constructor.name} ${this._messageHandler.name}]`;
  }

  _getBrowsingContextsForDescriptor(contextDescriptor) {
    const { id, type } = contextDescriptor;

    if (type === lazy.ContextDescriptorType.All) {
      return this._getBrowsingContexts();
    }

    if (type === lazy.ContextDescriptorType.TopBrowsingContext) {
      return this._getBrowsingContexts({ browserId: id });
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

    // Fetch all tab related browsing contexts for top-level windows.
    for (const { browsingContext } of lazy.TabManager.browsers) {
      if (lazy.isBrowsingContextCompatible(browsingContext, { browserId })) {
        browsingContexts = browsingContexts.concat(
          browsingContext.getAllBrowsingContextsInSubtree()
        );
      }
    }

    return browsingContexts;
  }
}
