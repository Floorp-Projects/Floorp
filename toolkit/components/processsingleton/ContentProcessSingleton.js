/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");

function ContentProcessSingleton() {}
ContentProcessSingleton.prototype = {
  classID: Components.ID("{ca2a8470-45c7-11e4-916c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      Services.obs.addObserver(this, "xpcom-shutdown");
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
