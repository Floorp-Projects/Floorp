/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PrivateBrowsingUtils"];

Components.utils.import("resource://gre/modules/Services.jsm");

const kAutoStartPref = "browser.components.autostart";

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
    return Services.prefs.getBoolPref(kAutoStart, false);
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

#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
function autoStartObserver(aSubject, aTopic, aData) {
  var newValue = Services.prefs.getBoolPref(kAutoStart);
  var windowsEnum = Services.wm.getEnumerator(null);
  while (windowsEnum.hasMoreElements()) {
    var window = windowsEnum.getNext();
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .QueryInterface(Ci.nsILoadContext)
          .usePrivateBrowsing = newValue;
  }
}

Services.prefs.addObserver(kAutoStartPref, autoStartObserver, false);
#endif

