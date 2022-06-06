/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabManager"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppInfo: "chrome://remote/content/marionette/appinfo.js",
  EventPromise: "chrome://remote/content/shared/Sync.jsm",
  MobileTabBrowser: "chrome://remote/content/shared/MobileTabBrowser.jsm",
});

// Maps browser's permanentKey to uuid: WeakMap.<Object, string>
const browserUniqueIds = new WeakMap();

var TabManager = {
  /**
   * Retrieve all the browser elements from tabs as contained in open windows.
   *
   * @return {Array<xul:browser>}
   *     All the found <xul:browser>s. Will return an empty array if
   *     no windows and tabs can be found.
   */
  get browsers() {
    const browsers = [];

    for (const win of this.windows) {
      const tabBrowser = this.getTabBrowser(win);

      if (tabBrowser && tabBrowser.tabs) {
        const contentBrowsers = tabBrowser.tabs.map(tab => {
          return this.getBrowserForTab(tab);
        });
        browsers.push(...contentBrowsers);
      }
    }

    return browsers;
  },

  get windows() {
    return Services.wm.getEnumerator(null);
  },

  /**
   * Array of unique browser ids (UUIDs) for all content browsers of all
   * windows.
   *
   * TODO: Similarly to getBrowserById, we should improve the performance of
   * this getter in Bug 1750065.
   *
   * @return {Array<String>}
   *     Array of UUIDs for all content browsers.
   */
  get allBrowserUniqueIds() {
    const browserIds = [];

    for (const win of this.windows) {
      const tabBrowser = this.getTabBrowser(win);

      // Only return handles for browser windows
      if (tabBrowser && tabBrowser.tabs) {
        for (const tab of tabBrowser.tabs) {
          const contentBrowser = this.getBrowserForTab(tab);
          const winId = this.getIdForBrowser(contentBrowser);
          if (winId !== null) {
            browserIds.push(winId);
          }
        }
      }
    }

    return browserIds;
  },

  /**
   * Get the <code>&lt;xul:browser&gt;</code> for the specified tab.
   *
   * @param {Tab} tab
   *     The tab whose browser needs to be returned.
   *
   * @return {xul:browser}
   *     The linked browser for the tab or null if no browser can be found.
   */
  getBrowserForTab(tab) {
    if (tab && "linkedBrowser" in tab) {
      return tab.linkedBrowser;
    }

    return null;
  },

  /**
   * Return the tab browser for the specified chrome window.
   *
   * @param {ChromeWindow} win
   *     Window whose <code>tabbrowser</code> needs to be accessed.
   *
   * @return {Tab}
   *     Tab browser or null if it's not a browser window.
   */
  getTabBrowser(win) {
    // GeckoView
    // TODO: Migrate to AppInfo.isAndroid after AppInfo moves to shared/
    if (Services.appinfo.OS === "Android") {
      return new MobileTabBrowser(win);
      // Firefox
    } else if ("gBrowser" in win) {
      return win.gBrowser;
      // Thunderbird
    } else if (win.document.getElementById("tabmail")) {
      return win.document.getElementById("tabmail");
    }

    return null;
  },

  /**
   * Create a new tab.
   *
   * @param {Object} options
   * @param {Boolean=} options.focus
   *     Set to true if the new tab should be focused (selected). Defaults to
   *     false.
   * @param {Number} options.userContextId
   *     The user context (container) id.
   * @param {window=} options.window
   *     The window where the new tab will open. Defaults to Services.wm.getMostRecentWindow
   *     if no window is provided.
   */
  async addTab(options = {}) {
    const {
      focus = false,
      userContextId,
      window = Services.wm.getMostRecentWindow(null),
    } = options;
    const tabBrowser = this.getTabBrowser(window);

    const tab = tabBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId,
    });

    if (focus) {
      await this.selectTab(tab);
    }

    return tab;
  },

  /**
   * Retrieve the browser element corresponding to the provided unique id,
   * previously generated via getIdForBrowser.
   *
   * TODO: To avoid creating strong references on browser elements and
   * potentially leaking those elements, this method loops over all windows and
   * all tabs. It should be replaced by a faster implementation in Bug 1750065.
   *
   * @param {String} id
   *     A browser unique id created by getIdForBrowser.
   * @return {xul:browser}
   *     The <xul:browser> corresponding to the provided id. Will return null if
   *     no matching browser element is found.
   */
  getBrowserById(id) {
    for (const win of this.windows) {
      const tabBrowser = this.getTabBrowser(win);
      if (tabBrowser && tabBrowser.tabs) {
        for (let i = 0; i < tabBrowser.tabs.length; ++i) {
          const contentBrowser = this.getBrowserForTab(tabBrowser.tabs[i]);
          if (this.getIdForBrowser(contentBrowser) == id) {
            return contentBrowser;
          }
        }
      }
    }
    return null;
  },

  /**
   * Retrieve the browsing context corresponding to the provided unique id.
   *
   * @param {String} id
   *     A browsing context unique id (created by getIdForBrowsingContext).
   * @return {BrowsingContext=}
   *     The browsing context found for this id, null if none was found.
   */
  getBrowsingContextById(id) {
    const browser = this.getBrowserById(id);
    if (browser) {
      return browser.browsingContext;
    }

    return BrowsingContext.get(id);
  },

  /**
   * Retrieve the unique id for the given xul browser element. The id is a
   * dynamically generated uuid associated with the permanentKey property of the
   * given browser element. This method is preferable over getIdForBrowsingContext
   * in case of working with browser element of a tab, since we can not guarantee
   * that browsing context is attached to it.
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
    if (!browserUniqueIds.has(key)) {
      const uuid = Services.uuid.generateUUID().toString();
      browserUniqueIds.set(key, uuid.substring(1, uuid.length - 1));
    }
    return browserUniqueIds.get(key);
  },

  /**
   * Retrieve the id of a Browsing Context.
   *
   * For a top-level browsing context a custom unique id will be returned.
   *
   * @param {BrowsingContext=} browsingContext
   *     The browsing context to get the id from.
   *
   * @returns {String}
   *     The id of the browsing context.
   */
  getIdForBrowsingContext(browsingContext) {
    if (!browsingContext) {
      return null;
    }

    if (!browsingContext.parent) {
      // Top-level browsing contexts have their own custom unique id.
      return this.getIdForBrowser(browsingContext.embedderElement);
    }

    return browsingContext.id.toString();
  },

  getTabCount() {
    let count = 0;
    for (const win of this.windows) {
      // For browser windows count the tabs. Otherwise take the window itself.
      const tabbrowser = this.getTabBrowser(win);
      if (tabbrowser?.tabs) {
        count += tabbrowser.tabs.length;
      } else {
        count += 1;
      }
    }
    return count;
  },

  /**
   * Remove the given tab.
   *
   * @param {Tab} tab
   *     Tab to remove.
   */
  removeTab(tab) {
    const ownerWindow = this._getWindowForTab(tab);
    const tabBrowser = this.getTabBrowser(ownerWindow);
    tabBrowser.removeTab(tab);
  },

  /**
   * Select the given tab.
   *
   * @param {Tab} tab
   *     Tab to select.
   *
   * @returns {Promise}
   *     Promise that resolves when the given tab has been selected.
   */
  selectTab(tab) {
    const ownerWindow = this._getWindowForTab(tab);
    const tabBrowser = this.getTabBrowser(ownerWindow);

    if (tab === tabBrowser.selectedTab) {
      return Promise.resolve();
    }

    const selected = new EventPromise(ownerWindow, "TabSelect");
    tabBrowser.selectedTab = tab;
    return selected;
  },

  supportsTabs() {
    // TODO: Only Firefox supports adding tabs at the moment.
    // Geckoview support should be added via Bug 1506782.
    return AppInfo.name === "Firefox";
  },

  _getWindowForTab(tab) {
    // `.linkedBrowser.ownerGlobal` works both with Firefox Desktop and Mobile.
    // Other accessors (eg `.ownerGlobal` or `.browser.ownerGlobal`) fail on one
    // of the platforms.
    return tab.linkedBrowser.ownerGlobal;
  },
};
