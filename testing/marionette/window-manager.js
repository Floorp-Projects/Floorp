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
  browser: "chrome://marionette/content/browser.js",
  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * Provides helpers to interact with Window objects.
 *
 * @class WindowManager
 */
class WindowManager {
  constructor() {
    // Maps permanentKey to browsing context id: WeakMap.<Object, number>
    this._browserIds = new WeakMap();
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
   * Forces an update for the given browser's id.
   */
  updateIdForBrowser(browserElement, newId) {
    this._browserIds.set(browserElement.permanentKey, newId);
  }

  /**
   * Retrieves an id for the given xul browser element. In case
   * the browser is not known, an attempt is made to retrieve the id from
   * a CPOW, and null is returned if this fails.
   *
   * @param {xul:browser} browserElement
   *     The <xul:browser> for which we want to retrieve the id.
   * @return {Number} The unique id for this browser.
   */
  getIdForBrowser(browserElement) {
    if (browserElement === null) {
      return null;
    }

    const permKey = browserElement.permanentKey;
    if (this._browserIds.has(permKey)) {
      return this._browserIds.get(permKey);
    }

    const winId = browserElement.browsingContext.id;
    if (winId) {
      this._browserIds.set(permKey, winId);
      return winId;
    }
    return null;
  }

  /**
   * Retrieves an id for the given chrome window.
   *
   * @param {window} win
   *     The window object for which we want to retrieve the id.
   * @return {Number} The unique id for this chrome window.
   */
  getIdForWindow(win) {
    return win.browsingContext.id;
  }
}

// Expose a shared singleton.
const windowManager = new WindowManager();
