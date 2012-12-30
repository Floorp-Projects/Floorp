/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PrivateBrowsingUtils"];

Components.utils.import("resource://gre/modules/Services.jsm");

const kAutoStartPref = "browser.privatebrowsing.autostart";

// This will be set to true when the PB mode is autostarted from the command
// line for the current session.
let gTemporaryAutoStartMode = false;

const Cc = Components.classes;
const Ci = Components.interfaces;

this.PrivateBrowsingUtils = {
  isWindowPrivate: function pbu_isWindowPrivate(aWindow) {
    return this.privacyContextFromWindow(aWindow).usePrivateBrowsing;
  },

  privacyContextFromWindow: function pbu_privacyContextFromWindow(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsILoadContext);
  },

  get permanentPrivateBrowsing() {
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
    return gTemporaryAutoStartMode ||
           Services.prefs.getBoolPref(kAutoStartPref, false);
#else
    try {
      return Cc["@mozilla.org/privatebrowsing;1"].
             getService(Ci.nsIPrivateBrowsingService).
             autoStarted;
    } catch (e) {
      return false; // PB not supported
    }
#endif
  },

  // These should only be used from internal code
  enterTemporaryAutoStartMode: function pbu_enterTemporaryAutoStartMode() {
    gTemporaryAutoStartMode = true;
  },
  get isInTemporaryAutoStartMode() {
    return gTemporaryAutoStartMode;
  }
};

