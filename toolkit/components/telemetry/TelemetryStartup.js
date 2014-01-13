/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/TelemetryPing.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm")

/**
 * TelemetryStartup is needed to forward the "profile-after-change" notification
 * to TelemetryPing.jsm.
 */
function TelemetryStartup() {
}

TelemetryStartup.prototype.classID = Components.ID("{117b219f-92fe-4bd2-a21b-95a342a9d474}");
TelemetryStartup.prototype.QueryInterface = XPCOMUtils.generateQI([Components.interfaces.nsIObserver])
TelemetryStartup.prototype.observe = function(aSubject, aTopic, aData){
  if (aTopic == "profile-after-change") {
    TelemetryPing.observe(null, "profile-after-change", null);
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelemetryStartup]);
