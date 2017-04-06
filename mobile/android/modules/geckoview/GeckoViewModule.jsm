/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewModule"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewModule");

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewModule {
  constructor(aWindow, aBrowser, aEventDispatcher) {
    this.window = aWindow;
    this.browser = aBrowser;
    this.eventDispatcher = aEventDispatcher;

    this.eventDispatcher.registerListener(
      () => this.onSettingsUpdate(),
      "GeckoView:UpdateSettings");

    this.init();
    this.onSettingsUpdate();
  }

  // Override this with module initialization.
  init() {}

  // Called when settings have changed. Access settings via this.settings.
  onSettingsUpdate() {}

  get settings() {
    let view = this.window.arguments[0].QueryInterface(Ci.nsIAndroidView);
    return Object.freeze(view.settings);
  }

  get messageManager() {
    return this.browser.messageManager;
  }
}
