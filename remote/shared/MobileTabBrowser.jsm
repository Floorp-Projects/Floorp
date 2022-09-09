/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MobileTabBrowser"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewTabUtil: "resource://gre/modules/GeckoViewTestUtils.jsm",

  windowManager: "chrome://remote/content/shared/WindowManager.jsm",
});

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

  /**
   * Create a new tab.
   *
   * @param url URL to load within the newly opended tab.
   *
   * @return {Promise<Tab>} The created tab.
   * @throws {Error} Throws an error if the tab cannot be created.
   */
  addTab(url) {
    return lazy.GeckoViewTabUtil.createNewTab(url);
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
