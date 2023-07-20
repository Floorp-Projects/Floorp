/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MessageHandler } from "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NavigationManager: "chrome://remote/content/shared/NavigationManager.sys.mjs",
  RootTransport:
    "chrome://remote/content/shared/messagehandler/transports/RootTransport.sys.mjs",
  SessionData:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs",
  SessionDataMethod:
    "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * A RootMessageHandler is the root node of a MessageHandler network. It lives
 * in the parent process. It can forward commands to MessageHandlers in other
 * layers (at the moment WindowGlobalMessageHandlers in content processes).
 */
export class RootMessageHandler extends MessageHandler {
  #navigationManager;
  #rootTransport;
  #sessionData;

  /**
   * Returns the RootMessageHandler module path.
   *
   * @returns {string}
   */
  static get modulePath() {
    return "root";
  }

  /**
   * Returns the RootMessageHandler type.
   *
   * @returns {string}
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
   * @param {string} sessionId
   *     ID of the session the handler is used for.
   */
  constructor(sessionId) {
    super(sessionId, null);

    this.#rootTransport = new lazy.RootTransport(this);
    this.#sessionData = new lazy.SessionData(this);
    this.#navigationManager = new lazy.NavigationManager();
    this.#navigationManager.startMonitoring();
  }

  get navigationManager() {
    return this.#navigationManager;
  }

  get sessionData() {
    return this.#sessionData;
  }

  destroy() {
    this.#sessionData.destroy();
    this.#navigationManager.destroy();
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
  addSessionDataItem(sessionData = {}) {
    sessionData.method = lazy.SessionDataMethod.Add;
    return this.updateSessionData([sessionData]);
  }

  /**
   * Emit a public protocol event. This event will be sent over to the client.
   *
   * @param {string} name
   *     Name of the event. Protocol level events should be of the
   *     form [module name].[event name].
   * @param {object} data
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
   * RootTransport.
   *
   * @param {Command} command
   *     The command to forward. See type definition in MessageHandler.js
   * @returns {Promise}
   *     Returns a promise that resolves with the result of the command.
   */
  forwardCommand(command) {
    switch (command.destination.type) {
      case lazy.WindowGlobalMessageHandler.type:
        return this.#rootTransport.forwardCommand(command);
      default:
        throw new Error(
          `Cannot forward command to "${command.destination.type}" from "${this.constructor.type}".`
        );
    }
  }

  matchesContext() {
    return true;
  }

  /**
   * Remove session data items of a given module, category and
   * contextDescriptor.
   *
   * Forwards the call to the SessionData instance owned by this
   * RootMessageHandler and propagates the information via a command to existing
   * MessageHandlers.
   */
  removeSessionDataItem(sessionData = {}) {
    sessionData.method = lazy.SessionDataMethod.Remove;
    return this.updateSessionData([sessionData]);
  }

  /**
   * Update session data items of a given module, category and
   * contextDescriptor.
   *
   * Forwards the call to the SessionData instance owned by this
   * RootMessageHandler.
   */
  async updateSessionData(sessionData = []) {
    await this.#sessionData.updateSessionData(sessionData);
  }
}
