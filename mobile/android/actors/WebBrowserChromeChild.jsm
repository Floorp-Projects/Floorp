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
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  GeckoViewSettings: "resource://gre/modules/GeckoViewSettings.jsm",
});

var EXPORTED_SYMBOLS = ["WebBrowserChromeChild"];

// Implements nsIWebBrowserChrome
class WebBrowserChromeChild extends GeckoViewActorChild {
  // nsIWebBrowserChrome
  onBeforeLinkTraversal(aOriginalTarget, aLinkURI, aLinkNode, aIsAppTab) {
    debug`onBeforeLinkTraversal ${aLinkURI.displaySpec}`;
    return BrowserUtils.onBeforeLinkTraversal(
      aOriginalTarget,
      aLinkURI,
      aLinkNode,
      aIsAppTab
    );
  }

  // nsIWebBrowserChrome
  shouldLoadURI(
    aDocShell,
    aURI,
    aReferrerInfo,
    aHasPostData,
    aTriggeringPrincipal,
    aCsp
  ) {
    return true;
  }

  // nsIWebBrowserChrome
  shouldLoadURIInThisProcess(aURI) {
    return true;
  }
}

WebBrowserChromeChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIWebBrowserChrome3",
]);

const { debug, warn } = WebBrowserChromeChild.initLogging("WebBrowserChrome");
