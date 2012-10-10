/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PrivateBrowsingUtils"];

const Cc = Components.classes;
const Ci = Components.interfaces;

var PrivateBrowsingUtils = {
  isWindowPrivate: function pbu_isWindowPrivate(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsILoadContext)
                  .usePrivateBrowsing;
  },

  get permanentPrivateBrowsing() {
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
    return false; // permanent PB is not supported for now
#else
    try {
      return Cc["@mozilla.org/privatebrowsing;1"].
             getService(Ci.nsIPrivateBrowsingService).
             autoStarted;
    } catch (e) {
      return false; // PB not supported
    }
#endif
  }
};
