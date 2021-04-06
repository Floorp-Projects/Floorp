/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewSettings: "resource://gre/modules/GeckoViewSettings.jsm",
});

var EXPORTED_SYMBOLS = ["WebBrowserChromeChild"];

// Implements nsIWebBrowserChrome
class WebBrowserChromeChild extends GeckoViewActorChild {}

const { debug, warn } = WebBrowserChromeChild.initLogging("WebBrowserChrome");
