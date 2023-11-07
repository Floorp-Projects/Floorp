/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.sys.mjs",
  capture: "chrome://remote/content/shared/Capture.sys.mjs",
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  EventPromise: "chrome://remote/content/shared/Sync.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  modal: "chrome://remote/content/shared/Prompt.sys.mjs",
  registerNavigationId:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  NavigationListener:
    "chrome://remote/content/shared/listeners/NavigationListener.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  print: "chrome://remote/content/shared/PDF.sys.mjs",
  ProgressListener: "chrome://remote/content/shared/Navigate.sys.mjs",
  PromptListener:
    "chrome://remote/content/shared/listeners/PromptListener.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  waitForInitialNavigationCompleted:
    "chrome://remote/content/shared/Navigate.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

// Maximal window dimension allowed when emulating a viewport.
const MAX_WINDOW_SIZE = 10000000;

/**
 * @typedef {string} ClipRectangleType
 */

/**
 * Enum of possible clip rectangle types supported by the
 * browsingContext.captureScreenshot command.
 *
 * @readonly
 * @enum {ClipRectangleType}
 */
export const ClipRectangleType = {
  Box: "box",
  Element: "element",
};

/**
 * @typedef {object} CreateType
 */

/**
 * Enum of types supported by the browsingContext.create command.
 *
 * @readonly
 * @enum {CreateType}
 */
const CreateType = {
  tab: "tab",
  window: "window",
};

/**
 * @typedef {string} OriginType
 */

/**
 * Enum of origin type supported by the
 * browsingContext.captureScreenshot command.
 *
 * @readonly
 * @enum {OriginType}
 */
export const OriginType = {
  document: "document",
  viewport: "viewport",
};

/**
 * Enum of user prompt types supported by the browsingContext.handleUserPrompt
 * command, these types can be retrieved from `dialog.args.promptType`.
 *
 * @readonly
 * @enum {UserPromptType}
 */
const UserPromptType = {
  alert: "alert",
  confirm: "confirm",
  prompt: "prompt",
  beforeunload: "beforeunload",
};

/**
 * An object that contains details of a viewport.
 *
 * @typedef {object} Viewport
 *
 * @property {number} height
 *     The height of the viewport.
 * @property {number} width
 *     The width of the viewport.
 */

/**
 * @typedef {string} WaitCondition
 */

/**
 * Wait conditions supported by WebDriver BiDi for navigation.
 *
 * @enum {WaitCondition}
 */
const WaitCondition = {
  None: "none",
  Interactive: "interactive",
  Complete: "complete",
};

class BrowsingContextModule extends Module {
  #contextListener;
  #navigationListener;
  #promptListener;
  #subscribedEvents;

