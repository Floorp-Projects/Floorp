/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewScroll"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

class GeckoViewScroll extends GeckoViewModule {
  onEnable() {
    debug `onEnable`;
    this.registerContent("chrome://geckoview/content/GeckoViewScrollContent.js");
  }
}
