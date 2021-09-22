/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutHttpsOnlyErrorParent"];

const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class AboutHttpsOnlyErrorParent extends JSWindowActorParent {
  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "goBack":
        this.goBackFromErrorPage(this.browser);
        break;
      case "openInsecure":
        this.openWebsiteInsecure(this.browser, aMessage.data.inFrame);
        break;
    }
  }

  goBackFromErrorPage(aBrowser) {
    if (!aBrowser.canGoBack) {
      // If the unsafe page is the first or the only one in history, go to the
      // start page.
      aBrowser.loadURI(this.getDefaultHomePage(aBrowser.ownerGlobal), {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    } else {
      aBrowser.goBack();
    }
  }

  openWebsiteInsecure(aBrowser, aIsIFrame) {
    // No matter if the the error-page shows up within an iFrame or not, we always
    // create an exception for the top-level page.
    const currentURI = aBrowser.currentURI;
    const isViewSource = currentURI.schemeIs("view-source");

    let innerURI = isViewSource
      ? currentURI.QueryInterface(Ci.nsINestedURI).innerURI
      : currentURI;

    if (!innerURI.schemeIs("https") && !innerURI.schemeIs("http")) {
      // This should never happen
      throw new Error(
        "Exceptions can only be created for http or https sites."
      );
    }

    // If the error page is within an iFrame, we create an exception for whatever
    // scheme the top-level site is currently on, because the user wants to
    // unbreak the iFrame and not the top-level page. When the error page shows up
    // on a top-level request, then we replace the scheme with http, because the
    // user wants to unbreak the whole page.
    let newURI = aIsIFrame
      ? innerURI
      : innerURI
          .mutate()
          .setScheme("http")
          .finalize();

    const oldOriginAttributes = aBrowser.contentPrincipal.originAttributes;
    const hasFpiAttribute = !!oldOriginAttributes.firstPartyDomain.length;

    // Create new content principal for the permission. If first-party isolation
    // is enabled, we have to replace the about-page first-party domain with the
    // one from the exempt website.
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      newURI,
      {
        ...oldOriginAttributes,
        firstPartyDomain: hasFpiAttribute
          ? Services.eTLD.getBaseDomain(newURI)
          : "",
      }
    );

    // Create exception for this website that expires with the session.
    Services.perms.addFromPrincipal(
      principal,
      "https-only-load-insecure",
      Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION,
      Ci.nsIPermissionManager.EXPIRE_SESSION
    );

    const insecureSpec = isViewSource
      ? `view-source:${newURI.spec}`
      : newURI.spec;
    aBrowser.loadURI(insecureSpec, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
    });
  }

  getDefaultHomePage(win) {
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return win.BROWSER_NEW_TAB_URL || "about:blank";
    }
    let url = HomePage.getDefault();
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|")) {
      url = url.split("|")[0];
    }
    return url;
  }
}
