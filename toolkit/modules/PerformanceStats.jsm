// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PerformanceStats"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

/**
 * API for querying and examining performance data.
 *
 * The data exposed by this API is computed internally by the JavaScript VM.
 * See `PerformanceData` for the detail of the information provided by this
 * API.
 */

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

let performanceStatsService =
  Cc["@mozilla.org/toolkit/performance-stats-service;1"].
  getService(Ci.nsIPerformanceStatsService);


const PROPERTIES_NUMBERED = ["totalUserTime", "totalSystemTime", "totalCPOWTime", "ticks"];
const PROPERTIES_META = ["name", "addonId", "isSystem"];
const PROPERTIES_FLAT = [...PROPERTIES_NUMBERED, ...PROPERTIES_META];

/**
 * Information on a single component.
 *
 * This offers the following fields:
 *
 * @field {string} name The name of the component:
 * - for the process itself, "<process>";
 * - for platform code, "<platform>";
 * - for an add-on, the identifier of the addon (e.g. "myaddon@foo.bar");
 * - for a webpage, the url of the page.
 *
 * @field {string} addonId The identifier of the addon (e.g. "myaddon@foo.bar").
 *
 * @field {boolean} isSystem `true` if the component is a system component (i.e.
 *  an add-on or platform-code), `false` otherwise (i.e. a webpage).
 *
 * @field {number} totalUserTime The total amount of time spent executing code.
 *
 * @field {number} totalSystemTime The total amount of time spent executing
 *  system calls.
 *
 * @field {number} totalCPOWTime The total amount of time spent waiting for
 *  blocking cross-process communications
 *
 * @field {number} ticks The number of times the JavaScript VM entered the code
 *  of this component to execute it.
 *
 * @field {Array<number>} durations An array containing at each position `i`
 * the number of times execution of this component has lasted at least `2^i`
 * milliseconds.
 *
 * All numeric values are non-negative and can only increase.
 */
function PerformanceData(xpcom) {
  for (let k of PROPERTIES_FLAT) {
    this[k] = xpcom[k];
  }
  this.durations = xpcom.getDurations();
}
PerformanceData.prototype = {
  /**
   * Compare two instances of `PerformanceData`
   *
   * @return `true` if `this` and `to` have equal values in all fields.
   */
  equals: function(to) {
    if (!(to instanceof PerformanceData)) {
      throw new TypeError();
    }
    for (let k of PROPERTIES_FLAT) {
      if (this[k] != to[k]) {
        return false;
      }
    }
    for (let i = 0; i < this.durations.length; ++i) {
      if (to.durations[i] != this.durations[i]) {
        return false;
      }
    }
    return true;
  },

  /**
   * Compute the delta between two instances of `PerformanceData`.
   *
   * @param {PerformanceData|null} to. If `null`, assumed an instance of
   * `PerformanceData` in which all numeric values are 0.
   *
   * @return {PerformanceDiff} The performance usage between `to` and `this`.
   */
  substract: function(to = null) {
    return new PerformanceDiff(this, to);
  }
};

/**
 * The delta between two instances of `PerformanceData`.
 *
 * Used to monitor resource usage between two timestamps.
 *
 * @field {number} longestDuration An indication of the longest
 * execution duration between two timestamps:
 * - -1 == less than 1ms
 * - 0 == [1, 2[ ms
 * - 1 == [2, 4[ ms
 * - 3 == [4, 8[ ms
 * - 4 == [8, 16[ ms
 * - ...
 * - 7 == [128, ...] ms
 */
function PerformanceDiff(current, old = null) {
  for (let k of PROPERTIES_META) {
    this[k] = current[k];
  }

  if (old) {
    if (!(old instanceof PerformanceData)) {
      throw new TypeError();
    }
    if (current.durations.length != old.durations.length) {
      throw new TypeError("Internal error: mismatched length for `durations`.");
    }

    this.durations = [];

    this.longestDuration = -1;

    for (let i = 0; i < current.durations.length; ++i) {
      let delta = current.durations[i] - old.durations[i];
      this.durations[i] = delta;
      if (delta > 0) {
        this.longestDuration = i;
      }
    }
    for (let k of PROPERTIES_NUMBERED) {
      this[k] = current[k] - old[k];
    }
  } else {
    this.durations = current.durations.slice(0);

    for (let k of PROPERTIES_NUMBERED) {
      this[k] = current[k];
    }

    this.longestDuration = -1;
    for (let i = this.durations.length - 1; i >= 0; --i) {
      if (this.durations[i] > 0) {
        this.longestDuration = i;
        break;
      }
    }
  }
}

/**
 * A snapshot of the performance usage of the process.
 */
function Snapshot(xpcom) {
  this.componentsData = [];
  let enumeration = xpcom.getComponentsData().enumerate();
  while (enumeration.hasMoreElements()) {
    let stat = enumeration.getNext().QueryInterface(Ci.nsIPerformanceStats);
    this.componentsData.push(new PerformanceData(stat));
  }
  this.processData = new PerformanceData(xpcom.getProcessData());
}


this.PerformanceStats = {
  /**
   * Activate monitoring.
   */
  init() {
    //
    // The implementation actually does nothing, as monitoring is
    // initiated when loading the module.
    //
    // This function is actually provided as a gentle way to ensure
    // that client code that imports `PerformanceStats` lazily
    // does not forget to force the import, hence triggering
    // actual load of the module.
    //
  },

  /**
   * Get a snapshot of the performance usage of the current process.
   *
   * @type {Snapshot}
   */
  getSnapshot() {
    return new Snapshot(performanceStatsService.getSnapshot());
  },
};

performanceStatsService.isStopwatchActive = true;
