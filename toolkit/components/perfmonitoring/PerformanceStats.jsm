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
 * This API exposes data from several probes implemented by the JavaScript VM.
 * See `PerformanceStats.getMonitor()` for information on how to monitor data
 * from one or more probes and `PerformanceData` for the information obtained
 * from the probes.
 *
 * Data is collected by "Performance Group". Typically, a Performance Group
 * is an add-on, or a frame, or the internals of the application.
 */

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

// The nsIPerformanceStatsService provides lower-level
// access to SpiderMonkey and the probes.
XPCOMUtils.defineLazyServiceGetter(this, "performanceStatsService",
  "@mozilla.org/toolkit/performance-stats-service;1",
  Ci.nsIPerformanceStatsService);

// The finalizer lets us automatically release (and when possible deactivate)
// probes when a monitor is garbage-collected.
XPCOMUtils.defineLazyServiceGetter(this, "finalizer",
  "@mozilla.org/toolkit/finalizationwitness;1",
  Ci.nsIFinalizationWitnessService
);


// The topic used to notify that a PerformanceMonitor has been garbage-collected
// and that we can release/close the probes it holds.
const FINALIZATION_TOPIC = "performancemonitor-finalize";

const PROPERTIES_META_IMMUTABLE = ["name", "addonId", "isSystem"];
const PROPERTIES_META = [...PROPERTIES_META_IMMUTABLE, "windowId", "title"];

/**
 * Access to a low-level performance probe.
 *
 * Each probe is dedicated to some form of performance monitoring.
 * As each probe may have a performance impact, a probe is activated
 * only when a client has requested a PerformanceMonitor for this probe,
 * and deactivated once all clients are disposed of.
 */
function Probe(name, impl) {
  this._name = name;
  this._counter = 0;
  this._impl = impl;
}
Probe.prototype = {
  /**
   * Acquire the probe on behalf of a client.
   *
   * If the probe was inactive, activate it. Note that activating a probe
   * can incur a memory or performance cost.
   */
  acquire: function() {
    if (this._counter == 0) {
      this._impl.isActive = true;
    }
    this._counter++;
  },

  /**
   * Release the probe on behalf of a client.
   *
   * If this was the last client for this probe, deactivate it.
   */
  release: function() {
    this._counter--;
    if (this._counter == 0) {
      this._impl.isActive = false;
    }
  },

  /**
   * Obtain data from this probe, once it is available.
   *
   * @param {nsIPerformanceStats} xpcom A xpcom object obtained from
   * SpiderMonkey. Only the fields updated by the low-level probe
   * are in a specified state.
   * @return {object} An object containing the data extracted from this
   * probe. Actual format depends on the probe.
   */
  extract: function(xpcom) {
    if (!this._impl.isActive) {
      throw new Error(`Probe is inactive: ${this._name}`);
    }
    return this._impl.extract(xpcom);
  },

  /**
   * @param {object} a An object returned by `this.extract()`.
   * @param {object} b An object returned by `this.extract()`.
   *
   * @return {true} If `a` and `b` hold identical values.
   */
  isEqual: function(a, b) {
    if (a == null && b == null) {
      return true;
    }
    if (a != null && b != null) {
      return this._impl.isEqual(a, b);
    }
    return false;
  },

  /**
   * @param {object} a An object returned by `this.extract()`. May
   * NOT be `null`.
   * @param {object} b An object returned by `this.extract()`. May
   * be `null`.
   *
   * @return {object} An object representing `a - b`. If `b` is
   * `null`, this is `a`.
   */
  substract: function(a, b) {
    if (a == null) {
      throw new TypeError();
    }
    if (b == null) {
      return a;
    }
    return this._impl.substract(a, b);
  },

  /**
   * The name of the probe.
   */
  get name() {
    return this._name;
  }
};

