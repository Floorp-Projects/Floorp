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
});

var TabManager = {
  get gBrowser() {
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    return window.gBrowser;
  },

  addTab({ userContextId }) {
    const tab = this.gBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId,
    });
    this.selectTab(tab);

    return tab;
  },

  removeTab(tab) {
    this.gBrowser.removeTab(tab);
  },

  selectTab(tab) {
    this.gBrowser.selectedTab = tab;
  },
};
