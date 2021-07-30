/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FrameTransport"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
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
    const browsingContext = BrowsingContext.get(command.destination.id);
    if (!browsingContext) {
      throw new Error(
        "Unable to find a BrowsingContext for id " + command.destination.id
      );
    }

    return browsingContext.currentWindowGlobal
      .getActor("MessageHandlerFrame")
      .sendCommand(command, this._messageHandler.sessionId);
  }
}
