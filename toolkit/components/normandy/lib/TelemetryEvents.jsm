/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["TelemetryEvents"];

const TELEMETRY_CATEGORY = "normandy";

const TelemetryEvents = {
  init() {
    Services.telemetry.setEventRecordingEnabled(TELEMETRY_CATEGORY, true);
  },

  sendEvent(method, object, value, extra) {
    Services.telemetry.recordEvent(
      TELEMETRY_CATEGORY,
      method,
      object,
      value,
      extra
    );
  },
};
