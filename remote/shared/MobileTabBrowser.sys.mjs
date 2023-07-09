/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  GeckoViewTabUtil: "resource://gre/modules/GeckoViewTestUtils.sys.mjs",

  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
});

// GeckoView shim for Desktop's gBrowser
export class MobileTabBrowser {
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

  /**
   * Create a new tab.
   *
   * @param {string} uriString
   *     The URI string to load within the newly opened tab.
   *
   * @returns {Promise<Tab>}
   *     The created tab.
   * @throws {Error}
   *     Throws an error if the tab cannot be created.
   */
  addTab(uriString) {
    return lazy.GeckoViewTabUtil.createNewTab(uriString);
  }

  getTabForBrowser(browser) {
    if (browser != this.selectedBrowser) {
      throw new Error("GeckoView only supports a single tab");
    }

    return this.selectedTab;
  }

  removeEventListener() {
    this.window.removeEventListener(...arguments);
  }

  removeTab(tab) {
    if (tab != this.selectedTab) {
      throw new Error("GeckoView only supports a single tab");
    }

    return lazy.windowManager.closeWindow(this.window);
  }
}
