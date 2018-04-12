/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ErrorPageEventHandler: "chrome://geckoview/content/ErrorPageEventHandler.js",
  LoadURIDelegate: "resource://gre/modules/LoadURIDelegate.jsm",
});

XPCOMUtils.defineLazyGetter(this, "dump", () =>
  ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                     {}).AndroidLog.d.bind(null, "ViewNavigation[C]"));

function debug(aMsg) {
  // dump(aMsg);
}

// Implements nsILoadURIDelegate.
class GeckoViewNavigationContent extends GeckoViewContentModule {
  onEnable() {
    debug("onEnable");

    docShell.loadURIDelegate = this;
  }

  onDisable() {
    debug("onDisable");

    docShell.loadURIDelegate = null;
  }

  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug("loadURI " + (aUri && aUri.spec) + " " + aWhere + " " + aFlags);

    // TODO: Remove this when we have a sensible error API.
    if (aUri && aUri.displaySpec.startsWith("about:certerror")) {
      addEventListener("click", ErrorPageEventHandler, true);
    }

    return LoadURIDelegate.load(this.eventDispatcher, aUri, aWhere, aFlags,
                                aTriggeringPrincipal);
  }
}

var navigationListener = new GeckoViewNavigationContent("GeckoViewNavigation", this);
