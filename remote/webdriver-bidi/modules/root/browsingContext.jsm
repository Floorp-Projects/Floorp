/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["browsingContext"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.jsm",
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
  waitForInitialNavigationCompleted:
    "chrome://remote/content/shared/Navigate.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.WEBDRIVER_BIDI)
);

class BrowsingContextModule extends Module {
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

  /**
   * Close the provided browsing context.
   *
   * @param {Object=} options
   * @param {string} context
   *     Id of the browsing context to close.
   *
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   * @throws {InvalidArgumentError}
   *     If the browsing context is not a top-level one.
   */
  close(options = {}) {
    const { context: contextId } = options;

    assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new error.NoSuchFrameError(
        `Browsing Context with id ${contextId} not found`
      );
    }

    if (context.parent) {
      throw new error.InvalidArgumentError(
        `Browsing Context with id ${contextId} is not top-level`
      );
    }

    if (TabManager.getTabCount() === 1) {
      // The behavior when closing the last tab is currently unspecified.
      // Warn the consumer about potential issues
      logger.warn(
        `Closing the last open tab (Browsing Context id ${contextId}), expect inconsistent behavior across platforms`
      );
    }

    const browser = context.embedderElement;
    const tabBrowser = TabManager.getTabBrowser(browser.ownerGlobal);
    const tab = tabBrowser.getTabForBrowser(browser);
    TabManager.removeTab(tab);
  }

  /**
   * An object that holds the WebDriver Bidi browsing context information.
   *
   * @typedef BrowsingContextInfo
   *
   * @property {string} context
   *     The id of the browsing context.
   * @property {string=} parent
   *     The parent of the browsing context if it's the root browsing context
   *     of the to be processed browsing context tree.
   * @property {string} url
   *     The current documents location.
   * @property {Array<BrowsingContextInfo>=} children
   *     List of child browsing contexts. Only set if maxDepth hasn't been
   *     reached yet.
   */

  /**
   * An object that holds the WebDriver Bidi browsing context tree information.
   *
   * @typedef BrowsingContextGetTreeResult
   *
   * @property {Array<BrowsingContextInfo>} contexts
   *     List of child browsing contexts.
   */

  /**
   * Returns a tree of all browsing contexts that are descendents of the
   * given context, or all top-level contexts when no parent is provided.
   *
   * @param {Object=} options
   * @param {number=} maxDepth
   *     Depth of the browsing context tree to traverse. If not specified
   *     the whole tree is returned.
   * @param {string=} parent
   *     Id of the parent browsing context.
   *
   * @returns {BrowsingContextGetTreeResult}
   *     Tree of browsing context information.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  getTree(options = {}) {
    const { maxDepth = null, parent: parentId = null } = options;

    if (maxDepth !== null) {
      assert.positiveInteger(
        maxDepth,
        `Expected "maxDepth" to be a positive integer, got ${maxDepth}`
      );
    }

    let contexts;
    if (parentId !== null) {
      // With a parent id specified return the context info for itself
      // and the full tree.

      assert.string(
        parentId,
        `Expected "parent" to be a string, got ${parentId}`
      );

      contexts = [this.#getBrowsingContext(parentId)];
    } else {
      // Return all top-level browsing contexts.
      contexts = TabManager.browsers.map(browser => browser.browsingContext);
    }

    const contextsInfo = contexts.map(context => {
      return this.#getBrowsingContextInfo(context, { maxDepth });
    });

    return { contexts: contextsInfo };
  }

  /**
   * Retrieves a browsing context based on its id.
   *
   * @param {Number} contextId
   *     Id of the browsing context.
   * @returns {BrowsingContext=}
   *     The browsing context or null if <var>contextId</var> is null.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  #getBrowsingContext(contextId) {
    // The WebDriver BiDi specification expects null to be
    // returned if no browsing context id has been specified.
    if (contextId === null) {
      return null;
    }

    const context = TabManager.getBrowsingContextById(contextId);
    if (context === null) {
      throw new error.NoSuchFrameError(
        `Browsing Context with id ${contextId} not found`
      );
    }

    return context;
  }

  /**
   * Get the WebDriver BiDi browsing context information.
   *
   * @param {BrowsingContext} context
   *     The browsing context to get the information from.
   * @param {Object=} options
   * @param {boolean=} isRoot
   *     Flag that indicates if this browsing context is the root of all the
   *     browsing contexts to be returned. Defaults to true.
   * @param {number=} maxDepth
   *     Depth of the browsing context tree to traverse. If not specified
   *     the whole tree is returned.
   * @returns {BrowsingContextInfo}
   *     The information about the browsing context.
   */
  #getBrowsingContextInfo(context, options = {}) {
    const { isRoot = true, maxDepth = null } = options;

    let children = null;
    if (maxDepth === null || maxDepth > 0) {
      children = context.children.map(context =>
        this.#getBrowsingContextInfo(context, {
          maxDepth: maxDepth === null ? maxDepth : maxDepth - 1,
          isRoot: false,
        })
      );
    }

    const contextInfo = {
      context: TabManager.getIdForBrowsingContext(context),
      url: context.currentURI.spec,
      children,
    };

    if (isRoot) {
      // Only emit the parent id for the top-most browsing context.
      const parentId = TabManager.getIdForBrowsingContext(context.parent);
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
      maxDepth: 0,
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

const browsingContext = BrowsingContextModule;
