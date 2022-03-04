/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["browsingContext"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.jsm",
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
  waitForInitialNavigationCompleted:
    "chrome://remote/content/shared/Navigate.jsm",
});

class BrowsingContext extends Module {
  #contextListener;

  /**
   * Create a new module instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this Module instance.
   */
  constructor(messageHandler) {
    super(messageHandler);

    // Create the console-api listener and listen on "message" events.
    this.#contextListener = new BrowsingContextListener();
    this.#contextListener.on("attached", this.#onContextAttached);
  }

  destroy() {
    this.#contextListener.off("attached", this.#onContextAttached);
    this.#contextListener.destroy();
  }

  #getBrowsingContextInfo(browsingContext, options = {}) {
    const { depth } = options;

    const contextInfo = {
      context: TabManager.getIdForBrowsingContext(browsingContext),
      url: browsingContext.currentURI.spec,
      children: null,
    };

    if (depth == 0) {
      // Only emit the parent id for the top-most browsing context.
      const parentId = TabManager.getIdForBrowsingContext(
        browsingContext.parent
      );
      contextInfo.parent = parentId;
    }

    return contextInfo;
  }

  #onContextAttached = async (eventName, data = {}) => {
    const { browsingContext, why } = data;

    // Filter out top-level browsing contexts that are created because of a
    // cross-group navigation.
    if (why === "replace") {
      return;
    }

    // Filter out notifications for chrome context until support gets
    // added (bug 1722679).
    if (!browsingContext.webProgress) {
      return;
    }

    // Wait until navigation starts, so that an active document is attached.
    await waitForInitialNavigationCompleted(browsingContext.webProgress, {
      resolveWhenStarted: true,
    });

    const contextInfo = this.#getBrowsingContextInfo(browsingContext, {
      depth: 0,
      maxDepth: 1,
    });
    this.emitProtocolEvent("browsingContext.contextCreated", contextInfo);
  };

  /**
   * Internal commands
   */

  _subscribeEvent(params) {
    // TODO: Bug 1741861. Move this logic to a shared module or the an abstract
    // class.
    switch (params.event) {
      case "browsingContext.contextCreated":
        this.#contextListener.startListening();

        return this.messageHandler.addSessionData({
          moduleName: "browsingContext",
          category: "event",
          contextDescriptor: {
            type: ContextDescriptorType.All,
          },
          values: [params.event],
        });
      default:
        throw new Error(
          `Unsupported event for browsingContext module ${params.event}`
        );
    }
  }

  _unsubscribeEvent(params) {
    switch (params.event) {
      case "browsingContext.contextCreated":
        this.#contextListener.stopListening();

        return this.messageHandler.removeSessionData({
          moduleName: "browsingContext",
          category: "event",
          contextDescriptor: {
            type: ContextDescriptorType.All,
          },
          values: [params.event],
        });
      default:
        throw new Error(
          `Unsupported event for browsingContext module ${params.event}`
        );
    }
  }

  static get supportedEvents() {
    return ["browsingContext.contextCreated"];
  }
}

const browsingContext = BrowsingContext;
