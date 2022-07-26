/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyScriptGetter(
  this,
  "PrintUtils",
  "chrome://global/content/printUtils.js"
);

// This is an implementation of nsIBrowserDOMWindow that handles only opening
// print browsers, because the "open a new window fallback" is just too slow
// in some cases and causes timeouts.
function BrowserDOMWindow() {}
BrowserDOMWindow.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIBrowserDOMWindow"]),

  _maybeOpen(aOpenWindowInfo, aWhere) {
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_PRINT_BROWSER) {
      return PrintUtils.handleStaticCloneCreatedForPrint(aOpenWindowInfo);
    }
    return null;
  },

  createContentWindow(
    aURI,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp
  ) {
    return this._maybeOpen(aOpenWindowInfo, aWhere)?.browsingContext;
  },

  openURI(aURI, aOpenWindowInfo, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
    return this._maybeOpen(aOpenWindowInfo, aWhere)?.browsingContext;
  },

  createContentWindowInFrame(aURI, aParams, aWhere, aFlags, aName) {
    return this._maybeOpen(aParams.openWindowInfo, aWhere);
  },

  openURIInFrame(aURI, aParams, aWhere, aFlags, aName) {
    return this._maybeOpen(aParams.openWindowInfo, aWhere);
  },

  canClose() {
    return true;
  },

  get tabCount() {
    return 1;
  },
};

window.browserDOMWindow = new BrowserDOMWindow();
