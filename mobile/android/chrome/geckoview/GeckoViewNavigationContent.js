/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewNavigationContent"));

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewNavigationContent extends GeckoViewContentModule {
  register() {
    debug("register");

    docShell.loadURIDelegate = this;
  }

  unregister() {
    debug("unregister");

    docShell.loadURIDelegate = null;
  }

  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug("loadURI " + (aUri && aUri.spec) + " " + aWhere + " " + aFlags);

    let message = {
      type: "GeckoView:OnLoadUri",
      uri: aUri ? aUri.displaySpec : "",
      where: aWhere,
      flags: aFlags
    };

    debug("dispatch " + JSON.stringify(message));

    let handled = undefined;
    this.eventDispatcher.sendRequestForResult(message).then(response => {
      handled = response;
    }, () => {
      // There was an error or listener was not registered in GeckoSession, treat as unhandled.
      handled = false;
    });
    Services.tm.spinEventLoopUntil(() => handled !== undefined);

    if (!handled) {
      throw Cr.NS_ERROR_ABORT;
    }
  }
}

var navigationListener = new GeckoViewNavigationContent("GeckoViewNavigation", this);
