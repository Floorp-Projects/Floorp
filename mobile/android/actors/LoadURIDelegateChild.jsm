/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);
const { LoadURIDelegate } = ChromeUtils.import(
  "resource://gre/modules/LoadURIDelegate.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

var EXPORTED_SYMBOLS = ["LoadURIDelegateChild"];

// Implements nsILoadURIDelegate.
class LoadURIDelegateChild extends GeckoViewActorChild {
  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug`loadURI: uri=${aUri && aUri.spec}
                    where=${aWhere} flags=0x${aFlags.toString(16)}
                    tp=${aTriggeringPrincipal && aTriggeringPrincipal.spec}`;

    // Ignore any load going to the extension process
    // TODO: Remove workaround after Bug 1619798
    if (
      WebExtensionPolicy.useRemoteWebExtensions &&
      lazy.E10SUtils.getRemoteTypeForURIObject(
        aUri,
        /* aMultiProcess */ true,
        /* aRemoteSubframes */ false,
        Services.appinfo.remoteType
      ) == lazy.E10SUtils.EXTENSION_REMOTE_TYPE
    ) {
      debug`Bypassing load delegate in the Extension process.`;
      return false;
    }

    return LoadURIDelegate.load(
      this.contentWindow,
      this.eventDispatcher,
      aUri,
      aWhere,
      aFlags,
      aTriggeringPrincipal
    );
  }

  // nsILoadURIDelegate.
  handleLoadError(aUri, aError, aErrorModule) {
    debug`handleLoadError: uri=${aUri && aUri.spec}
                             displaySpec=${aUri && aUri.displaySpec}
                             error=${aError}`;
    if (aUri && LoadURIDelegate.isSafeBrowsingError(aError)) {
      const message = {
        type: "GeckoView:ContentBlocked",
        uri: aUri.spec,
        error: aError,
      };

      this.eventDispatcher.sendRequest(message);
    }

    return LoadURIDelegate.handleLoadError(
      this.contentWindow,
      this.eventDispatcher,
      aUri,
      aError,
      aErrorModule
    );
  }
}

LoadURIDelegateChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsILoadURIDelegate",
]);

const { debug, warn } = LoadURIDelegateChild.initLogging("LoadURIDelegate");