  /**
   * Create a new module instance.
   *
   * @param {MessageHandler} messageHandler
   *     The MessageHandler instance which owns this Module instance.
   */
  constructor(messageHandler) {
    super(messageHandler);

    this.#contextListener = new lazy.BrowsingContextListener();
    this.#contextListener.on("attached", this.#onContextAttached);
    this.#contextListener.on("discarded", this.#onContextDiscarded);

    // Create the navigation listener and listen to "navigation-started" and
    // "location-changed" events.
    this.#navigationListener = new lazy.NavigationListener(
      this.messageHandler.navigationManager
    );
    this.#navigationListener.on("location-changed", this.#onLocationChanged);
    this.#navigationListener.on(
      "navigation-started",
      this.#onNavigationStarted
    );

    // Create the prompt listener and listen to "closed" and "opened" events.
    this.#promptListener = new lazy.PromptListener();
    this.#promptListener.on("closed", this.#onPromptClosed);
    this.#promptListener.on("opened", this.#onPromptOpened);

    // Set of event names which have active subscriptions.
    this.#subscribedEvents = new Set();

    // Treat the event of moving a page to BFCache as context discarded event for iframes.
    this.messageHandler.on("windowglobal-pagehide", this.#onPageHideEvent);
  }

  destroy() {
    this.#contextListener.off("attached", this.#onContextAttached);
    this.#contextListener.off("discarded", this.#onContextDiscarded);
    this.#contextListener.destroy();

    this.#promptListener.off("closed", this.#onPromptClosed);
    this.#promptListener.off("opened", this.#onPromptOpened);
    this.#promptListener.destroy();

    this.#subscribedEvents = null;

    this.messageHandler.off("windowglobal-pagehide", this.#onPageHideEvent);
  }

  /**
   * Activates and focuses the given top-level browsing context.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async activate(options = {}) {
    const { context: contextId } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );
    const context = this.#getBrowsingContext(contextId);

    if (context.parent) {
      throw new lazy.error.InvalidArgumentError(
        `Browsing Context with id ${contextId} is not top-level`
      );
    }

    const tab = lazy.TabManager.getTabForBrowsingContext(context);
    const window = lazy.TabManager.getWindowForTab(tab);

    await lazy.windowManager.focusWindow(window);
    await lazy.TabManager.selectTab(tab);
  }

  /**
   * Used as an argument for browsingContext.captureScreenshot command, as one of the available variants
   * {BoxClipRectangle} or {ElementClipRectangle}, to represent a target of the command.
   *
   * @typedef ClipRectangle
   */

  /**
   * Used as an argument for browsingContext.captureScreenshot command
   * to represent a box which is going to be a target of the command.
   *
   * @typedef BoxClipRectangle
   *
   * @property {ClipRectangleType} [type=ClipRectangleType.Box]
   * @property {number} x
   * @property {number} y
   * @property {number} width
   * @property {number} height
   */

  /**
   * Used as an argument for browsingContext.captureScreenshot command
   * to represent an element which is going to be a target of the command.
   *
   * @typedef ElementClipRectangle
   *
   * @property {ClipRectangleType} [type=ClipRectangleType.Element]
   * @property {SharedReference} element
   */

  /**
   * Capture a base64-encoded screenshot of the provided browsing context.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to screenshot.
   * @param {ClipRectangle=} options.clip
   *     A box or an element of which a screenshot should be taken.
   *     If not present, take a screenshot of the whole viewport.
   * @param {OriginType=} options.origin
   *
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async captureScreenshot(options = {}) {
    const {
      clip = null,
      context: contextId,
      origin = OriginType.viewport,
    } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );
    const context = this.#getBrowsingContext(contextId);

    const originTypeValues = Object.values(OriginType);
    lazy.assert.that(
      value => originTypeValues.includes(value),
      `Expected "origin" to be one of ${originTypeValues}, got ${origin}`
    )(origin);

    if (clip !== null) {
      lazy.assert.object(clip, `Expected "clip" to be a object, got ${clip}`);

      const { type } = clip;
      switch (type) {
        case ClipRectangleType.Box: {
          const { x, y, width, height } = clip;

          lazy.assert.number(x, `Expected "x" to be a number, got ${x}`);
          lazy.assert.number(y, `Expected "y" to be a number, got ${y}`);
          lazy.assert.number(
            width,
            `Expected "width" to be a number, got ${width}`
          );
          lazy.assert.number(
            height,
            `Expected "height" to be a number, got ${height}`
          );

          break;
        }

        case ClipRectangleType.Element: {
          const { element } = clip;

          lazy.assert.object(
            element,
            `Expected "element" to be an object, got ${element}`
          );

          break;
        }

        default:
          throw new lazy.error.InvalidArgumentError(
            `Expected "type" to be one of ${Object.values(
              ClipRectangleType
            )}, got ${type}`
          );
      }
    }

    const rect = await this.messageHandler.handleCommand({
      moduleName: "browsingContext",
      commandName: "_getScreenshotRect",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {
        clip,
        origin,
      },
      retryOnAbort: true,
    });

    if (rect.width === 0 || rect.height === 0) {
      throw new lazy.error.UnableToCaptureScreen(
        `The dimensions of requested screenshot are incorrect, got width: ${rect.width} and height: ${rect.height}.`
      );
    }

    const canvas = await lazy.capture.canvas(
      context.topChromeWindow,
      context,
      rect.x,
      rect.y,
      rect.width,
      rect.height
    );

    return {
      data: lazy.capture.toBase64(canvas),
    };
  }

  /**
   * Close the provided browsing context.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to close.
   *
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   * @throws {InvalidArgumentError}
   *     If the browsing context is not a top-level one.
   */
  async close(options = {}) {
    const { context: contextId } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing Context with id ${contextId} not found`
      );
    }

    if (context.parent) {
      throw new lazy.error.InvalidArgumentError(
        `Browsing Context with id ${contextId} is not top-level`
      );
    }

    if (lazy.TabManager.getTabCount() === 1) {
      // The behavior when closing the last tab is currently unspecified.
      // Warn the consumer about potential issues
      lazy.logger.warn(
        `Closing the last open tab (Browsing Context id ${contextId}), expect inconsistent behavior across platforms`
      );
    }

    const tab = lazy.TabManager.getTabForBrowsingContext(context);
    await lazy.TabManager.removeTab(tab);
  }

  /**
   * Create a new browsing context using the provided type "tab" or "window".
   *
   * @param {object=} options
   * @param {boolean=} options.background
   *     Whether the tab/window should be open in the background. Defaults to false,
   *     which means that the tab/window will be open in the foreground.
   * @param {string=} options.referenceContext
   *     Id of the top-level browsing context to use as reference.
   *     If options.type is "tab", the new tab will open in the same window as
   *     the reference context, and will be added next to the reference context.
   *     If options.type is "window", the reference context is ignored.
   * @param {CreateType} options.type
   *     Type of browsing context to create.
   *
   * @throws {InvalidArgumentError}
   *     If the browsing context is not a top-level one.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async create(options = {}) {
    const {
      background = false,
      referenceContext: referenceContextId = null,
      type,
    } = options;
    if (type !== CreateType.tab && type !== CreateType.window) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "type" to be one of ${Object.values(CreateType)}, got ${type}`
      );
    }

    lazy.assert.boolean(
      background,
      lazy.pprint`Expected "background" to be a boolean, got ${background}`
    );

    let browser;

    // Since each tab in GeckoView has its own Gecko instance running,
    // which means also its own window object, for Android we will need to focus
    // a previously focused window in case of opening the tab in the background.
    const previousWindow = Services.wm.getMostRecentBrowserWindow();
    const previousTab =
      lazy.TabManager.getTabBrowser(previousWindow).selectedTab;

    switch (type) {
      case "window":
        const newWindow = await lazy.windowManager.openBrowserWindow({
          focus: !background,
        });
        browser = lazy.TabManager.getTabBrowser(newWindow).selectedBrowser;
        break;

      case "tab":
        if (!lazy.TabManager.supportsTabs()) {
          throw new lazy.error.UnsupportedOperationError(
            `browsingContext.create with type "tab" not supported in ${lazy.AppInfo.name}`
          );
        }

        let referenceTab;
        if (referenceContextId !== null) {
          lazy.assert.string(
            referenceContextId,
            lazy.pprint`Expected "referenceContext" to be a string, got ${referenceContextId}`
          );

          const referenceBrowsingContext =
            lazy.TabManager.getBrowsingContextById(referenceContextId);
          if (!referenceBrowsingContext) {
            throw new lazy.error.NoSuchFrameError(
              `Browsing Context with id ${referenceContextId} not found`
            );
          }

          if (referenceBrowsingContext.parent) {
            throw new lazy.error.InvalidArgumentError(
              `referenceContext with id ${referenceContextId} is not a top-level browsing context`
            );
          }

          referenceTab = lazy.TabManager.getTabForBrowsingContext(
            referenceBrowsingContext
          );
        }

        const tab = await lazy.TabManager.addTab({
          focus: !background,
          referenceTab,
        });
        browser = lazy.TabManager.getBrowserForTab(tab);
    }

    await lazy.waitForInitialNavigationCompleted(
      browser.browsingContext.webProgress,
      {
        unloadTimeout: 5000,
      }
    );

    // The tab on Android is always opened in the foreground,
    // so we need to select the previous tab,
    // and we have to wait until is fully loaded.
    // TODO: Bug 1845559. This workaround can be removed,
    // when the API to create a tab for Android supports the background option.
    if (lazy.AppInfo.isAndroid && background) {
      await lazy.windowManager.focusWindow(previousWindow);
      await lazy.TabManager.selectTab(previousTab);
    }

    // Force a reflow by accessing `clientHeight` (see Bug 1847044).
    browser.parentElement.clientHeight;

    return {
      context: lazy.TabManager.getIdForBrowser(browser),
    };
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
   * given context, or all top-level contexts when no root is provided.
   *
   * @param {object=} options
   * @param {number=} options.maxDepth
   *     Depth of the browsing context tree to traverse. If not specified
   *     the whole tree is returned.
   * @param {string=} options.root
   *     Id of the root browsing context.
   *
   * @returns {BrowsingContextGetTreeResult}
   *     Tree of browsing context information.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  getTree(options = {}) {
    const { maxDepth = null, root: rootId = null } = options;

    if (maxDepth !== null) {
      lazy.assert.positiveInteger(
        maxDepth,
        `Expected "maxDepth" to be a positive integer, got ${maxDepth}`
      );
    }

    let contexts;
    if (rootId !== null) {
      // With a root id specified return the context info for itself
      // and the full tree.
      lazy.assert.string(
        rootId,
        `Expected "root" to be a string, got ${rootId}`
      );
      contexts = [this.#getBrowsingContext(rootId)];
    } else {
      // Return all top-level browsing contexts.
      contexts = lazy.TabManager.browsers.map(
        browser => browser.browsingContext
      );
    }

    const contextsInfo = contexts.map(context => {
      return this.#getBrowsingContextInfo(context, { maxDepth });
    });

    return { contexts: contextsInfo };
  }

  /**
   * Closes an open prompt.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context.
   * @param {boolean=} options.accept
   *     Whether user prompt should be accepted or dismissed.
   *     Defaults to true.
   * @param {string=} options.userText
   *     Input to the user prompt's value field.
   *     Defaults to an empty string.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchAlertError}
   *     If there is no current user prompt.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   * @throws {UnsupportedOperationError}
   *     Raised when the command is called for "beforeunload" prompt.
   */
  async handleUserPrompt(options = {}) {
    const { accept = true, context: contextId, userText = "" } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = this.#getBrowsingContext(contextId);

    lazy.assert.boolean(
      accept,
      `Expected "accept" to be a boolean, got ${accept}`
    );

    lazy.assert.string(
      userText,
      `Expected "userText" to be a string, got ${userText}`
    );

    const tab = lazy.TabManager.getTabForBrowsingContext(context);
    const browser = lazy.TabManager.getBrowserForTab(tab);
    const window = lazy.TabManager.getWindowForTab(tab);
    const dialog = lazy.modal.findPrompt({
      window,
      contentBrowser: browser,
    });

    const closePrompt = async callback => {
      const dialogClosed = new lazy.EventPromise(
        window,
        "DOMModalDialogClosed"
      );
      callback();
      await dialogClosed;
    };

    if (dialog && dialog.isOpen) {
      switch (dialog.promptType) {
        case UserPromptType.alert: {
          await closePrompt(() => dialog.accept());
          return;
        }
        case UserPromptType.confirm: {
          await closePrompt(() => {
            if (accept) {
              dialog.accept();
            } else {
              dialog.dismiss();
            }
          });

          return;
        }
        case UserPromptType.prompt: {
          await closePrompt(() => {
            if (accept) {
              dialog.text = userText;
              dialog.accept();
            } else {
              dialog.dismiss();
            }
          });

          return;
        }
        case UserPromptType.beforeunload: {
          // TODO: Bug 1824220. Implement support for "beforeunload" prompts.
          throw new lazy.error.UnsupportedOperationError(
            '"beforeunload" prompts are not supported yet.'
          );
        }
      }
    }

    throw new lazy.error.NoSuchAlertError();
  }

  /**
   * An object that holds the WebDriver Bidi navigation information.
   *
   * @typedef BrowsingContextNavigateResult
   *
   * @property {string} navigation
   *     Unique id for this navigation.
   * @property {string} url
   *     The requested or reached URL.
   */

  /**
   * Navigate the given context to the provided url, with the provided wait condition.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to navigate.
   * @param {string} options.url
   *     Url for the navigation.
   * @param {WaitCondition=} options.wait
   *     Wait condition for the navigation, one of "none", "interactive", "complete".
   *
   * @returns {BrowsingContextNavigateResult}
   *     Navigation result.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchFrameError}
   *     If the browsing context for context cannot be found.
   */
  async navigate(options = {}) {
    const { context: contextId, url, wait = WaitCondition.None } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    lazy.assert.string(url, `Expected "url" to be string, got ${url}`);

    const waitConditions = Object.values(WaitCondition);
    if (!waitConditions.includes(wait)) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "wait" to be one of ${waitConditions}, got ${wait}`
      );
    }

    const context = this.#getBrowsingContext(contextId);

    // webProgress will be stable even if the context navigates, retrieve it
    // immediately before doing any asynchronous call.
    const webProgress = context.webProgress;

    const base = await this.messageHandler.handleCommand({
      moduleName: "browsingContext",
      commandName: "_getBaseURL",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      retryOnAbort: true,
    });

    let targetURI;
    try {
      const baseURI = Services.io.newURI(base);
      targetURI = Services.io.newURI(url, null, baseURI);
    } catch (e) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "url" to be a valid URL (${e.message})`
      );
    }

    return this.#awaitNavigation(
      webProgress,
      () => {
        context.loadURI(targetURI, {
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_IS_LINK,
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
          hasValidUserGestureActivation: true,
        });
      },
      {
        wait,
      }
    );
  }

  /**
   * An object that holds the information about margins
   * for Webdriver BiDi browsingContext.print command.
   *
   * @typedef BrowsingContextPrintMarginParameters
   *
   * @property {number=} bottom
   *     Bottom margin in cm. Defaults to 1cm (~0.4 inches).
   * @property {number=} left
   *     Left margin in cm. Defaults to 1cm (~0.4 inches).
   * @property {number=} right
   *     Right margin in cm. Defaults to 1cm (~0.4 inches).
   * @property {number=} top
   *     Top margin in cm. Defaults to 1cm (~0.4 inches).
   */

  /**
   * An object that holds the information about paper size
   * for Webdriver BiDi browsingContext.print command.
   *
   * @typedef BrowsingContextPrintPageParameters
   *
   * @property {number=} height
   *     Paper height in cm. Defaults to US letter height (27.94cm / 11 inches).
   * @property {number=} width
   *     Paper width in cm. Defaults to US letter width (21.59cm / 8.5 inches).
   */

  /**
   * Used as return value for Webdriver BiDi browsingContext.print command.
   *
   * @typedef BrowsingContextPrintResult
   *
   * @property {string} data
   *     Base64 encoded PDF representing printed document.
   */

  /**
   * Creates a paginated PDF representation of a document
   * of the provided browsing context, and returns it
   * as a Base64-encoded string.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context.
   * @param {boolean=} options.background
   *     Whether or not to print background colors and images.
   *     Defaults to false, which prints without background graphics.
   * @param {BrowsingContextPrintMarginParameters=} options.margin
   *     Paper margins.
   * @param {('landscape'|'portrait')=} options.orientation
   *     Paper orientation. Defaults to 'portrait'.
   * @param {BrowsingContextPrintPageParameters=} options.page
   *     Paper size.
   * @param {Array<number|string>=} options.pageRanges
   *     Paper ranges to print, e.g., ['1-5', 8, '11-13'].
   *     Defaults to the empty array, which means print all pages.
   * @param {number=} options.scale
   *     Scale of the webpage rendering. Defaults to 1.0.
   * @param {boolean=} options.shrinkToFit
   *     Whether or not to override page size as defined by CSS.
   *     Defaults to true, in which case the content will be scaled
   *     to fit the paper size.
   *
   * @returns {BrowsingContextPrintResult}
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async print(options = {}) {
    const {
      context: contextId,
      background,
      margin,
      orientation,
      page,
      pageRanges,
      scale,
      shrinkToFit,
    } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );
    const context = this.#getBrowsingContext(contextId);

    const settings = lazy.print.addDefaultSettings({
      background,
      margin,
      orientation,
      page,
      pageRanges,
      scale,
      shrinkToFit,
    });

    for (const prop of ["top", "bottom", "left", "right"]) {
      lazy.assert.positiveNumber(
        settings.margin[prop],
        lazy.pprint`margin.${prop} is not a positive number`
      );
    }
    for (const prop of ["width", "height"]) {
      lazy.assert.positiveNumber(
        settings.page[prop],
        lazy.pprint`page.${prop} is not a positive number`
      );
    }
    lazy.assert.positiveNumber(
      settings.scale,
      `scale ${settings.scale} is not a positive number`
    );
    lazy.assert.that(
      scale =>
        scale >= lazy.print.minScaleValue && scale <= lazy.print.maxScaleValue,
      `scale ${settings.scale} is outside the range ${lazy.print.minScaleValue}-${lazy.print.maxScaleValue}`
    )(settings.scale);
    lazy.assert.boolean(settings.shrinkToFit);
    lazy.assert.that(
      orientation => lazy.print.defaults.orientationValue.includes(orientation),
      `orientation ${
        settings.orientation
      } doesn't match allowed values "${lazy.print.defaults.orientationValue.join(
        "/"
      )}"`
    )(settings.orientation);
    lazy.assert.boolean(
      settings.background,
      `background ${settings.background} is not boolean`
    );
    lazy.assert.array(settings.pageRanges);

    const printSettings = await lazy.print.getPrintSettings(settings);
    const binaryString = await lazy.print.printToBinaryString(
      context,
      printSettings
    );

    return {
      data: btoa(binaryString),
    };
  }

  /**
   * Reload the given context's document, with the provided wait condition.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to navigate.
   * @param {bool=} options.ignoreCache
   *     If true ignore the browser cache. [Not yet supported]
   * @param {WaitCondition=} options.wait
   *     Wait condition for the navigation, one of "none", "interactive", "complete".
   *
   * @returns {BrowsingContextNavigateResult}
   *     Navigation result.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchFrameError}
   *     If the browsing context for context cannot be found.
   */
  async reload(options = {}) {
    const {
      context: contextId,
      ignoreCache,
      wait = WaitCondition.None,
    } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    if (typeof ignoreCache != "undefined") {
      throw new lazy.error.UnsupportedOperationError(
        `Argument "ignoreCache" is not supported yet.`
      );
    }

    const waitConditions = Object.values(WaitCondition);
    if (!waitConditions.includes(wait)) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "wait" to be one of ${waitConditions}, got ${wait}`
      );
    }

    const context = this.#getBrowsingContext(contextId);

    // webProgress will be stable even if the context navigates, retrieve it
    // immediately before doing any asynchronous call.
    const webProgress = context.webProgress;

    return this.#awaitNavigation(
      webProgress,
      () => {
        context.reload(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
      },
      { wait }
    );
  }

  /**
   * Set the top-level browsing context's viewport to a given dimension.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context.
   * @param {Viewport|null} options.viewport
   *     Dimensions to set the viewport to, or `null` to reset it
   *     to the original dimensions.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws UnsupportedOperationError
   *     Raised when the command is called on Android.
   */
  async setViewport(options = {}) {
    const { context: contextId, viewport } = options;

    if (lazy.AppInfo.isAndroid) {
      // Bug 1840084: Add Android support for modifying the viewport.
      throw new lazy.error.UnsupportedOperationError(
        `Command not yet supported for ${lazy.AppInfo.name}`
      );
    }

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = this.#getBrowsingContext(contextId);
    if (context.parent) {
      throw new lazy.error.InvalidArgumentError(
        `Browsing Context with id ${contextId} is not top-level`
      );
    }
    const browser = context.embedderElement;

    if (typeof viewport !== "object") {
      throw new lazy.error.InvalidArgumentError(
        `Expected "viewport" to be an object or null, got ${viewport}`
      );
    }

    let targetHeight, targetWidth;
    if (viewport !== null) {
      const { height, width } = viewport;

      targetHeight = lazy.assert.positiveInteger(
        height,
        `Expected "height" to be a positive integer, got ${height}`
      );
      targetWidth = lazy.assert.positiveInteger(
        width,
        `Expected "width" to be a positive integer, got ${width}`
      );

      if (targetHeight > MAX_WINDOW_SIZE || targetWidth > MAX_WINDOW_SIZE) {
        throw new lazy.error.UnsupportedOperationError(
          `"width" or "height" cannot be larger than ${MAX_WINDOW_SIZE} px`
        );
      }

      browser.style.setProperty("height", targetHeight + "px");
      browser.style.setProperty("width", targetWidth + "px");
    } else {
      // Reset viewport to the original dimensions
      targetHeight = browser.parentElement.clientHeight;
      targetWidth = browser.parentElement.clientWidth;

      browser.style.removeProperty("height");
      browser.style.removeProperty("width");
    }

    // Wait until the viewport has been resized
    await this.messageHandler.forwardCommand({
      moduleName: "browsingContext",
      commandName: "_awaitViewportDimensions",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {
        height: targetHeight,
        width: targetWidth,
      },
    });
  }

  /**
   * Start and await a navigation on the provided BrowsingContext. Returns a
   * promise which resolves when the navigation is done according to the provided
   * navigation strategy.
   *
   * @param {WebProgress} webProgress
   *     The WebProgress instance to observe for this navigation.
   * @param {Function} startNavigationFn
   *     A callback that starts a navigation.
   * @param {object} options
   * @param {WaitCondition} options.wait
   *     The WaitCondition to use to wait for the navigation.
   *
   * @returns {Promise<BrowsingContextNavigateResult>}
   *     A Promise that resolves to navigate results when the navigation is done.
   */
  async #awaitNavigation(webProgress, startNavigationFn, options) {
    const { wait } = options;

    const context = webProgress.browsingContext;
    const browserId = context.browserId;

    const resolveWhenStarted = wait === WaitCondition.None;
    const listener = new lazy.ProgressListener(webProgress, {
      expectNavigation: true,
      resolveWhenStarted,
      // In case the webprogress is already navigating, always wait for an
      // explicit start flag.
      waitForExplicitStart: true,
    });

    const onDocumentInteractive = (evtName, wrappedEvt) => {
      if (webProgress.browsingContext.id !== wrappedEvt.contextId) {
        // Ignore load events for unrelated browsing contexts.
        return;
      }

      if (wrappedEvt.readyState === "interactive") {
        listener.stopIfStarted();
      }
    };

    const contextDescriptor = {
      type: lazy.ContextDescriptorType.TopBrowsingContext,
      id: browserId,
    };

    // For the Interactive wait condition, resolve as soon as
    // the document becomes interactive.
    if (wait === WaitCondition.Interactive) {
      await this.messageHandler.eventsDispatcher.on(
        "browsingContext._documentInteractive",
        contextDescriptor,
        onDocumentInteractive
      );
    }

    const navigated = listener.start();
    navigated.finally(async () => {
      if (listener.isStarted) {
        listener.stop();
      }

      if (wait === WaitCondition.Interactive) {
        await this.messageHandler.eventsDispatcher.off(
          "browsingContext._documentInteractive",
          contextDescriptor,
          onDocumentInteractive
        );
      }
    });

    const navigationId = lazy.registerNavigationId({
      contextDetails: { context: webProgress.browsingContext },
    });

    await startNavigationFn();
    await navigated;

    let url;
    if (wait === WaitCondition.None) {
      // If wait condition is None, the navigation resolved before the current
      // context has navigated.
      url = listener.targetURI.spec;
    } else {
      url = listener.currentURI.spec;
    }

    return {
      navigation: navigationId,
      url,
    };
  }

  /**
   * Retrieves a browsing context based on its id.
   *
   * @param {number} contextId
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

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (context === null) {
      throw new lazy.error.NoSuchFrameError(
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
   * @param {object=} options
   * @param {boolean=} options.isRoot
   *     Flag that indicates if this browsing context is the root of all the
   *     browsing contexts to be returned. Defaults to true.
   * @param {number=} options.maxDepth
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
      context: lazy.TabManager.getIdForBrowsingContext(context),
      url: context.currentURI.spec,
      children,
    };

    if (isRoot) {
      // Only emit the parent id for the top-most browsing context.
      const parentId = lazy.TabManager.getIdForBrowsingContext(context.parent);
      contextInfo.parent = parentId;
    }

    return contextInfo;
  }

  #onContextAttached = async (eventName, data = {}) => {
    if (this.#subscribedEvents.has("browsingContext.contextCreated")) {
      const { browsingContext, why } = data;

      // Filter out top-level browsing contexts that are created because of a
      // cross-group navigation.
      if (why === "replace") {
        return;
      }

      // TODO: Bug 1852941. We should also filter out events which are emitted
      // for DevTools frames.

      // Filter out notifications for chrome context until support gets
      // added (bug 1722679).
      if (!browsingContext.webProgress) {
        return;
      }

      const browsingContextInfo = this.#getBrowsingContextInfo(
        browsingContext,
        {
          maxDepth: 0,
        }
      );

      // This event is emitted from the parent process but for a given browsing
      // context. Set the event's contextInfo to the message handler corresponding
      // to this browsing context.
      const contextInfo = {
        contextId: browsingContext.id,
        type: lazy.WindowGlobalMessageHandler.type,
      };
      this.emitEvent(
        "browsingContext.contextCreated",
        browsingContextInfo,
        contextInfo
      );
    }
  };

  #onContextDiscarded = async (eventName, data = {}) => {
    if (this.#subscribedEvents.has("browsingContext.contextDestroyed")) {
      const { browsingContext, why } = data;

      // Filter out top-level browsing contexts that are destroyed because of a
      // cross-group navigation.
      if (why === "replace") {
        return;
      }

      // TODO: Bug 1852941. We should also filter out events which are emitted
      // for DevTools frames.

      // Filter out notifications for chrome context until support gets
      // added (bug 1722679).
      if (!browsingContext.webProgress) {
        return;
      }

      // If this event is for a child context whose top or parent context is also destroyed,
      // we don't need to send it, in this case the event for the top/parent context is enough.
      if (
        browsingContext.parent &&
        (browsingContext.top.isDiscarded || browsingContext.parent.isDiscarded)
      ) {
        return;
      }

      const browsingContextInfo = this.#getBrowsingContextInfo(
        browsingContext,
        {
          maxDepth: 0,
        }
      );

      // This event is emitted from the parent process but for a given browsing
      // context. Set the event's contextInfo to the message handler corresponding
      // to this browsing context.
      const contextInfo = {
        contextId: browsingContext.id,
        type: lazy.WindowGlobalMessageHandler.type,
      };
      this.emitEvent(
        "browsingContext.contextDestroyed",
        browsingContextInfo,
        contextInfo
      );
    }
  };

  #onLocationChanged = async (eventName, data) => {
    const { navigationId, navigableId, url } = data;
    const context = this.#getBrowsingContext(navigableId);

    if (this.#subscribedEvents.has("browsingContext.fragmentNavigated")) {
      const contextInfo = {
        contextId: context.id,
        type: lazy.WindowGlobalMessageHandler.type,
      };
      this.emitEvent(
        "browsingContext.fragmentNavigated",
        {
          context: navigableId,
          navigation: navigationId,
          timestamp: Date.now(),
          url,
        },
        contextInfo
      );
    }
  };

  #onPromptClosed = async (eventName, data) => {
    if (this.#subscribedEvents.has("browsingContext.userPromptClosed")) {
      const { contentBrowser, detail } = data;
      const contextId = lazy.TabManager.getIdForBrowser(contentBrowser);

      if (contextId === null) {
        return;
      }

      // This event is emitted from the parent process but for a given browsing
      // context. Set the event's contextInfo to the message handler corresponding
      // to this browsing context.
      const contextInfo = {
        contextId,
        type: lazy.WindowGlobalMessageHandler.type,
      };

      const params = {
        context: contextId,
        ...detail,
      };

      this.emitEvent("browsingContext.userPromptClosed", params, contextInfo);
    }
  };

  #onPromptOpened = async (eventName, data) => {
    if (this.#subscribedEvents.has("browsingContext.userPromptOpened")) {
      const { contentBrowser, prompt } = data;

      // Do not send opened event for unsupported prompt types.
      if (!(prompt.promptType in UserPromptType)) {
        return;
      }

      const contextId = lazy.TabManager.getIdForBrowser(contentBrowser);
      // This event is emitted from the parent process but for a given browsing
      // context. Set the event's contextInfo to the message handler corresponding
      // to this browsing context.
      const contextInfo = {
        contextId,
        type: lazy.WindowGlobalMessageHandler.type,
      };

      const eventPayload = {
        context: contextId,
        type: prompt.promptType,
        message: await prompt.getText(),
      };

      // Bug 1859814: Since the platform doesn't provide the access to the `defaultValue` of the prompt,
      // we use prompt the `value` instead. The `value` is set to `defaultValue` when `defaultValue` is provided.
      // This approach doesn't allow us to distinguish between the `defaultValue` being set to an empty string and
      // `defaultValue` not set, because `value` is always defaulted to an empty string.
      // We should switch to using the actual `defaultValue` when it's available and check for the `null` here.
      const defaultValue = await prompt.getInputText();
      if (defaultValue) {
        eventPayload.defaultValue = defaultValue;
      }

      this.emitEvent(
        "browsingContext.userPromptOpened",
        eventPayload,
        contextInfo
      );
    }
  };

  #onNavigationStarted = async (eventName, data) => {
    const { navigableId, navigationId, url } = data;
    const context = this.#getBrowsingContext(navigableId);

    if (this.#subscribedEvents.has("browsingContext.navigationStarted")) {
      const contextInfo = {
        contextId: context.id,
        type: lazy.WindowGlobalMessageHandler.type,
      };

      this.emitEvent(
        "browsingContext.navigationStarted",
        {
          context: navigableId,
          navigation: navigationId,
          timestamp: Date.now(),
          url,
        },
        contextInfo
      );
    }
  };

  #onPageHideEvent = (name, eventPayload) => {
    const { context } = eventPayload;
    if (context.parent) {
      this.#onContextDiscarded("windowglobal-pagehide", {
        browsingContext: context,
      });
    }
  };

  #stopListeningToNavigationEvent(event) {
    this.#subscribedEvents.delete(event);

    const hasNavigationEvent =
      this.#subscribedEvents.has("browsingContext.fragmentNavigated") ||
      this.#subscribedEvents.has("browsingContext.navigationStarted");

    if (!hasNavigationEvent) {
      this.#navigationListener.stopListening();
    }
  }

  #stopListeningToPromptEvent(event) {
    this.#subscribedEvents.delete(event);

    const hasPromptEvent =
      this.#subscribedEvents.has("browsingContext.userPromptClosed") ||
      this.#subscribedEvents.has("browsingContext.userPromptOpened");

    if (!hasPromptEvent) {
      this.#promptListener.stopListening();
    }
  }

  #subscribeEvent(event) {
    switch (event) {
      case "browsingContext.contextCreated":
      case "browsingContext.contextDestroyed": {
        this.#contextListener.startListening();
        this.#subscribedEvents.add(event);
        break;
      }
      case "browsingContext.fragmentNavigated":
      case "browsingContext.navigationStarted": {
        this.#navigationListener.startListening();
        this.#subscribedEvents.add(event);
        break;
      }
      case "browsingContext.userPromptClosed":
      case "browsingContext.userPromptOpened": {
        this.#promptListener.startListening();
        this.#subscribedEvents.add(event);
        break;
      }
    }
  }

  #unsubscribeEvent(event) {
    switch (event) {
      case "browsingContext.contextCreated":
      case "browsingContext.contextDestroyed": {
        this.#contextListener.stopListening();
        this.#subscribedEvents.delete(event);
        break;
      }
      case "browsingContext.fragmentNavigated":
      case "browsingContext.navigationStarted": {
        this.#stopListeningToNavigationEvent();
        this.#subscribedEvents.delete(event);
        break;
      }
      case "browsingContext.userPromptClosed":
      case "browsingContext.userPromptOpened": {
        this.#stopListeningToPromptEvent(event);
        break;
      }
    }
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData.
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }

  static get supportedEvents() {
    return [
      "browsingContext.contextCreated",
      "browsingContext.contextDestroyed",
      "browsingContext.domContentLoaded",
      "browsingContext.fragmentNavigated",
      "browsingContext.load",
      "browsingContext.navigationStarted",
      "browsingContext.userPromptClosed",
      "browsingContext.userPromptOpened",
    ];
  }
}

export const browsingContext = BrowsingContextModule;
