/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export var InitializationTracker = {
  initialized: false,
  onInitialized(profilerTime) {
    if (!this.initialized) {
      this.initialized = true;
      ChromeUtils.addProfilerMarker(
        "GeckoView Initialization END",
        profilerTime
      );
    }
  },
};

// A helper for timing_distribution metrics.
export class GleanStopwatch {
  constructor(aTimingDistribution) {
    this._metric = aTimingDistribution;
  }

  isRunning() {
    return !!this._timerId;
  }

  start() {
    if (this.isRunning()) {
      this.cancel();
    }
    this._timerId = this._metric.start();
  }

  finish() {
    if (this.isRunning()) {
      this._metric.stopAndAccumulate(this._timerId);
      this._timerId = null;
    }
  }

  cancel() {
    if (this.isRunning()) {
      this._metric.cancel(this._timerId);
      this._timerId = null;
    }
  }
}
