/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["RootMessageHandler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { MessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandler.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  FrameTransport:
    "chrome://remote/content/shared/messagehandler/transports/FrameTransport.jsm",
  SessionData:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.jsm",
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

    this._frameTransport = new lazy.FrameTransport(this);
    this._sessionData = new lazy.SessionData(this);
  }

  get sessionData() {
    return this._sessionData;
  }

  destroy() {
    this._sessionData.destroy();
    super.destroy();
  }

  /**
   * Add new session data items of a given module, category and
   * contextDescriptor.
   *
   * Forwards the call to the SessionData instance owned by this
   * RootMessageHandler and propagates the information via a command to existing
   * MessageHandlers.
   */
  addSessionData(sessionData = {}) {
    return this._updateSessionData(sessionData, { mode: "add" });
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
      case lazy.WindowGlobalMessageHandler.type:
        return this._frameTransport.forwardCommand(command);
      default:
        throw new Error(
          `Cannot forward command to "${command.destination.type}" from "${this.constructor.type}".`
        );
    }
  }

  /**
   * Remove session data items of a given module, category and
   * contextDescriptor.
   *
   * Forwards the call to the SessionData instance owned by this
   * RootMessageHandler and propagates the information via a command to existing
   * MessageHandlers.
   */
  removeSessionData(sessionData = {}) {
    return this._updateSessionData(sessionData, { mode: "remove" });
  }

  _updateSessionData(sessionData, options = {}) {
    const { mode } = options;

    // TODO: We currently only support adding or removing items separately.
    // Supporting both will be added with transactions in Bug 1741834.
    if (mode != "add" && mode != "remove") {
      throw new Error(`Unsupported mode for _updateSessionData ${mode}`);
    }

    const { moduleName, category, contextDescriptor, values } = sessionData;
    const isAdding = mode === "add";

    const updateMethod = isAdding ? "addSessionData" : "removeSessionData";
    const updatedValues = this._sessionData[updateMethod](
      moduleName,
      category,
      contextDescriptor,
      values
    );

    if (updatedValues.length == 0) {
      // Avoid unnecessary broadcast if no value was removed.
      return [];
    }

    const destination = {
      type: lazy.WindowGlobalMessageHandler.type,
      contextDescriptor,
    };

    // Don't apply session data if the module is not present
    // for the destination.
    if (!this._moduleCache.hasModule(moduleName, destination)) {
      return Promise.resolve();
    }

    return this.handleCommand({
      moduleName,
      commandName: "_applySessionData",
      params: {
        [isAdding ? "added" : "removed"]: updatedValues,
        category,
      },
      destination,
    });
  }
}
