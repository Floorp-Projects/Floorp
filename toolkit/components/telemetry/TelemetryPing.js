/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm")
Components.utils.import("resource://gre/modules/Deprecated.jsm");

let JSM = {};
Components.utils.import("resource://gre/modules/TelemetryPing.jsm", JSM);

function TelemetryPing() {
  Deprecated.warning("nsITelemetryPing is deprecated. Please use TelemetryPing.jsm instead",
    "https://bugzilla.mozilla.org/show_bug.cgi?id=913070");
}

TelemetryPing.prototype = Object.create(JSM.TelemetryPing);
TelemetryPing.prototype.classID = Components.ID("{55d6a5fa-130e-4ee6-a158-0133af3b86ba}");
TelemetryPing.prototype.QueryInterface = XPCOMUtils.generateQI([Components.interfaces.nsITelemetryPing]);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelemetryPing]);
