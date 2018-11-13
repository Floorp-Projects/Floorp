/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["HistogramStopwatch"];

// A helper for histogram timer probes.
class HistogramStopwatch {
  constructor(aName, aAssociated) {
    this._name = aName;
    this._obj = aAssociated;
  }

  isRunning() {
    return TelemetryStopwatch.running(this._name, this._obj);
  }

  start() {
    if (this.isRunning()) {
      this.cancel();
    }
    TelemetryStopwatch.start(this._name, this._obj);
  }

  finish() {
    TelemetryStopwatch.finish(this._name, this._obj);
  }

  cancel() {
    TelemetryStopwatch.cancel(this._name, this._obj);
  }

  timeElapsed() {
    return TelemetryStopwatch.timeElapsed(this._name, this._obj, false);
  }
}