// Utility function. Return the position of the last non-0 item in an
// array, or -1 if there isn't any such item.
function lastNonZero(array) {
  for (let i = array.length - 1; i >= 0; --i) {
    if (array[i] != 0) {
      return i;
    }
  }
  return -1;
}

/**
 * The actual Probes implemented by SpiderMonkey.
 */
let Probes = {
  /**
   * A probe measuring jank.
   *
   * Data provided by this probe uses the following format:
   *
   * @field {number} totalCPUTime The total amount of time spent using the
   * CPU for this performance group, in µs.
   * @field {number} totalSystemTime The total amount of time spent in the
   * kernel for this performance group, in µs.
   * @field {Array<number>} durations An array containing at each position `i`
   * the number of times execution of this component has lasted at least `2^i`
   * milliseconds.
   * @field {number} longestDuration The index of the highest non-0 value in
   * `durations`.
   */
  jank: new Probe("jank", {
    set isActive(x) { /* always active in the current implementation */ },
    get isActive() { return true; },
    extract: function(xpcom) {
      let durations = xpcom.getDurations();
      return {
        totalUserTime: xpcom.totalUserTime,
        totalSystemTime: xpcom.totalSystemTime,
        durations: durations,
        longestDuration: lastNonZero(durations)
      }
    },
    isEqual: function(a, b) {
      // invariant: `a` and `b` are both non-null
      if (a.totalUserTime != b.totalUserTime) {
        return false;
      }
      if (a.totalSystemTime != b.totalSystemTime) {
        return false;
      }
      for (let i = 0; i < a.durations.length; ++i) {
        if (a.durations[i] != b.durations[i]) {
          return false;
        }
      }
      return true;
    },
    substract: function(a, b) {
      // invariant: `a` and `b` are both non-null
      let result = {
        totalUserTime: a.totalUserTime - b.totalUserTime,
        totalSystemTime: a.totalSystemTime - b.totalSystemTime,
        durations: [],
        longestDuration: -1,
      };
      for (let i = 0; i < a.durations.length; ++i) {
        result.durations[i] = a.durations[i] - b.durations[i];
      }
      result.longestDuration = lastNonZero(result.durations);
      return result;
    }
  }),

  /**
   * A probe measuring CPOW activity.
   *
   * Data provided by this probe uses the following format:
   *
   * @field {number} totalCPOWTime The amount of wallclock time
   * spent executing blocking cross-process calls, in µs.
   */
  cpow: new Probe("cpow", {
    set isActive(x) { /* always active in the current implementation */ },
    get isActive() { return true; },
    extract: function(xpcom) {
      return {
        totalCPOWTime: xpcom.totalCPOWTime
      };
    },
    isEqual: function(a, b) {
      return a.totalCPOWTime == b.totalCPOWTime;
    },
    substract: function(a, b) {
      return {
        totalCPOWTime: a.totalCPOWTime - b.totalCPOWTime
      };
    }
  }),

  /**
   * A probe measuring activations, i.e. the number
   * of times code execution has entered a given
   * PerformanceGroup.
   *
   * Note that this probe is always active.
   *
   * Data provided by this probe uses the following format:
   * @type {number} ticks The number of times execution has entered
   * this performance group.
   */
  ticks: new Probe("ticks", {
    set isActive(x) { /* this probe cannot be deactivated */ },
    get isActive() { return true; },
    extract: function(xpcom) {
      return {
        ticks: xpcom.ticks
      };
    },
    isEqual: function(a, b) {
      return a.ticks == b.ticks;
    },
    substract: function(a, b) {
      return {
        ticks: a.ticks - b.ticks
      };
    }
  }),
};


/**
 * A monitor for a set of probes.
 *
 * Keeping probes active when they are unused is often a bad
 * idea for performance reasons. Upon destruction, or whenever
 * a client calls `dispose`, this monitor releases the probes,
 * which may let the system deactivate them.
 */
