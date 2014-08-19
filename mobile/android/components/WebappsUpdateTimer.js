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

Cu.import("resource://gre/modules/JNI.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/WebappManager.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("WebappsUpdateTimer");

function getContextClassName() {
  let jenv = null;
  try {
    jenv = JNI.GetForThread();

    let GeckoAppShell = JNI.LoadClass(jenv, "org.mozilla.gecko.GeckoAppShell", {
      static_methods: [
        { name: "getContext", sig: "()Landroid/content/Context;" }
      ],
    });

    JNI.LoadClass(jenv, "android.content.Context", {
      methods: [
        { name: "getClass", sig: "()Ljava/lang/Class;" },
      ],
    });

    JNI.LoadClass(jenv, "java.lang.Class", {
      methods: [
        { name: "getName", sig: "()Ljava/lang/String;" },
      ],
    });

    return JNI.ReadString(jenv, GeckoAppShell.getContext().getClass().getName());
  } finally {
    if (jenv) {
      JNI.UnloadClasses(jenv);
    }
  }
}

function WebappsUpdateTimer() {}

WebappsUpdateTimer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback,
                                         Ci.nsISupportsWeakReference]),
  classID: Components.ID("{8f7002cb-e959-4f0a-a2e8-563232564385}"),

  notify: function(aTimer) {
    // Ignore the timer if automatic update checks are disabled or if this
    // is a webapp process (since we only want to bug people about updates
    // from the browser process).
    if (Services.prefs.getIntPref("browser.webapps.checkForUpdates") == 0 ||
        getContextClassName().startsWith("org.mozilla.gecko.webapp")) {
      return;
    }

    // If we are offline, wait to be online to start the update check.
    if (Services.io.offline) {
      Log.i("network offline for webapp update check; waiting");
      Services.obs.addObserver(this, "network:offline-status-changed", true);
      return;
    }

    Log.i("periodic check for webapp updates");
    WebappManager.checkForUpdates();
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== "network:offline-status-changed" || aData !== "online") {
      return;
    }

    Log.i("network back online for webapp update check; commencing");
    Services.obs.removeObserver(this, "network:offline-status-changed");
    WebappManager.checkForUpdates();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsUpdateTimer]);
