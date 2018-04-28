/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewTelemetryController: "resource://gre/modules/GeckoViewTelemetryController.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
});

function ContentProcessSingleton() {}
ContentProcessSingleton.prototype = {
  classID: Components.ID("{ca2a8470-45c7-11e4-916c-0800200c9a66}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      Services.obs.addObserver(this, "xpcom-shutdown");
      // Initialize Telemetry in the content process: use a different
      // controller depending on the platform.
      if (Services.prefs.getBoolPref("toolkit.telemetry.isGeckoViewMode", false)) {
        GeckoViewTelemetryController.setup();
        return;
      }
      // Initialize Firefox Desktop Telemetry.
      TelemetryController.observe(null, topic, null);
      break;
    }
    case "xpcom-shutdown":
      Services.obs.removeObserver(this, "xpcom-shutdown");
      break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentProcessSingleton]);