function PerformanceMonitor(probes) {
  this._probes = probes;
  
  // Activate low-level features as needed
  for (let probe of probes) {
    probe.acquire();
  }

  // A finalization witness. At some point after the garbage-collection of
  // `this` object, a notification of `FINALIZATION_TOPIC` will be triggered
  // with `id` as message.
  this._id = PerformanceMonitor.makeId();
  this._finalizer = finalizer.make(FINALIZATION_TOPIC, this._id)
  PerformanceMonitor._monitors.set(this._id, probes);
}
PerformanceMonitor.prototype = {
  /**
   * Return asynchronously a snapshot with the data
   * for each probe monitored by this PerformanceMonitor.
   *
   * All numeric values are non-negative and can only increase. Depending on
   * the probe and the underlying operating system, probes may not be available
   * immediately and may miss some activity.
   *
   * Clients should NOT expect that the first call to `promiseSnapshot()`
   * will return a `Snapshot` in which all values are 0. For most uses,
   * the appropriate scenario is to perform a first call to `promiseSnapshot()`
   * to obtain a baseline, and then watch evolution of the values by calling
   * `promiseSnapshot()` and `substract()`.
   *
   * On the other hand, numeric values are also monotonic across several instances
   * of a PerformanceMonitor with the same probes. 
   *  let a = PerformanceStats.getMonitor(someProbes);
   *  let snapshot1 = yield a.promiseSnapshot();
   *
   *  // ...
   *  let b = PerformanceStats.getMonitor(someProbes); // Same list of probes
   *  let snapshot2 = yield b.promiseSnapshot();
   *
   *  // all values of `snapshot2` are greater or equal to values of `snapshot1`.
   *
   * @return {Promise}
   * @resolve {Snapshot}
   */
  promiseSnapshot: function() {
    if (!this._finalizer) {
      throw new Error("dispose() has already been called, this PerformanceMonitor is not usable anymore");
    }
    // Current implementation is actually synchronous.
    return Promise.resolve().then(() => new Snapshot({
      xpcom: performanceStatsService.getSnapshot(),
      probes: this._probes
    }));
  },

  /**
   * Release the probes used by this monitor.
   *
   * Releasing probes as soon as they are unused is a good idea, as some probes
   * cost CPU and/or memory.
   */
  dispose: function() {
    if (!this._finalizer) {
      return;
    }
    this._finalizer.forget();
    PerformanceMonitor.dispose(this._id);

    // As a safeguard against double-release, reset everything to `null`
    this._probes = null;
    this._id = null;
    this._finalizer = null;
  }
};
/**
 * @type {Map<string, Array<string>>} A map from id (as produced by `makeId`)
 * to list of probes. Used to deallocate a list of probes during finalization.
 */
PerformanceMonitor._monitors = new Map();

/**
 * Create a `PerformanceMonitor` for a list of probes, register it for
 * finalization.
 */
PerformanceMonitor.make = function(probeNames) {
  // Sanity checks
  if (!Array.isArray(probeNames)) {
    throw new TypeError("Expected an array, got " + probes);
  }
  let probes = [];
  for (let probeName of probeNames) {
    if (!(probeName in Probes)) {
      throw new TypeError("Probe not implemented: " + k);
    }
    probes.push(Probes[probeName]);
  }

  return new PerformanceMonitor(probes);
};

/**
 * Implementation of `dispose`.
 *
 * The actual implementation of `dispose` is as a method of `PerformanceMonitor`,
 * rather than `PerformanceMonitor.prototype`, to avoid needing a strong reference
 * to instances of `PerformanceMonitor`, which would defeat the purpose of
 * finalization.
 */
PerformanceMonitor.dispose = function(id) {
  let probes = PerformanceMonitor._monitors.get(id);
  if (!probes) {
    throw new TypeError("`dispose()` has already been called on this monitor");
  }

  PerformanceMonitor._monitors.delete(id);
  for (let probe of probes) {
    probe.release();
  }
}

