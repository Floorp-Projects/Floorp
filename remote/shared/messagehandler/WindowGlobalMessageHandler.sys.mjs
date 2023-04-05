/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ContextDescriptorType,
  MessageHandler,
} from "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs";

/**
 * A WindowGlobalMessageHandler is dedicated to debugging a single window
 * global. It follows the lifecycle of the corresponding window global and will
 * therefore not survive any navigation. This MessageHandler cannot forward
 * commands further to other MessageHandlers and represents a leaf node in a
 * MessageHandler network.
 */
export class WindowGlobalMessageHandler extends MessageHandler {
  #innerWindowId;

  constructor() {
    super(...arguments);

    this.#innerWindowId = this.context.window.windowGlobalChild.innerWindowId;
  }

  /**
   * Returns the WindowGlobalMessageHandler module path.
   *
   * @returns {string}
   */
  static get modulePath() {
    return "windowglobal";
  }

  /**
   * Returns the WindowGlobalMessageHandler type.
   *
   * @returns {string}
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
   * @returns {string}
   *     The browsing context id.
   */
  static getIdFromContext(context) {
    return context.id;
  }

  get innerWindowId() {
    return this.#innerWindowId;
  }

  get processActor() {
    return ChromeUtils.domProcessChild.getActor("WebDriverProcessData");
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

    // Create a Map with the structure moduleName -> category -> relevant session data items.
    const structuredUpdates = new Map();
    for (const sessionDataItem of sessionDataItems) {
      const { category, contextDescriptor, moduleName } = sessionDataItem;

      if (!this.matchesContext(contextDescriptor)) {
        continue;
      }
      if (!structuredUpdates.has(moduleName)) {
        // Skip session data item if the module is not present
        // for the destination.
        if (!this.moduleCache.hasModule(moduleName, destination)) {
          continue;
        }
        structuredUpdates.set(moduleName, new Map());
      }

      if (!structuredUpdates.get(moduleName).has(category)) {
        structuredUpdates.get(moduleName).set(category, new Set());
      }

      structuredUpdates
        .get(moduleName)
        .get(category)
        .add(sessionDataItem);
    }

    const sessionDataPromises = [];

    for (const [moduleName, categories] of structuredUpdates.entries()) {
      for (const [category, relevantSessionData] of categories.entries()) {
        sessionDataPromises.push(
          this.handleCommand({
            moduleName,
            commandName: "_applySessionData",
            params: {
              category,
              initial: true,
              sessionData: Array.from(relevantSessionData),
            },
            destination,
          })
        );
      }
    }

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

  matchesContext(contextDescriptor) {
    return (
      contextDescriptor.type === ContextDescriptorType.All ||
      (contextDescriptor.type === ContextDescriptorType.TopBrowsingContext &&
        contextDescriptor.id === this.context.browserId)
    );
  }
}
