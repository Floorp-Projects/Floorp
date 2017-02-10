/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewModule"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

class GeckoViewModule {
  constructor(window, browser, eventDispatcher) {
    this.window = window;
    this.browser = browser;
    this.eventDispatcher = eventDispatcher;

    this.init();
  }

  init() {}

  get messageManager() {
    return this.browser.messageManager;
  }
}
