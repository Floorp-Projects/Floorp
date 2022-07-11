/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTab", "GeckoViewTabBridge"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

const { ExtensionError } = ExtensionUtils;

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  mobileWindowTracker: "resource://gre/modules/GeckoViewWebExtension.jsm",
});

class Tab {
  constructor(window) {
    this.id = GeckoViewTabBridge.windowIdToTabId(window.docShell.outerWindowID);
    this.browser = window.browser;
    this.active = false;
  }

  get linkedBrowser() {
    return this.browser;
  }

  getActive() {
    return this.active;
  }

  get userContextId() {
    return this.browser.ownerGlobal.moduleManager.settings
      .unsafeSessionContextId;
  }
}

// Because of bug 1410749, we can't use 0, though, and just to be safe
// we choose a value that is unlikely to overlap with Fennec's tab IDs.
const TAB_ID_BASE = 10000;

const GeckoViewTabBridge = {
  /**
   * Converts windowId to tabId as in GeckoView every browser window has exactly one tab.
   *
   * @param {number} windowId outerWindowId
   *
   * @returns {number} tabId
   */
  windowIdToTabId(windowId) {
    return TAB_ID_BASE + windowId;
  },

  /**
   * Converts tabId to windowId.
   *
   * @param {number} tabId
   *
   * @returns {number}
   *          outerWindowId of browser window to which the tab belongs.
   */
  tabIdToWindowId(tabId) {
    return tabId - TAB_ID_BASE;
  },

  /**
   * Delegates openOptionsPage handling to the app.
   *
   * @param {number} extensionId
   *        The ID of the extension requesting the options menu.
   *
   * @returns {Promise<Void>}
   *          A promise resolved after successful handling.
   */
  async openOptionsPage(extensionId) {
    debug`openOptionsPage for extensionId ${extensionId}`;

    try {
      await lazy.EventDispatcher.instance.sendRequestForResult({
        type: "GeckoView:WebExtension:OpenOptionsPage",
        extensionId,
      });
    } catch (errorMessage) {
      // The error message coming from GeckoView is about :OpenOptionsPage not
      // being registered so we need to have one that's extension friendly
      // here.
      throw new ExtensionError("runtime.openOptionsPage is not supported");
    }
  },

  /**
   * Request the GeckoView App to create a new tab (GeckoSession).
   *
   * @param {object} options
   * @param {string} options.extensionId
   *        The ID of the extension that requested a new tab.
   * @param {object} options.createProperties
   *        The properties for the new tab, see tabs.create reference for details.
   *
   * @returns {Promise<Tab>}
   *          A promise resolved to the newly created tab.
   * @throws {Error}
   *         Throws an error if the GeckoView app doesn't support tabs.create or fails to handle the request.
   */
  async createNewTab({ extensionId, createProperties } = {}) {
    debug`createNewTab`;

    const newSessionId = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1)
      .replace(/-/g, "");

    // The window might already be open by the time we get the response, so we
    // need to start waiting before we send the message.
    const windowPromise = new Promise(resolve => {
      const handler = {
        observe(aSubject, aTopic, aData) {
          if (
            aTopic === "geckoview-window-created" &&
            aSubject.name === newSessionId
          ) {
            Services.obs.removeObserver(handler, "geckoview-window-created");
            resolve(aSubject);
          }
        },
      };
      Services.obs.addObserver(handler, "geckoview-window-created");
    });

    let didOpenSession = false;
    try {
      didOpenSession = await lazy.EventDispatcher.instance.sendRequestForResult(
        {
          type: "GeckoView:WebExtension:NewTab",
          extensionId,
          createProperties,
          newSessionId,
        }
      );
    } catch (errorMessage) {
      // The error message coming from GeckoView is about :NewTab not being
      // registered so we need to have one that's extension friendly here.
      throw new ExtensionError("tabs.create is not supported");
    }

    if (!didOpenSession) {
      throw new ExtensionError("Cannot create new tab");
    }

    const window = await windowPromise;
    if (!window.tab) {
      window.tab = new Tab(window);
    }
    return window.tab;
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
    try {
      await window.WindowEventDispatcher.sendRequestForResult({
        type: "GeckoView:WebExtension:CloseTab",
        extensionId,
      });
    } catch (errorMessage) {
      throw new ExtensionError(errorMessage);
    }
  },

  async updateTab({ window, extensionId, updateProperties } = {}) {
    try {
      await window.WindowEventDispatcher.sendRequestForResult({
        type: "GeckoView:WebExtension:UpdateTab",
        extensionId,
        updateProperties,
      });
    } catch (errorMessage) {
      throw new ExtensionError(errorMessage);
    }
  },
};

class GeckoViewTab extends GeckoViewModule {
  onInit() {
    const { window } = this;
    if (!window.tab) {
      window.tab = new Tab(window);
    }

    this.registerListener(["GeckoView:WebExtension:SetTabActive"]);
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:WebExtension:SetTabActive": {
        const { active } = aData;
        lazy.mobileWindowTracker.setTabActive(this.window, active);
        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewTab.initLogging("GeckoViewTab");
