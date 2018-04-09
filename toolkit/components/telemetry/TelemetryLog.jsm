/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["TelemetryLog"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

const LOG_ENTRY_MAX_COUNT = 1000;

var gLogEntries = [];

var TelemetryLog = Object.freeze({
  log(id, data) {
    if (gLogEntries.length >= LOG_ENTRY_MAX_COUNT) {
      return;
    }
    id = String(id);
    var ts;
    try {
      ts = Math.floor(Services.telemetry.msSinceProcessStart());
    } catch (e) {
      // If timestamp is screwed up, we just give up instead of making up
      // data.
      return;
    }

    var entry = [id, ts];
    if (data !== undefined) {
      entry = entry.concat(Array.prototype.map.call(data, String));
    }
    gLogEntries.push(entry);
  },

  entries() {
    return gLogEntries;
  }
});
