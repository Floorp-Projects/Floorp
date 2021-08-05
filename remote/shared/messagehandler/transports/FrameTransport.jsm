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

  MessageHandlerFrameActor:
    "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameActor.jsm",
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
    if (command.destination.id && command.destination.broadcast) {
      throw new Error(
        "Invalid command destination with both 'id' and 'broadcast' properties"
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

    // ... otherwise broadcast to all registered destinations.
    if (command.destination.broadcast) {
      return this._broadcastCommand(command);
    }

    throw new Error(
      "Unrecognized command destination, missing 'id' or 'broadcast' properties"
    );
  }

  _broadcastCommand(command) {
    const browsingContexts = this._getAllBrowsingContexts();

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
    return `[object ${this.constructor.name} ${this._messageHandler.key}]`;
  }

  _getAllBrowsingContexts() {
    let browsingContexts = [];
    // Fetch all top level window's browsing contexts
    // Note that getWindowEnumerator works from all processes, including the content process.
    // Looping on windows this way limits to desktop Firefox. See Bug 1723919.
    for (const win of Services.ww.getWindowEnumerator("navigator:browser")) {
      if (!win.gBrowser) {
        continue;
      }

      for (const { browsingContext } of win.gBrowser.browsers) {
        // Skip window globals running in the parent process, unless we want to
        // support debugging Chrome context, see Bug 1713440.
        const isChrome = browsingContext.currentWindowGlobal.osPid === -1;

        // Skip temporary initial documents that are about to be replaced by
        // another document. The new document will be linked to a new window
        // global, new JSWindowActors, etc. So attempting to forward any command
        // to the temporary initial document would be useless as it has no
        // connection to the real document that will be loaded shortly after.
        // The new document will be handled as a new context, which should rely
        // on session data (Bug 1713443) to setup the necessary configuration
        // , events, etc.
        const isInitialDocument =
          browsingContext.currentWindowGlobal.isInitialDocument;
        if (isChrome || isInitialDocument) {
          continue;
        }

        browsingContexts = browsingContexts.concat(
          browsingContext.getAllBrowsingContextsInSubtree()
        );
      }
    }

    return browsingContexts;
  }
}
