/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["browser", "Context", "WindowState"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppInfo: "chrome://remote/content/marionette/appinfo.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  MessageManagerDestroyedPromise: "chrome://remote/content/marionette/sync.js",
  waitForEvent: "chrome://remote/content/marionette/sync.js",
  WebElementEventTarget: "chrome://remote/content/marionette/dom.js",
  windowManager: "chrome://remote/content/shared/WindowManager.jsm",
});

/** @namespace */
this.browser = {};

/**
 * Variations of Marionette contexts.
 *
 * Choosing a context through the <tt>Marionette:SetContext</tt>
 * command directs all subsequent browsing context scoped commands
 * to that context.
 *
 * @class Marionette.Context
 */
class Context {
  /**
   * Gets the correct context from a string.
   *
   * @param {string} s
   *     Context string serialisation.
   *
   * @return {Context}
   *     Context.
   *
   * @throws {TypeError}
   *     If <var>s</var> is not a context.
   */
  static fromString(s) {
    switch (s) {
      case "chrome":
        return Context.Chrome;

      case "content":
        return Context.Content;

      default:
        throw new TypeError(`Unknown context: ${s}`);
    }
  }
}
Context.Chrome = "chrome";
Context.Content = "content";
this.Context = Context;

// GeckoView shim for Desktop's gBrowser
class MobileTabBrowser {
  constructor(window) {
    this.window = window;
  }

  get tabs() {
    return [this.window.tab];
  }

  get selectedTab() {
    return this.window.tab;
  }

  set selectedTab(tab) {
    if (tab != this.selectedTab) {
      throw new Error("GeckoView only supports a single tab");
    }

    // Synthesize a custom TabSelect event to indicate that a tab has been
    // selected even when we don't change it.
    const event = this.window.CustomEvent("TabSelect", {
      bubbles: true,
      cancelable: false,
      detail: {
        previousTab: this.selectedTab,
      },
    });
    this.window.document.dispatchEvent(event);
  }

  get selectedBrowser() {
    return this.selectedTab.linkedBrowser;
  }

  addEventListener() {
    this.window.addEventListener(...arguments);
  }

  removeEventListener() {
    this.window.removeEventListener(...arguments);
  }
}

/**
 * Get the <code>&lt;xul:browser&gt;</code> for the specified tab.
 *
 * @param {Tab} tab
 *     The tab whose browser needs to be returned.
 *
 * @return {Browser}
 *     The linked browser for the tab or null if no browser can be found.
 */
browser.getBrowserForTab = function(tab) {
  if (tab && "linkedBrowser" in tab) {
    return tab.linkedBrowser;
  }

  return null;
};

/**
 * Return the tab browser for the specified chrome window.
 *
 * @param {ChromeWindow} win
 *     Window whose <code>tabbrowser</code> needs to be accessed.
 *
 * @return {Tab}
 *     Tab browser or null if it's not a browser window.
 */
browser.getTabBrowser = function(window) {
  // GeckoView
  if (AppInfo.isAndroid) {
    return new MobileTabBrowser(window);
    // Firefox
  } else if ("gBrowser" in window) {
    return window.gBrowser;
    // Thunderbird
  } else if (window.document.getElementById("tabmail")) {
    return window.document.getElementById("tabmail");
  }

  return null;
};

/**
 * Creates a browsing context wrapper.
 *
 * Browsing contexts handle interactions with the browser, according to
 * the current environment.
 */
