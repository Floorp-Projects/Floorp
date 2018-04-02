/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewSelectionAction"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewSelectionAction"));

function debug(aMsg) {
  // dump(aMsg);
}

// Handles inter-op between accessible carets and GeckoSession.
class GeckoViewSelectionAction extends GeckoViewModule {
  init() {
  }

  register() {
    debug("register");
    this.registerContent("chrome://geckoview/content/GeckoViewSelectionActionContent.js");
  }

  unregister() {
    debug("unregister");
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug("receiveMessage " + aMsg.name);
  }
}
