/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabManager"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  MobileTabBrowser: "chrome://remote/content/shared/MobileTabBrowser.jsm",
});

// Maps browser's permanentKey to uuid: WeakMap.<Object, string>
const browserUniqueIds = new WeakMap();

var TabManager = {
  get gBrowser() {
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    return this.getTabBrowser(window);
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

  addTab({ userContextId }) {
    const tab = this.gBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId,
    });
    this.selectTab(tab);

    return tab;
  },

  /**
   * Retrieve a the browser element corresponding to the provided unique id,
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
   * Retrieve the unique id for the given xul browser element. The id is a
   * dynamically generated uuid associated with the permanentKey property of the
   * given browser element.
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
   * Retrieve the unique id for the browser element owning the provided browsing
   * context.
   *
   * @param {BrowsingContext} browsingContext
   *     The browsing context for which we want to retrieve the (browser) uuid.
   * @return {String} The unique id for the browser owning the browsing context.
   */
  getBrowserIdForBrowsingContext(browsingContext) {
    const contentBrowser = browsingContext.top.embedderElement;
    return this.getIdForBrowser(contentBrowser);
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

  removeTab(tab) {
    this.gBrowser.removeTab(tab);
  },

  selectTab(tab) {
    this.gBrowser.selectedTab = tab;
  },
};