// Generate a unique id for each PerformanceMonitor. Used during
// finalization.
PerformanceMonitor._counter = 0;
PerformanceMonitor.makeId = function() {
  return "PerformanceMonitor-" + (this._counter++);
}

// Once a `PerformanceMonitor` has been garbage-collected,
// release the probes unless `dispose()` has already been called.
Services.obs.addObserver(function(subject, topic, value) {
  PerformanceMonitor.dispose(value);
}, FINALIZATION_TOPIC, false);

// Public API
this.PerformanceStats = {
  /**
   * Create a monitor for observing a set of performance probes.
   */
  getMonitor: function(probes) {
    return PerformanceMonitor.make(probes);
  }
};


/**
 * Information on a single performance group.
 *
 * This offers the following fields:
 *
 * @field {string} name The name of the performance group:
 * - for the process itself, "<process>";
 * - for platform code, "<platform>";
 * - for an add-on, the identifier of the addon (e.g. "myaddon@foo.bar");
 * - for a webpage, the url of the page.
 *
 * @field {string} addonId The identifier of the addon (e.g. "myaddon@foo.bar").
 *
 * @field {string|null} title The title of the webpage to which this code
 *  belongs. Note that this is the title of the entire webpage (i.e. the tab),
 *  even if the code is executed in an iframe. Also note that this title may
 *  change over time.
 *
 * @field {number} windowId The outer window ID of the top-level nsIDOMWindow
 *  to which this code belongs. May be 0 if the code doesn't belong to any
 *  nsIDOMWindow.
 *
 * @field {boolean} isSystem `true` if the component is a system component (i.e.
 *  an add-on or platform-code), `false` otherwise (i.e. a webpage).
 *
 * @field {object|undefined} activations See the documentation of probe "ticks".
 *   `undefined` if this probe is not active.
 *
 * @field {object|undefined} jank See the documentation of probe "jank".
 *   `undefined` if this probe is not active.
 *
 * @field {object|undefined} cpow See the documentation of probe "cpow".
 *   `undefined` if this probe is not active.
 */
function PerformanceData({xpcom, probes}) {
  for (let k of PROPERTIES_META) {
    this[k] = xpcom[k];
  }
  for (let probe of probes) {
    this[probe.name] = probe.extract(xpcom);
  }
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
    for (let probeName of Object.keys(Probes)) {
      let probe = Probes[probeName];
      if (!probe.isEqual(this[probeName], to[probeName])) {
        return false;
      }
    }
  },

  /**
   * Compute the delta between two instances of `PerformanceData`.
   *
   * @param {PerformanceData|null} to. If `null`, assumed an instance of
   * `PerformanceData` in which all numeric values are 0.
   *
   * @return {PerformanceDiff} The performance usage between `to` and `this`.
   */
  subtract: function(to = null) {
    return new PerformanceDiff(this, to);
  }
};

/**
 * The delta between two instances of `PerformanceData`.
 *
 * Used to monitor resource usage between two timestamps.
 */
function PerformanceDiff(current, old = null) {
  for (let k of PROPERTIES_META) {
    this[k] = current[k];
  }

  for (let probeName of Object.keys(Probes)) {
    let other = old ? old[probeName] : null;
    if (probeName in current) {
      this[probeName] = Probes[probeName].substract(current[probeName], other);
    }
  }
}

/**
 * A snapshot of the performance usage of the process.
 */
function Snapshot({xpcom, probes}) {
  this.componentsData = [];
  let enumeration = xpcom.getComponentsData().enumerate();
  while (enumeration.hasMoreElements()) {
    let stat = enumeration.getNext().QueryInterface(Ci.nsIPerformanceStats);
    this.componentsData.push(new PerformanceData({xpcom: stat, probes}));
  }
  this.processData = new PerformanceData({xpcom: xpcom.getProcessData(), probes});
}


// In the current implementation, all probes are always active.
performanceStatsService.isStopwatchActive = true;
