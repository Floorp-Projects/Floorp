/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.sys.mjs",
  EventPromise: "chrome://remote/content/shared/Sync.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  MobileTabBrowser: "chrome://remote/content/shared/MobileTabBrowser.sys.mjs",
});

class TabManagerClass {
  #browserUniqueIds;
  #contextListener;
  #navigableIds;

  constructor() {
    // Maps browser's permanentKey to uuid: WeakMap.<Object, string>
    this.#browserUniqueIds = new WeakMap();

    // Maps browsing contexts to uuid: WeakMap.<BrowsingContext, string>.
    // It's required as a fallback, since in the case when a context was discarded
    // embedderElement is gone, and we cannot retrieve
    // the context id from this.#browserUniqueIds.
    this.#navigableIds = new WeakMap();

    this.#contextListener = new lazy.BrowsingContextListener();
    this.#contextListener.on("attached", this.#onContextAttached);
    this.#contextListener.startListening();

    this.browsers.forEach(browser => {
      if (this.isValidCanonicalBrowsingContext(browser.browsingContext)) {
        this.#navigableIds.set(
          browser.browsingContext,
          this.getIdForBrowsingContext(browser.browsingContext)
        );
      }
    });
  }

  /**
   * Retrieve all the browser elements from tabs as contained in open windows.
   *
   * @returns {Array<XULBrowser>}
   *     All the found <xul:browser>s. Will return an empty array if
   *     no windows and tabs can be found.
   */
  get browsers() {
    const browsers = [];

    for (const win of this.windows) {
      for (const tab of this.getTabsForWindow(win)) {
        const contentBrowser = this.getBrowserForTab(tab);
        if (contentBrowser !== null) {
          browsers.push(contentBrowser);
        }
      }
    }

    return browsers;
  }

  get windows() {
    return Services.wm.getEnumerator(null);
  }

  /**
   * Array of unique browser ids (UUIDs) for all content browsers of all
   * windows.
   *
   * TODO: Similarly to getBrowserById, we should improve the performance of
   * this getter in Bug 1750065.
   *
   * @returns {Array<string>}
   *     Array of UUIDs for all content browsers.
   */
  get allBrowserUniqueIds() {
    const browserIds = [];

    for (const win of this.windows) {
      // Only return handles for browser windows
      for (const tab of this.getTabsForWindow(win)) {
        const contentBrowser = this.getBrowserForTab(tab);
        const winId = this.getIdForBrowser(contentBrowser);
        if (winId !== null) {
          browserIds.push(winId);
        }
      }
    }

    return browserIds;
  }

  /**
   * Get the <code>&lt;xul:browser&gt;</code> for the specified tab.
   *
   * @param {Tab} tab
   *     The tab whose browser needs to be returned.
   *
   * @returns {XULBrowser}
   *     The linked browser for the tab or null if no browser can be found.
   */
  getBrowserForTab(tab) {
    if (tab && "linkedBrowser" in tab) {
      return tab.linkedBrowser;
    }

    return null;
  }

  /**
   * Return the tab browser for the specified chrome window.
   *
   * @param {ChromeWindow} win
   *     Window whose <code>tabbrowser</code> needs to be accessed.
   *
   * @returns {Tab}
   *     Tab browser or null if it's not a browser window.
   */
  getTabBrowser(win) {
    if (lazy.AppInfo.isAndroid) {
      return new lazy.MobileTabBrowser(win);
    } else if (lazy.AppInfo.isFirefox) {
      return win.gBrowser;
    }

    return null;
  }

  /**
   * Create a new tab.
   *
   * @param {object} options
   * @param {boolean=} options.focus
   *     Set to true if the new tab should be focused (selected). Defaults to
   *     false. `false` value is not properly supported on Android, additional
   *     focus of previously selected tab is required after initial navigation.
   * @param {Tab=} options.referenceTab
   *     The reference tab after which the new tab will be added. If no
   *     reference tab is provided, the new tab will be added after all the
   *     other tabs.
   * @param {number} options.userContextId
   *     The user context (container) id.
   * @param {window=} options.window
   *     The window where the new tab will open. Defaults to Services.wm.getMostRecentWindow
   *     if no window is provided. Will be ignored if referenceTab is provided.
   */
  async addTab(options = {}) {
    let {
      focus = false,
      referenceTab = null,
      userContextId,
      window = Services.wm.getMostRecentWindow(null),
    } = options;

    let index;
    if (referenceTab != null) {
      // If a reference tab was specified, the window should be the window
      // owning the reference tab.
      window = this.getWindowForTab(referenceTab);
    }

    if (referenceTab != null) {
      index = this.getTabsForWindow(window).indexOf(referenceTab) + 1;
    }

    const tabBrowser = this.getTabBrowser(window);
    const tab = await tabBrowser.addTab("about:blank", {
      index,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId,
    });

    if (focus) {
      await this.selectTab(tab);
    }

    return tab;
  }

  /**
   * Retrieve the browser element corresponding to the provided unique id,
   * previously generated via getIdForBrowser.
   *
   * TODO: To avoid creating strong references on browser elements and
   * potentially leaking those elements, this method loops over all windows and
   * all tabs. It should be replaced by a faster implementation in Bug 1750065.
   *
   * @param {string} id
   *     A browser unique id created by getIdForBrowser.
   * @returns {XULBrowser}
   *     The <xul:browser> corresponding to the provided id. Will return null if
   *     no matching browser element is found.
   */
  getBrowserById(id) {
    for (const win of this.windows) {
      for (const tab of this.getTabsForWindow(win)) {
        const contentBrowser = this.getBrowserForTab(tab);
        if (this.getIdForBrowser(contentBrowser) == id) {
          return contentBrowser;
        }
      }
    }
    return null;
  }

  /**
   * Retrieve the browsing context corresponding to the provided unique id.
   *
   * @param {string} id
   *     A browsing context unique id (created by getIdForBrowsingContext).
   * @returns {BrowsingContext=}
   *     The browsing context found for this id, null if none was found.
   */
  getBrowsingContextById(id) {
    const browser = this.getBrowserById(id);
    if (browser) {
      return browser.browsingContext;
    }

    return BrowsingContext.get(id);
  }

  /**
   * Retrieve the unique id for the given xul browser element. The id is a
   * dynamically generated uuid associated with the permanentKey property of the
   * given browser element. This method is preferable over getIdForBrowsingContext
   * in case of working with browser element of a tab, since we can not guarantee
   * that browsing context is attached to it.
   *
   * @param {XULBrowser} browserElement
   *     The <xul:browser> for which we want to retrieve the id.
   * @returns {string} The unique id for this browser.
   */
  getIdForBrowser(browserElement) {
    if (browserElement === null) {
      return null;
    }

    const key = browserElement.permanentKey;
    if (key === undefined) {
      return null;
    }

    if (!this.#browserUniqueIds.has(key)) {
      this.#browserUniqueIds.set(key, lazy.generateUUID());
    }
    return this.#browserUniqueIds.get(key);
  }

  /**
   * Retrieve the id of a Browsing Context.
   *
   * For a top-level browsing context a custom unique id will be returned.
   *
   * @param {BrowsingContext=} browsingContext
   *     The browsing context to get the id from.
   *
   * @returns {string}
   *     The id of the browsing context.
   */
  getIdForBrowsingContext(browsingContext) {
    if (!browsingContext) {
      return null;
    }

    if (!browsingContext.parent) {
      // Top-level browsing contexts have their own custom unique id.
      // If a context was discarded, embedderElement is already gone,
      // so use navigable id instead.
      return browsingContext.embedderElement
        ? this.getIdForBrowser(browsingContext.embedderElement)
        : this.#navigableIds.get(browsingContext);
    }

    return browsingContext.id.toString();
  }

  /**
   * Get the navigable for the given browsing context.
   *
   * Because Gecko doesn't support the Navigable concept in content
   * scope the content browser could be used to uniquely identify
   * top-level browsing contexts.
   *
   * @param {BrowsingContext} browsingContext
   *
   * @returns {BrowsingContext|XULBrowser} The navigable
   *
   * @throws {TypeError}
   *     If `browsingContext` is not a CanonicalBrowsingContext instance.
   */
  getNavigableForBrowsingContext(browsingContext) {
    if (!this.isValidCanonicalBrowsingContext(browsingContext)) {
      throw new TypeError(
        `Expected browsingContext to be a CanonicalBrowsingContext, got ${browsingContext}`
      );
    }

    if (browsingContext.isContent && browsingContext.parent === null) {
      return browsingContext.embedderElement;
    }

    return browsingContext;
  }

  getTabCount() {
    let count = 0;
    for (const win of this.windows) {
      // For browser windows count the tabs. Otherwise take the window itself.
      const tabsLength = this.getTabsForWindow(win).length;
      count += tabsLength ? tabsLength : 1;
    }
    return count;
  }

  /**
   * Retrieve the tab owning a Browsing Context.
   *
   * @param {BrowsingContext=} browsingContext
   *     The browsing context to get the tab from.
   *
   * @returns {Tab|null}
   *     The tab owning the Browsing Context.
   */
  getTabForBrowsingContext(browsingContext) {
    const browser = browsingContext?.top.embedderElement;
    if (!browser) {
      return null;
    }

    const tabBrowser = this.getTabBrowser(browser.ownerGlobal);
    return tabBrowser.getTabForBrowser(browser);
  }

  /**
   * Retrieve the list of tabs for a given window.
   *
   * @param {ChromeWindow} win
   *     Window whose <code>tabs</code> need to be returned.
   *
   * @returns {Array<Tab>}
   *     The list of tabs. Will return an empty list if tab browser is not available
   *     or tabs are undefined.
   */
  getTabsForWindow(win) {
    const tabBrowser = this.getTabBrowser(win);
    // For web-platform reftests a faked tabbrowser is used,
    // which does not actually have tabs.
    if (tabBrowser && tabBrowser.tabs) {
      return tabBrowser.tabs;
    }
    return [];
  }

  getWindowForTab(tab) {
    // `.linkedBrowser.ownerGlobal` works both with Firefox Desktop and Mobile.
    // Other accessors (eg `.ownerGlobal` or `.browser.ownerGlobal`) fail on one
    // of the platforms.
    return tab.linkedBrowser.ownerGlobal;
  }

  /**
   * Check if the given argument is a valid canonical browsing context and was not
   * discarded.
   *
   * @param {BrowsingContext} browsingContext
   *     The browsing context to check.
   *
   * @returns {boolean}
   *     True if the browsing context is valid, false otherwise.
   */
  isValidCanonicalBrowsingContext(browsingContext) {
    return (
      CanonicalBrowsingContext.isInstance(browsingContext) &&
      !browsingContext.isDiscarded
    );
  }

  /**
   * Remove the given tab.
   *
   * @param {Tab} tab
   *     Tab to remove.
   */
  async removeTab(tab) {
    if (!tab) {
      return;
    }

    const ownerWindow = this.getWindowForTab(tab);
    const tabBrowser = this.getTabBrowser(ownerWindow);
    await tabBrowser.removeTab(tab);
  }

  /**
   * Select the given tab.
   *
   * @param {Tab} tab
   *     Tab to select.
   *
   * @returns {Promise}
   *     Promise that resolves when the given tab has been selected.
   */
  async selectTab(tab) {
    if (!tab) {
      return Promise.resolve();
    }

    const ownerWindow = this.getWindowForTab(tab);
    const tabBrowser = this.getTabBrowser(ownerWindow);

    if (tab === tabBrowser.selectedTab) {
      return Promise.resolve();
    }

    const selected = new lazy.EventPromise(ownerWindow, "TabSelect");
    tabBrowser.selectedTab = tab;

    await selected;

    // Sometimes at that point window is not focused.
    if (Services.focus.activeWindow != ownerWindow) {
      const activated = new lazy.EventPromise(ownerWindow, "activate");
      ownerWindow.focus();
      return activated;
    }

    return Promise.resolve();
  }

  supportsTabs() {
    return lazy.AppInfo.isAndroid || lazy.AppInfo.isFirefox;
  }

  #onContextAttached = (eventName, data = {}) => {
    const { browsingContext } = data;
    if (this.isValidCanonicalBrowsingContext(browsingContext)) {
      this.#navigableIds.set(
        browsingContext,
        this.getIdForBrowsingContext(browsingContext)
      );
    }
  };
}

// Expose a shared singleton.
export const TabManager = new TabManagerClass();
