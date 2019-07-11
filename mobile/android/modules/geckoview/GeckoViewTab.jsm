/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTab"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// Based on the "Tab" prototype from mobile/android/chrome/content/browser.js
class Tab {
  constructor(id, browser) {
    this.id = id;
    this.browser = browser;
  }

  getActive() {
    return this.browser.docShellIsActive;
  }
}

// Stub BrowserApp implementation for WebExtensions support.
class BrowserAppShim {
  constructor(window) {
    // Because of bug 1410749, we can't use 0, though, and just to be safe
    // we choose a value that is unlikely to overlap with Fennec's tab IDs.
    const tabId = 10000 + window.windowUtils.outerWindowID;
    this.selectedBrowser = window.browser;
    this.selectedTab = new Tab(tabId, this.selectedBrowser);
    this.tabs = [this.selectedTab];
  }

  closeTab(aTab) {
    // not implemented
  }

  getTabForId(aId) {
    return this.selectedTab;
  }

  getTabForBrowser(aBrowser) {
    return this.selectedTab;
  }

  getTabForWindow(aWindow) {
    return this.selectedTab;
  }

  getTabForDocument(aDocument) {
    return this.selectedTab;
  }

  getBrowserForOuterWindowID(aID) {
    return this.selectedBrowser;
  }

  getBrowserForDocument(aDocument) {
    return this.selectedBrowser;
  }

  /**
   * Request the GeckoView App to create a new tab (GeckoSession).
   *
   * @param {string} url The url to load in the newly created tab
   * @param {object} options
   * @param {nsIPrincipal} options.triggeringPrincipal
   * @param {boolean} [options.disallowInheritPrincipal]
   * @param {string} options.extensionId
   *
   * @returns {Promise<Tab>}
   *          A promise resolved to the newly created tab.
   * @throws {Error}
   *         Throws an error if the GeckoView app doesn't support tabs.create or fails to handle the request.
   */
  async createNewTab(url, options) {
    url = url || "about:blank";

    if (!options.extensionId) {
      throw new Error("options.extensionId missing");
    }

    const message = {
      type: "GeckoView:WebExtension:NewTab",
      uri: url,
      extensionId: options.extensionId,
    };

    const sessionId = await EventDispatcher.instance.sendRequestForResult(
      message
    );

    if (!sessionId) {
      throw new Error("Cannot create new tab");
    }

    let window;
    const browser = await new Promise(resolve => {
      const handler = {
        observe(aSubject, aTopic, aData) {
          if (
            aTopic === "geckoview-window-created" &&
            aSubject.name === sessionId
          ) {
            Services.obs.removeObserver(handler, "geckoview-window-created");
            window = aSubject;
            resolve(window.browser);
          }
        },
      };
      Services.obs.addObserver(handler, "geckoview-window-created");
    });

    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    if (options.disallowInheritPrincipal) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
    }

    browser.loadURI(url, {
      flags,
      triggeringPrincipal: options.triggeringPrincipal,
    });

    return BrowserAppShim.getBrowserApp(window).selectedTab;
  }

  // ext-tabs calls tabListener.initTabReady(); which rely on deck when initializing ProgressListeners.
  // Deck will be removed by https://phabricator.services.mozilla.com/D36575.
  get deck() {
    return {
      addEventListener() {},
      removeEventListener() {},
    };
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

class GeckoViewTab extends GeckoViewModule {
  onInit() {
    BrowserAppShim.getBrowserApp(this.window);
  }
}

const { debug, warn } = GeckoViewTab.initLogging("GeckoViewTab"); // eslint-disable-line no-unused-vars
