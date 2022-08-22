/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WindowGlobalMessageHandler"];

const { ContextDescriptorType, MessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandler.jsm"
);

/**
 * A WindowGlobalMessageHandler is dedicated to debugging a single window
 * global. It follows the lifecycle of the corresponding window global and will
 * therefore not survive any navigation. This MessageHandler cannot forward
 * commands further to other MessageHandlers and represents a leaf node in a
 * MessageHandler network.
 */
class WindowGlobalMessageHandler extends MessageHandler {
  #innerWindowId;

  constructor() {
    super(...arguments);

    this.#innerWindowId = this.context.window.windowGlobalChild.innerWindowId;
  }

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

  get innerWindowId() {
    return this.#innerWindowId;
  }

  get window() {
    return this.context.window;
  }

  async applyInitialSessionDataItems(sessionDataItems) {
    if (!Array.isArray(sessionDataItems)) {
      return;
    }

    const destination = {
      type: WindowGlobalMessageHandler.type,
    };

    const sessionDataPromises = sessionDataItems.map(sessionDataItem => {
      const {
        moduleName,
        category,
        contextDescriptor,
        value,
      } = sessionDataItem;
      if (!this._matchesContext(contextDescriptor)) {
        return Promise.resolve();
      }

      // Don't apply session data if the module is not present
      // for the destination.
      if (!this.moduleCache.hasModule(moduleName, destination)) {
        return Promise.resolve();
      }

      return this.handleCommand({
        moduleName,
        commandName: "_applySessionData",
        params: {
          category,
          // TODO: We might call _applySessionData several times for the same
          // moduleName & category, but with different values. Instead we can
          // use the fact that _applySessionData supports arrays of values,
          // though it will make the implementation more complex.
          added: [value],
        },
        destination,
      });
    });

    await Promise.all(sessionDataPromises);

    // With the session data applied the handler is now ready to be used.
    this.emitEvent("window-global-handler-created", {
      contextId: this.contextId,
      innerWindowId: this.#innerWindowId,
    });
  }

  forwardCommand(command) {
    throw new Error(
      `Cannot forward commands from a "WINDOW_GLOBAL" MessageHandler`
    );
  }

  _matchesContext(contextDescriptor) {
    return (
      contextDescriptor.type === ContextDescriptorType.All ||
      (contextDescriptor.type === ContextDescriptorType.TopBrowsingContext &&
        contextDescriptor.id === this.context.browserId)
    );
  }
}
