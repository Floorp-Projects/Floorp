/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component triggers a periodic webapp update check.
 */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/WebappManager.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function log(message) {
  // We use *dump* instead of Services.console.logStringMessage so the messages
  // have the INFO level of severity instead of the ERROR level.  And we don't
  // append a newline character to the end of the message because *dump* spills
  // into the Android native logging system, which strips newlines from messages
  // and breaks messages into lines automatically at display time (i.e. logcat).
  dump(message);
}

function WebappsUpdateTimer() {}

WebappsUpdateTimer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback,
                                         Ci.nsISupportsWeakReference]),
  classID: Components.ID("{8f7002cb-e959-4f0a-a2e8-563232564385}"),

  notify: function(aTimer) {
    if (Services.prefs.getIntPref("browser.webapps.checkForUpdates") == 0) {
      // Do nothing, because updates are disabled.
      log("Webapps update timer invoked in webapp process; ignoring.");
      return;
    }

    // If we are offline, wait to be online to start the update check.
    if (Services.io.offline) {
      log("network offline for webapp update check; waiting");
      Services.obs.addObserver(this, "network:offline-status-changed", true);
      return;
    }

    log("periodic check for webapp updates");
    WebappManager.checkForUpdates();
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== "network:offline-status-changed" || aData !== "online") {
      return;
    }

    log("network back online for webapp update check; commencing");
    Services.obs.removeObserver(this, "network:offline-status-changed");
    WebappManager.checkForUpdates();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsUpdateTimer]);
