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
const { SessionStore } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionStore.jsm"
);

class AboutHttpsOnlyErrorParent extends JSWindowActorParent {
  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "goBack":
        this.goBackFromErrorPage(this.browser.ownerGlobal);
        break;
      case "openInsecure":
        this.openWebsiteInsecure(this.browser);
        break;
    }
  }

  goBackFromErrorPage(aWindow) {
    if (!aWindow.gBrowser) {
      return;
    }

    let state = JSON.parse(
      SessionStore.getTabState(aWindow.gBrowser.selectedTab)
    );
    if (state.index == 1) {
      // If the unsafe page is the first or the only one in history, go to the
      // start page.
      aWindow.gBrowser.loadURI(this.getDefaultHomePage(aWindow), {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    } else {
      aWindow.gBrowser.goBack();
    }
  }

  openWebsiteInsecure(aBrowser) {
    // HTTPS-Only Mode does an internal redirect from HTTP to HTTPS and
    // displays the error page if the request fails. So if we want to load the
    // page with the exception, we need to replace http:// with https://
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

    let newURI = innerURI
      .mutate()
      .setScheme("http")
      .finalize();

    let principal = Services.scriptSecurityManager.createContentPrincipal(
      newURI,
      aBrowser.contentPrincipal.originAttributes
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
    });
  }

  getDefaultHomePage(win) {
    let url = win.BROWSER_NEW_TAB_URL;
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return url;
    }
    url = HomePage.getDefault();
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|")) {
      url = url.split("|")[0];
    }
    return url;
  }
}
