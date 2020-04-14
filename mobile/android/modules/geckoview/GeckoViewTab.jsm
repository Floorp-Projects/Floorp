/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTab", "GeckoViewTabBridge"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
  mobileWindowTracker: "resource://gre/modules/GeckoViewWebExtension.jsm",
});

// Based on the "Tab" prototype from mobile/android/chrome/content/browser.js
class Tab {
  constructor(id, browser) {
    this.id = id;
    this.browser = browser;
    this.active = false;
  }

  getActive() {
    return this.active;
  }
}

// Stub BrowserApp implementation for WebExtensions support.
class BrowserAppShim {
  constructor(window) {
    const tabId = GeckoViewTabBridge.windowIdToTabId(
      window.windowUtils.outerWindowID
    );
    this.selectedBrowser = window.browser;
    this.selectedTab = new Tab(tabId, this.selectedBrowser);
    this.tabs = [this.selectedTab];
  }

  getTabForBrowser(aBrowser) {
    return this.selectedTab;
  }

  static getBrowserApp(window) {
    let { BrowserApp } = window;

    if (!BrowserApp) {
      BrowserApp = window.gBrowser = window.BrowserApp = new BrowserAppShim(
        window
      );
    }

    return BrowserApp;
  }
}

// Because of bug 1410749, we can't use 0, though, and just to be safe
// we choose a value that is unlikely to overlap with Fennec's tab IDs.
const TAB_ID_BASE = 10000;

const GeckoViewTabBridge = {
  /**
   * Converts windowId to tabId as in GeckoView every browser window has exactly one tab.
   *
   * @param {windowId} number outerWindowId
   *
   * @returns {number} tabId
   */
  windowIdToTabId(windowId) {
    return TAB_ID_BASE + windowId;
  },

  /**
   * Converts tabId to windowId.
   *
   * @param {windowId} number
   *
   * @returns {number}
   *          outerWindowId of browser window to which the tab belongs.
   */
  tabIdToWindowId(tabId) {
    return tabId - TAB_ID_BASE;
  },

  /**
   * Request the GeckoView App to create a new tab (GeckoSession).
   *
   * @param {object} options
   * @param {string} options.url The url to load in the newly created tab
   * @param {nsIPrincipal} options.triggeringPrincipal
   * @param {boolean} [options.disallowInheritPrincipal]
   * @param {string} options.extensionId
   *
   * @returns {Promise<Tab>}
   *          A promise resolved to the newly created tab.
   * @throws {Error}
   *         Throws an error if the GeckoView app doesn't support tabs.create or fails to handle the request.
   */
  async createNewTab({ extensionId, createProperties } = {}) {
    const sessionId = await EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:WebExtension:NewTab",
      extensionId,
      createProperties,
    });

    if (!sessionId) {
      throw new Error("Cannot create new tab");
    }

    const window = await new Promise(resolve => {
      const handler = {
        observe(aSubject, aTopic, aData) {
          if (
            aTopic === "geckoview-window-created" &&
            aSubject.name === sessionId
          ) {
            Services.obs.removeObserver(handler, "geckoview-window-created");
            resolve(aSubject);
          }
        },
      };
      Services.obs.addObserver(handler, "geckoview-window-created");
    });

    return BrowserAppShim.getBrowserApp(window).selectedTab;
  },

  /**
   * Request the GeckoView App to close a tab (GeckoSession).
   *
   *
   * @param {object} options
   * @param {Window} options.window The window owning the tab to close
   * @param {string} options.extensionId
   *
   * @returns {Promise<Void>}
   *          A promise resolved after GeckoSession is closed.
   * @throws {Error}
   *         Throws an error if the GeckoView app doesn't allow extension to close tab.
   */
  async closeTab({ window, extensionId } = {}) {
    await window.WindowEventDispatcher.sendRequestForResult({
      type: "GeckoView:WebExtension:CloseTab",
      extensionId,
    });
  },

  async updateTab({ window, extensionId, updateProperties } = {}) {
    await window.WindowEventDispatcher.sendRequestForResult({
      type: "GeckoView:WebExtension:UpdateTab",
      extensionId,
      updateProperties,
    });
  },
};

class GeckoViewTab extends GeckoViewModule {
  onInit() {
    BrowserAppShim.getBrowserApp(this.window);

    this.registerListener(["GeckoView:WebExtension:SetTabActive"]);
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:WebExtension:SetTabActive": {
        const { active } = aData;
        mobileWindowTracker.setTabActive(this.window, active);
        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewTab.initLogging("GeckoViewTab"); // eslint-disable-line no-unused-vars
