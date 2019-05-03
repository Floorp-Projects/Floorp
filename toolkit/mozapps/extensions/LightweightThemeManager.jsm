/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "hiddenWindow", () => {
  const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

  let browser = Services.appShell.createWindowlessBrowser(true);
  let principal = Services.scriptSecurityManager.getSystemPrincipal();
  browser.docShell.createAboutBlankContentViewer(principal);

  Services.obs.addObserver(() => { browser.close(); }, "xpcom-will-shutdown");

  return browser.document.ownerGlobal;
});

if (AppConstants.DEBUG) {
  void hiddenWindow;
}

var _fallbackThemeData = null;

var LightweightThemeManager = {
  set fallbackThemeData(data) {
    if (data && Object.getOwnPropertyNames(data).length) {
      _fallbackThemeData = Object.assign({}, data);

      for (let key of ["theme", "darkTheme"]) {
        let data = _fallbackThemeData[key] || null;
        if (data && typeof data.headerURL === "string") {
          data.headerImage = new hiddenWindow.Image();
          data.headerImage.src = data.headerURL;
        }
      }
    } else {
      _fallbackThemeData = null;
    }
  },

  /*
   * Returns the currently active theme, taking the fallback theme into account
   * if we'd be using the default theme otherwise.
   *
   * This will always return the original theme data and not make use of
   * locally persisted resources.
   */
  get currentThemeWithFallback() {
    return _fallbackThemeData && _fallbackThemeData.theme;
  },

  get themeData() {
    return _fallbackThemeData || {theme: null};
  },
};
