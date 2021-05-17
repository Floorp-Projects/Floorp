/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["windowManager"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  AppInfo: "chrome://marionette/content/appinfo.js",
  browser: "chrome://marionette/content/browser.js",
  error: "chrome://marionette/content/error.js",
  Log: "chrome://marionette/content/log.js",
  waitForEvent: "chrome://marionette/content/sync.js",
  waitForObserverTopic: "chrome://marionette/content/sync.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

/**
 * Provides helpers to interact with Window objects.
 *
 * @class WindowManager
 */
class WindowManager {
  constructor() {
    // Maps browser's permanentKey to uuid: WeakMap.<Object, string>
    this._windowHandles = new WeakMap();
    // Maps ChromeWindow to uuid: WeakMap.<Object, string>
    this._chromeWindowHandles = new WeakMap();
  }

  get windowHandles() {
    const windowHandles = [];

    for (const win of this.windows) {
      const tabBrowser = browser.getTabBrowser(win);

      // Only return handles for browser windows
      if (tabBrowser && tabBrowser.tabs) {
        for (const tab of tabBrowser.tabs) {
          const winId = this.getIdForBrowser(browser.getBrowserForTab(tab));
          if (winId !== null) {
            windowHandles.push(winId);
          }
        }
      }
    }

    return windowHandles;
  }

  get chromeWindowHandles() {
    const chromeWindowHandles = [];

    for (const win of this.windows) {
      chromeWindowHandles.push(this.getIdForWindow(win));
    }

    return chromeWindowHandles;
  }

  get windows() {
    return Services.wm.getEnumerator(null);
  }

  /**
   * Find a specific window matching the provided window handle.
   *
   * @param {String} handle
   *     The unique handle of either a chrome window or a content browser, as
   *     returned by :js:func:`#getIdForBrowser` or :js:func:`#getIdForWindow`.
   *
   * @return {Object} A window properties object,
   *     @see :js:func:`GeckoDriver#getWindowProperties`
   */
  findWindowByHandle(handle) {
    for (const win of this.windows) {
      // In case the wanted window is a chrome window, we are done.
      const chromeWindowId = this.getIdForWindow(win);
      if (chromeWindowId == handle) {
        return this.getWindowProperties(win);
      }

      // Otherwise check if the chrome window has a tab browser, and that it
      // contains a tab with the wanted window handle.
      const tabBrowser = browser.getTabBrowser(win);
      if (tabBrowser && tabBrowser.tabs) {
        for (let i = 0; i < tabBrowser.tabs.length; ++i) {
          let contentBrowser = browser.getBrowserForTab(tabBrowser.tabs[i]);
          let contentWindowId = this.getIdForBrowser(contentBrowser);

          if (contentWindowId == handle) {
            return this.getWindowProperties(win, { tabIndex: i });
          }
        }
      }
    }

    return null;
  }

  /**
   * A set of properties describing a window and that should allow to uniquely
   * identify it. The described window can either be a Chrome Window or a
   * Content Window.
   *
   * @typedef {Object} WindowProperties
   * @property {Window} win - The Chrome Window containing the window.
   *     When describing a Chrome Window, this is the window itself.
   * @property {String} id - The unique id of the containing Chrome Window.
   * @property {Boolean} hasTabBrowser - `true` if the Chrome Window has a
   *     tabBrowser.
   * @property {Number} tabIndex - Optional, the index of the specific tab
   *     within the window.
   */

  /**
   * Returns a WindowProperties object, that can be used with :js:func:`GeckoDriver#setWindowHandle`.
   *
   * @param {Window} win
   *     The Chrome Window for which we want to create a properties object.
   * @param {Object} options
   * @param {Number} options.tabIndex
   *     Tab index of a specific Content Window in the specified Chrome Window.
   * @return {WindowProperties} A window properties object.
   */
  getWindowProperties(win, options = {}) {
    if (!(win instanceof Window)) {
      throw new TypeError("Invalid argument, expected a Window object");
    }

    return {
      win,
      id: this.getIdForWindow(win),
      hasTabBrowser: !!browser.getTabBrowser(win),
      tabIndex: options.tabIndex,
    };
  }

  /**
   * Retrieves an id for the given xul browser element. The id is a dynamically
   * generated uuid associated with the permanentKey property of the given
   * browser element.
   *
   * @param {xul:browser} browserElement
   *     The <xul:browser> for which we want to retrieve the id.
   * @return {String} The unique id for this browser.
   */
  getIdForBrowser(browserElement) {
    if (browserElement === null) {
      return null;
    }

    const key = browserElement.permanentKey;
    if (!this._windowHandles.has(key)) {
      const uuid = uuidGen.generateUUID().toString();
      this._windowHandles.set(key, uuid.substring(1, uuid.length - 1));
    }
    return this._windowHandles.get(key);
  }

  /**
   * Retrieves an id for the given chrome window. The id is a dynamically
   * generated uuid associated with the window object.
   *
   * @param {window} win
   *     The window object for which we want to retrieve the id.
   * @return {String} The unique id for this chrome window.
   */
  getIdForWindow(win) {
    if (!this._chromeWindowHandles.has(win)) {
      const uuid = uuidGen.generateUUID().toString();
      this._chromeWindowHandles.set(win, uuid.substring(1, uuid.length - 1));
    }
    return this._chromeWindowHandles.get(win);
  }

  /**
   * Close the specified window.
   *
   * @param {window} win
   *     The window to close.
   * @return {Promise}
   *     A promise which is resolved when the current window has been closed.
   */
  async closeWindow(win) {
    const destroyed = waitForObserverTopic("xul-window-destroyed", {
      checkFn: () => win && win.closed,
    });

    win.close();

    return destroyed;
  }

  /**
   * Focus the specified window.
   *
   * @param {window} win
   *     The window to focus.
   * @return {Promise}
   *     A promise which is resolved when the window has been focused.
   */
  async focusWindow(win) {
    if (Services.focus.activeWindow != win) {
      let activated = waitForEvent(win, "activate");
      let focused = waitForEvent(win, "focus", { capture: true });

      win.focus();

      await Promise.all([activated, focused]);
    }
  }

  /**
   * Open a new browser window.
   *
   * @param {window} openerWindow
   *     The window from which the new window should be opened.
   * @param {Boolean} [focus=false]
   *     If true, the opened window will receive the focus.
   * @param {Boolean} [isPrivate=false]
   *     If true, the opened window will be a private window.
   * @return {Promise}
   *     A promise resolving to the newly created chrome window.
   */
  async openBrowserWindow(openerWindow, focus = false, isPrivate = false) {
    switch (AppInfo.name) {
      case "Firefox":
        // Open new browser window, and wait until it is fully loaded.
        // Also wait for the window to be focused and activated to prevent a
        // race condition when promptly focusing to the original window again.
        const win = openerWindow.OpenBrowserWindow({ private: isPrivate });

        const activated = waitForEvent(win, "activate");
        const focused = waitForEvent(win, "focus", { capture: true });
        const startup = waitForObserverTopic(
          "browser-delayed-startup-finished",
          {
            checkFn: subject => subject == win,
          }
        );

        win.focus();
        await Promise.all([activated, focused, startup]);

        // The new window shouldn't get focused. As such set the
        // focus back to the opening window.
        if (!focus) {
          await this.focusWindow(openerWindow);
        }

        return win;

      default:
        throw new error.UnsupportedOperationError(
          `openWindow() not supported in ${AppInfo.name}`
        );
    }
  }
}

// Expose a shared singleton.
const windowManager = new WindowManager();
