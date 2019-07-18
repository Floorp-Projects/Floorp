/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabManager"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var TabManager = {
  addTab({ userContextId }) {
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    const { gBrowser } = window;
    const tab = gBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId,
    });
    gBrowser.selectedTab = tab;
    return tab;
  },
};
