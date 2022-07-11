/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PrivateBrowsingUtils"];

const kAutoStartPref = "browser.privatebrowsing.autostart";

// This will be set to true when the PB mode is autostarted from the command
// line for the current session.
var gTemporaryAutoStartMode = false;

var PrivateBrowsingUtils = {
  get enabled() {
    return Services.policies.isAllowed("privatebrowsing");
  },

  // Rather than passing content windows to this function, please use
  // isBrowserPrivate since it works with e10s.
  isWindowPrivate: function pbu_isWindowPrivate(aWindow) {
    if (!aWindow.isChromeWindow) {
      dump(
        "WARNING: content window passed to PrivateBrowsingUtils.isWindowPrivate. " +
          "Use isContentWindowPrivate instead (but only for frame scripts).\n" +
          new Error().stack
      );
    }

    return this.privacyContextFromWindow(aWindow).usePrivateBrowsing;
  },

  // This should be used only in frame scripts.
  isContentWindowPrivate: function pbu_isWindowPrivate(aWindow) {
    return this.privacyContextFromWindow(aWindow).usePrivateBrowsing;
  },

  isBrowserPrivate(aBrowser) {
    let chromeWin = aBrowser.ownerGlobal;
    if (chromeWin.gMultiProcessBrowser || !aBrowser.contentWindow) {
      // In e10s we have to look at the chrome window's private
      // browsing status since the only alternative is to check the
      // content window, which is in another process.  If the browser
      // is lazy or is running in windowless configuration then the
      // content window doesn't exist.
      return this.isWindowPrivate(chromeWin);
    }
    return this.privacyContextFromWindow(aBrowser.contentWindow)
      .usePrivateBrowsing;
  },

  privacyContextFromWindow: function pbu_privacyContextFromWindow(aWindow) {
    return aWindow.docShell.QueryInterface(Ci.nsILoadContext);
  },

  get permanentPrivateBrowsing() {
    try {
      return (
        gTemporaryAutoStartMode || Services.prefs.getBoolPref(kAutoStartPref)
      );
    } catch (e) {
      // The pref does not exist
      return false;
    }
  },

  // These should only be used from internal code
  enterTemporaryAutoStartMode: function pbu_enterTemporaryAutoStartMode() {
    gTemporaryAutoStartMode = true;
  },
  get isInTemporaryAutoStartMode() {
    return gTemporaryAutoStartMode;
  },
};
