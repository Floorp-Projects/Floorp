// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["LoadURIDelegate"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

const LoadURIDelegate = {
  // Delegate URI loading to the app.
  // Return whether the loading has been handled.
  load: function(aWindow, aEventDispatcher, aUri, aWhere, aFlags) {
    if (!aWindow) {
      return Promise.resolve(false);
    }

    const message = {
      type: "GeckoView:OnLoadRequest",
      uri: aUri ? aUri.displaySpec : "",
      where: aWhere,
      flags: aFlags
    };

    return aEventDispatcher.sendRequestForResult(message).then((response) => response || false).catch(() => false);
  },

  handleLoadError: function(aWindow, aEventDispatcher, aUri, aError,
                            aErrorModule) {
    let errorClass = 0;
    try {
      let nssErrorsService = Cc["@mozilla.org/nss_errors_service;1"]
                             .getService(Ci.nsINSSErrorsService);
      errorClass = nssErrorsService.getErrorClass(aError);
    } catch (e) {}

    const msg = {
      type: "GeckoView:OnLoadError",
      uri: aUri.spec,
      error: aError,
      errorModule: aErrorModule,
      errorClass
    };

    let handled = undefined;
    aEventDispatcher.sendRequestForResult(msg).then(response => {
      handled = response;
    }, () => {
      // There was an error or listener was not registered in GeckoSession,
      // treat as unhandled.
      handled = false;
    });
    Services.tm.spinEventLoopUntil(() =>
        aWindow.closed || handled !== undefined);

    return handled || false;
  }
};
