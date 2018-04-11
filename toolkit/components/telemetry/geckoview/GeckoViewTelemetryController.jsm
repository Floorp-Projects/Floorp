/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
});

GeckoViewUtils.initLogging("GeckoView.TelemetryController", this);

var EXPORTED_SYMBOLS = ["GeckoViewTelemetryController"];

let GeckoViewTelemetryController = {
  /**
   * Setup the Telemetry recording flags. This must be called
   * in all the processes that need to collect Telemetry.
   */
  setup() {
    debug `setup`;

    TelemetryUtils.setTelemetryRecordingFlags();

    debug `setup - canRecordPrereleaseData ${Services.telemetry.canRecordPrereleaseData
          }, canRecordReleaseData ${Services.telemetry.canRecordReleaseData}`;
  },
};
