/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TELEMETRY_CATEGORY = "normandy";

export const TelemetryEvents = {
  init() {
    Services.telemetry.setEventRecordingEnabled(TELEMETRY_CATEGORY, true);
  },

  sendEvent(method, object, value, extra) {
    for (const val of Object.values(extra)) {
      if (val == null) {
        throw new Error(
          "Extra parameters in telemetry events must not be null"
        );
      }
    }
    Services.telemetry.recordEvent(
      TELEMETRY_CATEGORY,
      method,
      object,
      value,
      extra
    );
  },
};
