/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewTab"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewTab"));

// function debug(aMsg) {
//   dump(aMsg);
// }

// Stub BrowserApp implementation for WebExtensions support.
class GeckoViewTab extends GeckoViewModule {
  onInit() {
    this.browser.tab = { id: 0, browser: this.browser };

    this.window.gBrowser = this.window.BrowserApp = {
      selectedBrowser: this.browser,
      tabs: [this.browser.tab],
      selectedTab: this.browser.tab,

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
    };
  }
}
