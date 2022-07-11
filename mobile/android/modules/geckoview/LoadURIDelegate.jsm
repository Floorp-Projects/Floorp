// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["LoadURIDelegate"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("LoadURIDelegate");

const LoadURIDelegate = {
  // Delegate URI loading to the app.
  // Return whether the loading has been handled.
  load(aWindow, aEventDispatcher, aUri, aWhere, aFlags, aTriggeringPrincipal) {
    if (!aWindow) {
      return false;
    }

    const triggerUri =
      aTriggeringPrincipal &&
      (aTriggeringPrincipal.isNullPrincipal ? null : aTriggeringPrincipal.URI);

    const message = {
      type: "GeckoView:OnLoadRequest",
      uri: aUri ? aUri.displaySpec : "",
      where: aWhere,
      flags: aFlags,
      triggerUri: triggerUri && triggerUri.displaySpec,
      hasUserGesture: aWindow.document.hasValidTransientUserGestureActivation,
    };

    let handled = undefined;
    aEventDispatcher.sendRequestForResult(message).then(
      response => {
        handled = response;
      },
      () => {
        // There was an error or listener was not registered in GeckoSession,
        // treat as unhandled.
        handled = false;
      }
    );
    Services.tm.spinEventLoopUntil(
      "LoadURIDelegate.jsm:load",
      () => aWindow.closed || handled !== undefined
    );

    return handled || false;
  },

  handleLoadError(aWindow, aEventDispatcher, aUri, aError, aErrorModule) {
    let errorClass = 0;
    try {
      const nssErrorsService = Cc[
        "@mozilla.org/nss_errors_service;1"
      ].getService(Ci.nsINSSErrorsService);
      errorClass = nssErrorsService.getErrorClass(aError);
    } catch (e) {}

    const msg = {
      type: "GeckoView:OnLoadError",
      uri: aUri && aUri.spec,
      error: aError,
      errorModule: aErrorModule,
      errorClass,
    };

    let errorPageURI = undefined;
    aEventDispatcher.sendRequestForResult(msg).then(
      response => {
        try {
          errorPageURI = response ? Services.io.newURI(response) : null;
        } catch (e) {
          warn`Failed to parse URI '${response}`;
          errorPageURI = null;
          Components.returnCode = Cr.NS_ERROR_ABORT;
        }
      },
      e => {
        errorPageURI = null;
        Components.returnCode = Cr.NS_ERROR_ABORT;
      }
    );
    Services.tm.spinEventLoopUntil(
      "LoadURIDelegate.jsm:handleLoadError",
      () => aWindow.closed || errorPageURI !== undefined
    );

    return errorPageURI;
  },

  isSafeBrowsingError(aError) {
    return (
      aError === Cr.NS_ERROR_PHISHING_URI ||
      aError === Cr.NS_ERROR_MALWARE_URI ||
      aError === Cr.NS_ERROR_HARMFUL_URI ||
      aError === Cr.NS_ERROR_UNWANTED_URI
    );
  },
};
