/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);
const { LoadURIDelegate } = ChromeUtils.import(
  "resource://gre/modules/LoadURIDelegate.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

var EXPORTED_SYMBOLS = ["LoadURIDelegateChild"];

// Implements nsILoadURIDelegate.
class LoadURIDelegateChild extends GeckoViewActorChild {
  /** Returns true if this docShell is of type Content, false otherwise. */
  get isContentWindow() {
    if (!this.docShell) {
      return false;
    }

    // Some WebExtension windows are tagged as content but really they are not
    // for us (e.g. the remote debugging window or the background extension
    // page).
    const browser = this.browsingContext.top.embedderElement;
    if (browser) {
      const viewType = browser.getAttribute("webextension-view-type");
      if (viewType) {
        debug`webextension-view-type: ${viewType}`;
        return false;
      }
      const debugTarget = browser.getAttribute(
        "webextension-addon-debug-target"
      );
      if (debugTarget) {
        debug`webextension-addon-debug-target: ${debugTarget}`;
        return false;
      }
    }

    return this.docShell.itemType == this.docShell.typeContent;
  }

  // nsILoadURIDelegate.
  loadURI(aUri, aWhere, aFlags, aTriggeringPrincipal) {
    debug`loadURI: uri=${aUri && aUri.spec}
                    where=${aWhere} flags=0x${aFlags.toString(16)}
                    tp=${aTriggeringPrincipal &&
                      aTriggeringPrincipal.URI &&
                      aTriggeringPrincipal.URI.spec}`;
    if (!this.isContentWindow) {
      debug`loadURI: not a content window`;
      // This is an internal Gecko window, nothing to do
      return false;
    }

    // Ignore any load going to the extension process
    // TODO: Remove workaround after Bug 1619798
    if (
      WebExtensionPolicy.useRemoteWebExtensions &&
      E10SUtils.getRemoteTypeForURIObject(
        aUri,
        /* aMultiProcess */ true,
        /* aRemoteSubframes */ false,
        Services.appinfo.remoteType
      ) == E10SUtils.EXTENSION_REMOTE_TYPE
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
    if (!this.isContentWindow) {
      // This is an internal Gecko window, nothing to do
      debug`handleLoadError: not a content window`;
      return null;
    }

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
  Ci.nsILoadURIDelegate,
]);

const { debug, warn } = LoadURIDelegateChild.initLogging("LoadURIDelegate"); // eslint-disable-line no-unused-vars
