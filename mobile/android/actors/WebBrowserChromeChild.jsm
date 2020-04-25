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
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
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
    debug`shouldLoadURI ${aURI.displaySpec}`;

    if (!GeckoViewSettings.useMultiprocess) {
      // If we're in non-e10s mode there's no other process we can load this
      // page in.
      return true;
    }

    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aHasPostData)) {
      E10SUtils.redirectLoad(
        aDocShell,
        aURI,
        aReferrerInfo,
        aTriggeringPrincipal,
        false,
        null,
        aCsp
      );
      return false;
    }

    return true;
  }

  // nsIWebBrowserChrome
  shouldLoadURIInThisProcess(aURI) {
    debug`shouldLoadURIInThisProcess ${aURI.displaySpec}`;
    const remoteSubframes = this.docShell.nsILoadContext.useRemoteSubframes;
    return E10SUtils.shouldLoadURIInThisProcess(aURI, remoteSubframes);
  }

  // nsIWebBrowserChrome
  reloadInFreshProcess(
    aDocShell,
    aURI,
    aReferrerInfo,
    aTriggeringPrincipal,
    aLoadFlags,
    aCsp
  ) {
    debug`reloadInFreshProcess ${aURI.displaySpec}`;
    E10SUtils.redirectLoad(
      aDocShell,
      aURI,
      aReferrerInfo,
      aTriggeringPrincipal,
      true,
      aLoadFlags,
      aCsp
    );
    return true;
  }
}

WebBrowserChromeChild.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIWebBrowserChrome3,
]);

const { debug, warn } = WebBrowserChromeChild.initLogging("WebBrowserChrome"); // eslint-disable-line no-unused-vars
