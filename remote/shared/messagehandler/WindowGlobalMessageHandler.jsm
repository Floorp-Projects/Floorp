/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WindowGlobalMessageHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  MessageHandler:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
});

/**
 * A WindowGlobalMessageHandler is dedicated to debugging a single window
 * global. It follows the lifecycle of the corresponding window global and will
 * therefore not survive any navigation. This MessageHandler cannot forward
 * commands further to other MessageHandlers and represents a leaf node in a
 * MessageHandler network.
 */
class WindowGlobalMessageHandler extends MessageHandler {
  /**
   * Returns the WindowGlobalMessageHandler module path.
   *
   * @return {String}
   */
  static get modulePath() {
    return "windowglobal";
  }

  /**
   * Returns the WindowGlobalMessageHandler type.
   *
   * @return {String}
   */
  static get type() {
    return "WINDOW_GLOBAL";
  }

  /**
   * For WINDOW_GLOBAL MessageHandlers, `context` is a BrowsingContext,
   * and BrowsingContext.id can be used as the context id.
   *
   * @param {BrowsingContext} context
   *     WindowGlobalMessageHandler contexts are expected to be
   *     BrowsingContexts.
   * @return {String}
   *     The browsing context id.
   */
  static getIdFromContext(context) {
    return context.id;
  }

  forwardCommand(command) {
    throw new Error(
      `Cannot forward commands from a "WINDOW_GLOBAL" MessageHandler`
    );
  }
}
