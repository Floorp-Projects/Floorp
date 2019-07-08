/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);

class Page extends Domain {
  // commands

  bringToFront() {
    const { browser } = this.session.target;
    const navigator = browser.ownerGlobal;
    const { gBrowser } = navigator;

    // Focus the window responsible for this page.
    navigator.focus();

    // Select the corresponding tab
    gBrowser.selectedTab = gBrowser.getTabForBrowser(browser);
  }
}
