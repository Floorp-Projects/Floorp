/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["RootMessageHandler"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
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
  #frameTransport;
  #sessionData;

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

    this.#frameTransport = new lazy.FrameTransport(this);
    this.#sessionData = new lazy.SessionData(this);
  }

  get sessionData() {
    return this.#sessionData;
  }

  destroy() {
    this.#sessionData.destroy();
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
   * Emit a public protocol event. This event will be sent over to the client.
   *
   * @param {String} name
   *     Name of the event. Protocol level events should be of the
   *     form [module name].[event name].
   * @param {Object} data
   *     The event's data.
   */
  emitProtocolEvent(name, data) {
    this.emit("message-handler-protocol-event", {
      name,
      data,
      sessionId: this.sessionId,
    });
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
        return this.#frameTransport.forwardCommand(command);
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

  async _updateSessionData(sessionData, options = {}) {
    const { mode } = options;

    // TODO: We currently only support adding or removing items separately.
    // Supporting both will be added with transactions in Bug 1741834.
    if (mode != "add" && mode != "remove") {
      throw new Error(`Unsupported mode for _updateSessionData ${mode}`);
    }

    const { moduleName, category, contextDescriptor, values } = sessionData;
    const isAdding = mode === "add";

    const updateMethod = isAdding ? "addSessionData" : "removeSessionData";
    const updatedValues = this.#sessionData[updateMethod](
      moduleName,
      category,
      contextDescriptor,
      values
    );

    if (!updatedValues.length) {
      // Avoid unnecessary broadcast if no value was removed.
      return;
    }

    const windowGlobalDestination = {
      type: lazy.WindowGlobalMessageHandler.type,
      contextDescriptor,
    };

    const rootDestination = {
      type: RootMessageHandler.type,
    };

    for (const destination of [windowGlobalDestination, rootDestination]) {
      // Only apply session data if the module is present for the destination.
      if (this.supportsCommand(moduleName, "_applySessionData", destination)) {
        await this.handleCommand({
          moduleName,
          commandName: "_applySessionData",
          params: {
            [isAdding ? "added" : "removed"]: updatedValues,
            category,
            contextDescriptor,
          },
          destination,
        });
      }
    }
  }
}
