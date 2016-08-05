/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["TelemetryLog"];

const Cc = Components.classes;
const Ci = Components.interfaces;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
var gLogEntries = [];

this.TelemetryLog = Object.freeze({
  log: function(id, data) {
    id = String(id);
    var ts;
    try {
      ts = Math.floor(Telemetry.msSinceProcessStart());
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

  entries: function() {
    return gLogEntries;
  }
});
