/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MobileTabBrowser"];

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

  removeEventListener() {
    this.window.removeEventListener(...arguments);
  }
}
