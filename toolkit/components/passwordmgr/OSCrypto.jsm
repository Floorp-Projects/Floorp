/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Common front for various implementations of OSCrypto
 */

"use strict";

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["OSCrypto"];

var OSCrypto = {};

if (AppConstants.platform == "win") {
  Services.scriptloader.loadSubScript("resource://gre/modules/OSCrypto_win.js", this);
} else {
  throw new Error("OSCrypto.jsm isn't supported on this platform");
}
