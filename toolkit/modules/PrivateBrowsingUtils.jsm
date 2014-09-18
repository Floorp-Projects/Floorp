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
  // Rather than passing content windows to this function, please use
  // isBrowserPrivate since it works with e10s.
  isWindowPrivate: function pbu_isWindowPrivate(aWindow) {
    return this.privacyContextFromWindow(aWindow).usePrivateBrowsing;
  },

  isBrowserPrivate: function(aBrowser) {
    return this.isWindowPrivate(aBrowser.ownerDocument.defaultView);
  },

  privacyContextFromWindow: function pbu_privacyContextFromWindow(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsILoadContext);
  },

  get permanentPrivateBrowsing() {
    try {
      return gTemporaryAutoStartMode ||
             Services.prefs.getBoolPref(kAutoStartPref);
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

  whenHiddenPrivateWindowReady: function pbu_whenHiddenPrivateWindowReady(cb) {
    Components.utils.import("resource://gre/modules/Timer.jsm");

    let win = Services.appShell.hiddenPrivateDOMWindow;
    function isNotLoaded() {
      return ["complete", "interactive"].indexOf(win.document.readyState) == -1;
    }
    if (isNotLoaded()) {
      setTimeout(function poll() {
        if (isNotLoaded()) {
          setTimeout(poll, 100);
          return;
        }
        cb(Services.appShell.hiddenPrivateDOMWindow);
      }, 4);
    } else {
      cb(Services.appShell.hiddenPrivateDOMWindow);
    }
  }
};

