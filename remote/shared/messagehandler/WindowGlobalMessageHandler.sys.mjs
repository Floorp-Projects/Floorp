/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ContextDescriptorType,
  MessageHandler,
} from "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  getMessageHandlerFrameChildActor:
    "chrome://remote/content/shared/messagehandler/transports/js-window-actors/MessageHandlerFrameChild.sys.mjs",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
  WindowRealm: "chrome://remote/content/shared/Realm.sys.mjs",
});

/**
 * A WindowGlobalMessageHandler is dedicated to debugging a single window
 * global. It follows the lifecycle of the corresponding window global and will
 * therefore not survive any navigation. This MessageHandler cannot forward
 * commands further to other MessageHandlers and represents a leaf node in a
 * MessageHandler network.
 */
export class WindowGlobalMessageHandler extends MessageHandler {
  #innerWindowId;
  #realms;

  constructor() {
    super(...arguments);

    this.#innerWindowId = this.context.window.windowGlobalChild.innerWindowId;

    // Maps sandbox names to instances of window realms.
    this.#realms = new Map();
  }

  initialize(sessionDataItems) {
    // Create the default realm, it is mapped to an empty string sandbox name.
    this.#realms.set("", this.#createRealm());

    // This method, even though being async, is not awaited on purpose,
    // since for now the sessionDataItems are passed in response to an event in a for loop.
    this.#applyInitialSessionDataItems(sessionDataItems);

    // With the session data applied the handler is now ready to be used.
    this.emitEvent("window-global-handler-created", {
      contextId: this.contextId,
      innerWindowId: this.#innerWindowId,
    });
  }

  destroy() {
    for (const realm of this.#realms.values()) {
      realm.destroy();
    }
    this.emitEvent("windowglobal-pagehide", {
      context: this.context,
      innerWindowId: this.innerWindowId,
    });
    this.#realms = null;

    super.destroy();
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

  get realms() {
    return this.#realms;
  }

  get window() {
    return this.context.window;
  }

  #createRealm(sandboxName = null) {
    const realm = new lazy.WindowRealm(this.context.window, {
      sandboxName,
    });

    this.emitEvent("realm-created", {
      realmInfo: realm.getInfo(),
      innerWindowId: this.innerWindowId,
    });

    return realm;
  }

  #getRealmFromSandboxName(sandboxName = null) {
    if (sandboxName === null || sandboxName === "") {
      return this.#realms.get("");
    }

    if (this.#realms.has(sandboxName)) {
      return this.#realms.get(sandboxName);
    }

    const realm = this.#createRealm(sandboxName);

    this.#realms.set(sandboxName, realm);

    return realm;
  }

  async #applyInitialSessionDataItems(sessionDataItems) {
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
        if (!this.moduleCache.hasModuleClass(moduleName, destination)) {
          continue;
        }
        structuredUpdates.set(moduleName, new Map());
      }

      if (!structuredUpdates.get(moduleName).has(category)) {
        structuredUpdates.get(moduleName).set(category, new Set());
      }

      structuredUpdates.get(moduleName).get(category).add(sessionDataItem);
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
  }

  forwardCommand(command) {
    switch (command.destination.type) {
      case lazy.RootMessageHandler.type:
        return lazy
          .getMessageHandlerFrameChildActor(this)
          .sendCommand(command, this.sessionId);
      default:
        throw new Error(
          `Cannot forward command to "${command.destination.type}" from "${this.constructor.type}".`
        );
    }
  }

  /**
   * If <var>realmId</var> is null or not provided get the realm for
   * a given <var>sandboxName</var>, otherwise find the realm
   * in the cache with the realm id equal given <var>realmId</var>.
   *
   * @param {object} options
   * @param {string|null=} options.realmId
   *     The realm id.
   * @param {string=} options.sandboxName
   *     The name of sandbox
   *
   * @returns {Realm}
   *     The realm object.
   */
  getRealm(options = {}) {
    const { realmId = null, sandboxName } = options;
    if (realmId === null) {
      return this.#getRealmFromSandboxName(sandboxName);
    }

    const realm = Array.from(this.#realms.values()).find(
      realm => realm.id === realmId
    );

    if (realm) {
      return realm;
    }

    throw new lazy.error.NoSuchFrameError(`Realm with id ${realmId} not found`);
  }

  matchesContext(contextDescriptor) {
    return (
      contextDescriptor.type === ContextDescriptorType.All ||
      (contextDescriptor.type === ContextDescriptorType.TopBrowsingContext &&
        contextDescriptor.id === this.context.browserId)
    );
  }

  /**
   * Send a command to the root MessageHandler.
   *
   * @param {Command} command
   *     The command to send to the root MessageHandler.
   * @returns {Promise}
   *     A promise which resolves with the return value of the command.
   */
  sendRootCommand(command) {
    return this.handleCommand({
      ...command,
      destination: {
        type: lazy.RootMessageHandler.type,
      },
    });
  }
}
