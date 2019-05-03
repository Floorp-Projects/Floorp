/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {GeckoViewChildModule} = ChromeUtils.import("resource://gre/modules/GeckoViewChildModule.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ErrorPageEventHandler: "chrome://geckoview/content/ErrorPageEventHandler.js",
  LoadURIDelegate: "resource://gre/modules/LoadURIDelegate.jsm",
});

// Implements nsILoadURIDelegate.
class GeckoViewNavigationChild extends GeckoViewChildModule {
  onInit() {
    docShell.loadURIDelegate = this;

    if (Services.androidBridge.isFennec) {
      addEventListener("DOMContentLoaded", this);
    }

    if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) {
      let tabchild = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIBrowserChild);
      tabchild.webBrowserChrome = this;
    }
  }

  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug `loadURI: uri=${aUri && aUri.spec}
                    where=${aWhere} flags=${aFlags}
                    tp=${aTriggeringPrincipal && aTriggeringPrincipal.URI &&
                         aTriggeringPrincipal.URI.spec}`;

    if (!this.enabled) {
      return false;
    }

    return LoadURIDelegate.load(content, this.eventDispatcher,
                                aUri, aWhere, aFlags, aTriggeringPrincipal);
  }

  // nsILoadURIDelegate.
  handleLoadError(aUri, aError, aErrorModule) {
    debug `handleLoadError: uri=${aUri && aUri.spec}
                             uri2=${aUri && aUri.displaySpec}
                             error=${aError}`;

    if (aUri && LoadURIDelegate.isSafeBrowsingError(aError)) {
      const message = {
        type: "GeckoView:ContentBlocked",
        uri: aUri.spec,
        error: aError,
      };

      this.eventDispatcher.sendRequest(message);
    }

    if (!this.enabled) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return LoadURIDelegate.handleLoadError(content, this.eventDispatcher,
                                           aUri, aError, aErrorModule);
  }

  // nsIWebBrowserChrome
  onBeforeLinkTraversal(aOriginalTarget, aLinkURI, aLinkNode, aIsAppTab) {
    debug `onBeforeLinkTraversal ${aLinkURI.displaySpec}`;
    return BrowserUtils.onBeforeLinkTraversal(aOriginalTarget, aLinkURI, aLinkNode, aIsAppTab);
  }

  // nsIWebBrowserChrome
  shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData, aTriggeringPrincipal, aCsp) {
    debug `shouldLoadURI ${aURI.displaySpec}`;

    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, false, null, aCsp);
      return false;
    }

    return true;
  }

  // nsIWebBrowserChrome
  shouldLoadURIInThisProcess(aURI) {
    debug `shouldLoadURIInThisProcess ${aURI.displaySpec}`;
    let remoteSubframes = docShell.QueryInterface(Ci.nsILoadContext).useRemoteSubframes;
    return E10SUtils.shouldLoadURIInThisProcess(aURI, remoteSubframes);
  }

  // nsIWebBrowserChrome
  reloadInFreshProcess(aDocShell, aURI, aReferrer, aTriggeringPrincipal, aLoadFlags, aCsp) {
    debug `reloadInFreshProcess ${aURI.displaySpec}`;
    E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, true, aLoadFlags, aCsp);
    return true;
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        // TODO: Remove this when we have a better story re: interactive error pages.
        let target = aEvent.originalTarget;

        // ignore on frames and other documents
        if (target != content.document)
          return;

        let docURI = target.documentURI;

        if (docURI.startsWith("about:certerror") || docURI.startsWith("about:blocked")) {
          addEventListener("click", ErrorPageEventHandler, true);
          let listener = () => {
            removeEventListener("click", ErrorPageEventHandler, true);
            removeEventListener("pagehide", listener, true);
          };

          addEventListener("pagehide", listener, true);
        }

        break;
      }
    }
  }
}

const {debug, warn} = GeckoViewNavigationChild.initLogging("GeckoViewNavigation"); // eslint-disable-line no-unused-vars
const module = GeckoViewNavigationChild.create(this);
