/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["RootMessageHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FrameTransport:
    "chrome://remote/content/shared/messagehandler/transports/FrameTransport.jsm",
  MessageHandler:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

/**
 * A RootMessageHandler is the root node of a MessageHandler network. It lives
 * in the parent process. It can forward commands to MessageHandlers in other
 * layers (at the moment WindowGlobalMessageHandlers in content processes).
 */
class RootMessageHandler extends MessageHandler {
  /**
   * Returns the RootMessageHandler module path.
   *
   * @return {String}
   */
  static get modulePath() {
    return "root";
  }

  /**
   * Returns the RootMessageHandler type.
   *
   * @return {String}
   */
  static get type() {
    return "ROOT";
  }

  /**
   * The ROOT MessageHandler is unique for a given MessageHandler network
   * (ie for a given sessionId). Reuse the type as context id here.
   */
  static getIdFromContext(context) {
    return RootMessageHandler.type;
  }

  /**
   * Create a new RootMessageHandler instance.
   *
   * @param {String} sessionId
   *     ID of the session the handler is used for.
   */
  constructor(sessionId) {
    super(sessionId, null);

    this._frameTransport = new FrameTransport(this);
  }

  /**
   * Forward the provided command to WINDOW_GLOBAL MessageHandlers via the
   * FrameTransport.
   *
   * @param {Command} command
   *     The command to forward. See type definition in MessageHandler.js
   * @return {Promise}
   *     Returns a promise that resolves with the result of the command.
   */
  forwardCommand(command) {
    switch (command.destination.type) {
      case WindowGlobalMessageHandler.type:
        return this._frameTransport.forwardCommand(command);
      default:
        throw new Error(
          `Cannot forward command to "${command.destination.type}" from "${this.constructor.type}".`
        );
    }
  }
}