browser.Context = class {
  /**
   * @param {ChromeWindow} win
   *     ChromeWindow that contains the top-level browsing context.
   * @param {GeckoDriver} driver
   *     Reference to driver instance.
   */
  constructor(window, driver) {
    this.window = window;
    this.driver = driver;

    // In Firefox this is <xul:tabbrowser> (not <xul:browser>!)
    // and MobileTabBrowser in GeckoView.
    this.tabBrowser = browser.getTabBrowser(this.window);

    // Used to set curFrameId upon new session
    this.newSession = true;

    // A reference to the tab corresponding to the current window handle,
    // if any.  Specifically, this.tab refers to the last tab that Marionette
    // switched to in this browser window. Note that this may not equal the
    // currently selected tab.  For example, if Marionette switches to tab
    // A, and then clicks on a button that opens a new tab B in the same
    // browser window, this.tab will still point to tab A, despite tab B
    // being the currently selected tab.
    this.tab = null;
  }

  /**
   * Returns the content browser for the currently selected tab.
   * If there is no tab selected, null will be returned.
   */
  get contentBrowser() {
    if (this.tab) {
      return browser.getBrowserForTab(this.tab);
    } else if (
      this.tabBrowser &&
      this.driver.isReftestBrowser(this.tabBrowser)
    ) {
      return this.tabBrowser;
    }

    return null;
  }

  get messageManager() {
    if (this.contentBrowser) {
      return this.contentBrowser.messageManager;
    }

    return null;
  }

  /**
   * Checks if the browsing context has been discarded.
   *
   * The browsing context will have been discarded if the content
   * browser, represented by the <code>&lt;xul:browser&gt;</code>,
   * has been detached.
   *
   * @return {boolean}
   *     True if browsing context has been discarded, false otherwise.
   */
  get closed() {
    return this.contentBrowser === null;
  }

  /**
   * Gets the position and dimensions of the top-level browsing context.
   *
   * @return {Map.<string, number>}
   *     Object with |x|, |y|, |width|, and |height| properties.
   */
  get rect() {
    return {
      x: this.window.screenX,
      y: this.window.screenY,
      width: this.window.outerWidth,
      height: this.window.outerHeight,
    };
  }

  /**
   * Retrieves the current tabmodal UI object.  According to the browser
   * associated with the currently selected tab.
   */
  getTabModal() {
    let br = this.contentBrowser;
    if (!br.hasAttribute("tabmodalPromptShowing")) {
      return null;
    }

    // The modal is a direct sibling of the browser element.
    // See tabbrowser.xml's getTabModalPromptBox.
    let modalElements = br.parentNode.getElementsByTagName("tabmodalprompt");

    return br.tabModalPromptBox.getPrompt(modalElements[0]);
  }

  /**
   * Close the current window.
   *
   * @return {Promise}
   *     A promise which is resolved when the current window has been closed.
   */
  async closeWindow() {
    return windowManager.closeWindow(this.window);
  }

  /**
   * Focus the current window.
   *
   * @return {Promise}
   *     A promise which is resolved when the current window has been focused.
   */
  async focusWindow() {
    return windowManager.focusWindow(this.window);
  }

  /**
   * Open a new browser window.
   *
   * @return {Promise}
   *     A promise resolving to the newly created chrome window.
   */
  async openBrowserWindow(focus = false, isPrivate = false) {
    return windowManager.openBrowserWindow(this.window, focus, isPrivate);
  }

  /**
   * Close the current tab.
   *
   * @return {Promise}
   *     A promise which is resolved when the current tab has been closed.
   *
   * @throws UnsupportedOperationError
   *     If tab handling for the current application isn't supported.
   */
  closeTab() {
    // If the current window is not a browser then close it directly. Do the
    // same if only one remaining tab is open, or no tab selected at all.
    if (
      !this.tabBrowser ||
      !this.tabBrowser.tabs ||
      this.tabBrowser.tabs.length === 1 ||
      !this.tab
    ) {
      return this.closeWindow();
    }

    let destroyed = new MessageManagerDestroyedPromise(this.messageManager);
    let tabClosed;

    switch (AppInfo.name) {
      case "Firefox":
        tabClosed = waitForEvent(this.tab, "TabClose");
        this.tabBrowser.removeTab(this.tab);
        break;

      default:
        throw new error.UnsupportedOperationError(
          `closeTab() not supported in ${AppInfo.name}`
        );
    }

    return Promise.all([destroyed, tabClosed]);
  }

  /**
   * Open a new tab in the currently selected chrome window.
   */
  async openTab(focus = false) {
    let tab = null;

    switch (AppInfo.name) {
      case "Firefox":
        const opened = waitForEvent(this.window, "TabOpen");
        this.window.BrowserOpenTab();
        await opened;

        tab = this.tabBrowser.selectedTab;

        // The new tab is always selected by default. If focus is not wanted,
        // the previously tab needs to be selected again.
        if (!focus) {
          this.tabBrowser.selectedTab = this.tab;
        }

        break;

      default:
        throw new error.UnsupportedOperationError(
          `openTab() not supported in ${AppInfo.name}`
        );
    }

    return tab;
  }

  /**
   * Set the current tab.
   *
   * @param {number=} index
   *     Tab index to switch to. If the parameter is undefined,
   *     the currently selected tab will be used.
   * @param {ChromeWindow=} window
   *     Switch to this window before selecting the tab.
   * @param {boolean=} focus
   *      A boolean value which determins whether to focus
   *      the window. Defaults to true.
   *
   * @return {Tab}
   *     The selected tab.
   *
   * @throws UnsupportedOperationError
   *     If tab handling for the current application isn't supported.
   */
  async switchToTab(index, window = undefined, focus = true) {
    let currentTab = this.tabBrowser.selectedTab;

    if (window) {
      this.window = window;
      this.tabBrowser = browser.getTabBrowser(this.window);
    }

    if (!this.tabBrowser) {
      return null;
    }

    if (typeof index == "undefined") {
      this.tab = this.tabBrowser.selectedTab;
    } else {
      this.tab = this.tabBrowser.tabs[index];
    }

    if (focus && this.tab != currentTab) {
      const tabSelected = waitForEvent(this.window, "TabSelect");
      this.tabBrowser.selectedTab = this.tab;
      await tabSelected;
    }

    // TODO(ato): Currently tied to curBrowser, but should be moved to
    // WebElement when introduced by https://bugzil.la/1400256.
    this.eventObserver = new WebElementEventTarget(this.messageManager);

    return this.tab;
  }

  /**
   * Registers a new frame, and sets its current frame id to this frame
   * if it is not already assigned, and if a) we already have a session
   * or b) we're starting a new session and it is the right start frame.
   *
   * @param {xul:browser} target
   *     The <xul:browser> that was the target of the originating message.
   */
  register(target) {
    if (!this.tabBrowser) {
      return;
    }

    // If we're setting up a new session on Firefox, we only process the
    // registration for this frame if it belongs to the current tab.
    if (!this.tab) {
      this.switchToTab();
    }
  }
};

/**
 * Marionette representation of the {@link ChromeWindow} window state.
 *
 * @enum {string}
 */
const WindowState = {
  Maximized: "maximized",
  Minimized: "minimized",
  Normal: "normal",
  Fullscreen: "fullscreen",

  /**
   * Converts {@link nsIDOMChromeWindow.windowState} to WindowState.
   *
   * @param {number} windowState
   *     Attribute from {@link nsIDOMChromeWindow.windowState}.
   *
   * @return {WindowState}
   *     JSON representation.
   *
   * @throws {TypeError}
   *     If <var>windowState</var> was unknown.
   */
  from(windowState) {
    switch (windowState) {
      case 1:
        return WindowState.Maximized;

      case 2:
        return WindowState.Minimized;

      case 3:
        return WindowState.Normal;

      case 4:
        return WindowState.Fullscreen;

      default:
        throw new TypeError(`Unknown window state: ${windowState}`);
    }
  },
};
this.WindowState = WindowState;
