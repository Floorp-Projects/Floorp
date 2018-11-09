/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTab"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");

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
class GeckoViewTab extends GeckoViewModule {
  onInit() {
    let tab = new Tab(0, this.browser);

    this.window.gBrowser = this.window.BrowserApp = {
      selectedBrowser: this.browser,
      tabs: [tab],
      selectedTab: tab,

      closeTab: function(aTab) {
        // not implemented
      },

      getTabForId: function(aId) {
        return this.selectedTab;
      },

      getTabForBrowser: function(aBrowser) {
        return this.selectedTab;
      },

      getTabForWindow: function(aWindow) {
        return this.selectedTab;
      },

      getTabForDocument: function(aDocument) {
        return this.selectedTab;
      },

      getBrowserForOuterWindowID: function(aID) {
        return this.browser;
      },
    };
  }
}
