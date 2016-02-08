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
 *
 * Generally, if you have the choice between PerformanceStats and PerformanceWatcher,
 * you should favor PerformanceWatcher.
 */

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/ObjectUtils.jsm", this);
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
  "resource://gre/modules/Timer.jsm");

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

const PROPERTIES_META_IMMUTABLE = ["addonId", "isSystem", "isChildProcess", "groupId", "processId"];
const PROPERTIES_META = [...PROPERTIES_META_IMMUTABLE, "windowId", "title", "name"];

// How long we wait for children processes to respond.
const MAX_WAIT_FOR_CHILD_PROCESS_MS = 5000;

var isContent = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;
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
      Process.broadcast("acquire", [this._name]);
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
      try {
        this._impl.isActive = false;
      } catch (ex) {
        if (ex && typeof ex == "object" && ex.result == Components.results.NS_ERROR_NOT_AVAILABLE) {
          // The service has already been shutdown. Ignore further shutdown requests.
          return;
        }
        throw ex;
      }
      Process.broadcast("release", [this._name]);
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
  subtract: function(a, b) {
    if (a == null) {
      throw new TypeError();
    }
    if (b == null) {
      return a;
    }
    return this._impl.subtract(a, b);
  },

  importChildCompartments: function(parent, children) {
    if (!Array.isArray(children)) {
      throw new TypeError();
    }
    if (!parent || !(parent instanceof PerformanceDataLeaf)) {
      throw new TypeError();
    }
    return this._impl.importChildCompartments(parent, children);
  },

  /**
   * The name of the probe.
   */
  get name() {
    return this._name;
  },

  compose: function(stats) {
    if (!Array.isArray(stats)) {
      throw new TypeError();
    }
    return this._impl.compose(stats);
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
var Probes = {
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
    set isActive(x) {
      performanceStatsService.isMonitoringJank = x;
    },
    get isActive() {
      return performanceStatsService.isMonitoringJank;
    },
    extract: function(xpcom) {
      let durations = xpcom.getDurations();
      return {
        totalUserTime: xpcom.totalUserTime,
        totalSystemTime: xpcom.totalSystemTime,
        totalCPUTime: xpcom.totalUserTime + xpcom.totalSystemTime,
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
    subtract: function(a, b) {
      // invariant: `a` and `b` are both non-null
      let result = {
        totalUserTime: a.totalUserTime - b.totalUserTime,
        totalSystemTime: a.totalSystemTime - b.totalSystemTime,
        totalCPUTime: a.totalCPUTime - b.totalCPUTime,
        durations: [],
        longestDuration: -1,
      };
      for (let i = 0; i < a.durations.length; ++i) {
        result.durations[i] = a.durations[i] - b.durations[i];
      }
      result.longestDuration = lastNonZero(result.durations);
      return result;
    },
    importChildCompartments: function() { /* nothing to do */ },
    compose: function(stats) {
      let result = {
        totalUserTime: 0,
        totalSystemTime: 0,
        totalCPUTime: 0,
        durations: [],
        longestDuration: -1
      };
      for (let stat of stats) {
        result.totalUserTime += stat.totalUserTime;
        result.totalSystemTime += stat.totalSystemTime;
        result.totalCPUTime += stat.totalCPUTime;
        for (let i = 0; i < stat.durations.length; ++i) {
          result.durations[i] += stat.durations[i];
        }
        result.longestDuration = Math.max(result.longestDuration, stat.longestDuration);
      }
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
    set isActive(x) {
      performanceStatsService.isMonitoringCPOW = x;
    },
    get isActive() {
      return performanceStatsService.isMonitoringCPOW;
    },
    extract: function(xpcom) {
      return {
        totalCPOWTime: xpcom.totalCPOWTime
      };
    },
    isEqual: function(a, b) {
      return a.totalCPOWTime == b.totalCPOWTime;
    },
    subtract: function(a, b) {
      return {
        totalCPOWTime: a.totalCPOWTime - b.totalCPOWTime
      };
    },
    importChildCompartments: function() { /* nothing to do */ },
    compose: function(stats) {
      let totalCPOWTime = 0;
      for (let stat of stats) {
        totalCPOWTime += stat.totalCPOWTime;
      }
      return { totalCPOWTime };
    },
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
    subtract: function(a, b) {
      return {
        ticks: a.ticks - b.ticks
      };
    },
    importChildCompartments: function() { /* nothing to do */ },
    compose: function(stats) {
      let ticks = 0;
      for (let stat of stats) {
        ticks += stat.ticks;
      }
      return { ticks };
    },
  }),

  compartments: new Probe("compartments", {
    set isActive(x) {
      performanceStatsService.isMonitoringPerCompartment = x;
    },
    get isActive() {
      return performanceStatsService.isMonitoringPerCompartment;
    },
    extract: function(xpcom) {
      return null;
    },
    isEqual: function(a, b) {
      return true;
    },
    subtract: function(a, b) {
      return true;
    },
    importChildCompartments: function(parent, children) {
      parent.children = children;
    },
    compose: function(stats) {
      return null;
    },
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
   * The names of probes activated in this monitor.
   */
  get probeNames() {
    return this._probes.map(probe => probe.name);
  },

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
   * `promiseSnapshot()` and `subtract()`.
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
   * @param {object} options If provided, an object that may contain the following
   *   fields:
   *   {Array<string>} probeNames The subset of probes to use for this snapshot.
   *      These probes must be a subset of the probes active in the monitor.
   *
   * @return {Promise}
   * @resolve {Snapshot}
   */
  _checkBeforeSnapshot: function(options) {
    if (!this._finalizer) {
      throw new Error("dispose() has already been called, this PerformanceMonitor is not usable anymore");
    }
    let probes;
    if (options && options.probeNames || undefined) {
      if (!Array.isArray(options.probeNames)) {
        throw new TypeError();
      }
      // Make sure that we only request probes that we have
      for (let probeName of options.probeNames) {
        let probe = this._probes.find(probe => probe.name == probeName);
        if (!probe) {
          throw new TypeError(`I need probe ${probeName} but I only have ${this.probeNames}`);
        }
        if (!probes) {
          probes = [];
        }
        probes.push(probe);
      }
    } else {
      probes = this._probes;
    }
    return probes;
  },
  promiseContentSnapshot: function(options = null) {
    this._checkBeforeSnapshot(options);
    return (new ProcessSnapshot(performanceStatsService.getSnapshot()));
  },
  promiseSnapshot: function(options = null) {
    let probes = this._checkBeforeSnapshot(options);
    return Task.spawn(function*() {
      let childProcesses = yield Process.broadcastAndCollect("collect", {probeNames: probes.map(p => p.name)});
      let xpcom = performanceStatsService.getSnapshot();
      return new ApplicationSnapshot({
        xpcom,
        childProcesses,
        probes,
        date: Cu.now()
      });
    });
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
      throw new TypeError("Probe not implemented: " + probeName);
    }
    probes.push(Probes[probeName]);
  }

  return (new PerformanceMonitor(probes));
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
function PerformanceDataLeaf({xpcom, json, probes}) {
  if (xpcom && json) {
    throw new TypeError("Cannot import both xpcom and json data");
  }
  let source = xpcom || json;
  for (let k of PROPERTIES_META) {
    this[k] = source[k];
  }
  if (xpcom) {
    for (let probe of probes) {
      this[probe.name] = probe.extract(xpcom);
    }
    this.isChildProcess = false;
  } else {
    for (let probe of probes) {
      this[probe.name] = json[probe.name];
    }
    this.isChildProcess = true;
  }
  this.owner = null;
}
PerformanceDataLeaf.prototype = {
  /**
   * Compare two instances of `PerformanceData`
   *
   * @return `true` if `this` and `to` have equal values in all fields.
   */
  equals: function(to) {
    if (!(to instanceof PerformanceDataLeaf)) {
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
    return (new PerformanceDiffLeaf(this, to));
  }
};

function PerformanceData(timestamp) {
  this._parent = null;
  this._content = new Map();
  this._all = [];
  this._timestamp = timestamp;
}
PerformanceData.prototype = {
  addChild: function(stat) {
    if (!(stat instanceof PerformanceDataLeaf)) {
      throw new TypeError(); // FIXME
    }
    if (!stat.isChildProcess) {
      throw new TypeError(); // FIXME
    }
    this._content.set(stat.groupId, stat);
    this._all.push(stat);
    stat.owner = this;
  },
  setParent: function(stat) {
    if (!(stat instanceof PerformanceDataLeaf)) {
      throw new TypeError(); // FIXME
    }
    if (stat.isChildProcess) {
      throw new TypeError(); // FIXME
    }
    this._parent = stat;
    this._all.push(stat);
    stat.owner = this;
  },
  equals: function(to) {
    if (this._parent && !to._parent) {
      return false;
    }
    if (!this._parent && to._parent) {
      return false;
    }
    if (this._content.size != to._content.size) {
      return false;
    }
    if (this._parent && !this._parent.equals(to._parent)) {
      return false;
    }
    for (let [k, v] of this._content) {
      let v2 = to._content.get(k);
      if (!v2) {
        return false;
      }
      if (!v.equals(v2)) {
        return false;
      }
    }
    return true;
  },
  subtract: function(to = null) {
    return (new PerformanceDiff(this, to));
  },
  get addonId() {
    return this._all[0].addonId;
  },
  get title() {
    return this._all[0].title;
  }
};

function PerformanceDiff(current, old = null) {
  this.addonId = current.addonId;
  this.title = current.title;
  this.windowId = current.windowId;
  this.deltaT = old ? current._timestamp - old._timestamp : Infinity;
  this._all = [];

  // Handle the parent, if any.
  if (current._parent) {
    this._parent = old?current._parent.subtract(old._parent):current._parent;
    this._all.push(this._parent);
    this._parent.owner = this;
  } else {
    this._parent = null;
  }

  // Handle the children, if any.
  this._content = new Map();
  for (let [k, stat] of current._content) {
    let diff = stat.subtract(old ? old._content.get(k) : null);
    this._content.set(k, diff);
    this._all.push(diff);
    diff.owner = this;
  }

  // Now consolidate data
  for (let k of Object.keys(Probes)) {
    if (!(k in this._all[0])) {
      // The stats don't contain data from this probe.
      continue;
    }
    let data = this._all.map(item => item[k]);
    let probe = Probes[k];
    this[k] = probe.compose(data);
  }
}
PerformanceDiff.prototype = {
  toString: function() {
    return `[PerformanceDiff] ${this.key}`;
  },
  get windowIds() {
    return this._all.map(item => item.windowId).filter(x => !!x);
  },
  get groupIds() {
    return this._all.map(item => item.groupId);
  },
  get key() {
    if (this.addonId) {
      return this.addonId;
    }
    if (this._parent) {
      return this._parent.windowId;
    }
    return this._all[0].groupId;
  },
  get names() {
    return this._all.map(item => item.name);
  },
  get processes() {
    return this._all.map(item => ({ isChildProcess: item.isChildProcess, processId: item.processId}));
  }
};

/**
 * The delta between two instances of `PerformanceDataLeaf`.
 *
 * Used to monitor resource usage between two timestamps.
 */
function PerformanceDiffLeaf(current, old = null) {
  for (let k of PROPERTIES_META) {
    this[k] = current[k];
  }

  for (let probeName of Object.keys(Probes)) {
    let other = null;
    if (old && probeName in old) {
      other = old[probeName];
    }

    if (probeName in current) {
      this[probeName] = Probes[probeName].subtract(current[probeName], other);
    }
  }
}

/**
 * A snapshot of a single process.
 */
function ProcessSnapshot({xpcom, probes}) {
  this.componentsData = [];

  let subgroups = new Map();
  let enumeration = xpcom.getComponentsData().enumerate();
  while (enumeration.hasMoreElements()) {
    let xpcom = enumeration.getNext().QueryInterface(Ci.nsIPerformanceStats);
    let stat = (new PerformanceDataLeaf({xpcom, probes}));

    if (!xpcom.parentId) {
      this.componentsData.push(stat);
    } else {
      let siblings = subgroups.get(xpcom.parentId);
      if (!siblings) {
        subgroups.set(xpcom.parentId, (siblings = []));
      }
      siblings.push(stat);
    }
  }

  for (let group of this.componentsData) {
    for (let probe of probes) {
      probe.importChildCompartments(group, subgroups.get(group.groupId) || []);
    }
  }

  this.processData = (new PerformanceDataLeaf({xpcom: xpcom.getProcessData(), probes}));
}

/**
 * A snapshot of the performance usage of the application.
 *
 * @param {nsIPerformanceSnapshot} xpcom The data acquired from this process.
 * @param {Array<Object>} childProcesses The data acquired from children processes.
 * @param {Array<Probe>} probes The active probes.
 */
function ApplicationSnapshot({xpcom, childProcesses, probes, date}) {
  ProcessSnapshot.call(this, {xpcom, probes});

  this.addons = new Map();
  this.webpages = new Map();
  this.date = date;

  // Child processes
  for (let {componentsData} of (childProcesses || [])) {
    // We are only interested in `componentsData` for the time being.
    for (let json of componentsData) {
      let leaf = (new PerformanceDataLeaf({json, probes}));
      this.componentsData.push(leaf);
    }
  }

  for (let leaf of this.componentsData) {
    let key, map;
    if (leaf.addonId) {
      key = leaf.addonId;
      map = this.addons;
    } else if (leaf.windowId) {
      key = leaf.windowId;
      map = this.webpages;
    } else {
      continue;
    }

    let combined = map.get(key);
    if (!combined) {
      combined = new PerformanceData(date);
      map.set(key, combined);
    }
    if (leaf.isChildProcess) {
      combined.addChild(leaf);
    } else {
      combined.setParent(leaf);
    }
  }
}

/**
 * Communication with other processes
 */
var Process = {
  // a counter used to match responses to requests
  _idcounter: 0,
  _loader: null,
  /**
   * If we are in a child process, return `null`.
   * Otherwise, return the global parent process message manager
   * and load the script to connect to children processes.
   */
  get loader() {
    if (isContent) {
      return null;
    }
    if (this._loader) {
      return this._loader;
    }
    Services.ppmm.loadProcessScript("resource://gre/modules/PerformanceStats-content.js",
      true/*including future processes*/);
    return this._loader = Services.ppmm;
  },

  /**
   * Broadcast a message to all children processes.
   *
   * NOOP if we are in a child process.
   */
  broadcast: function(topic, payload) {
    if (!this.loader) {
      return;
    }
    this.loader.broadcastAsyncMessage("performance-stats-service-" + topic, {payload});
  },

  /**
   * Brodcast a message to all children processes and wait for answer.
   *
   * NOOP if we are in a child process, or if we have no children processes,
   * in which case we return `undefined`.
   *
   * @return {undefined} If we have no children processes, in particular
   * if we are in a child process.
   * @return {Promise<Array<Object>>} If we have children processes, an
   * array of objects with a structure similar to PerformanceData. Note
   * that the array may be empty if no child process responded.
   */
  broadcastAndCollect: Task.async(function*(topic, payload) {
    if (!this.loader || this.loader.childCount == 1) {
      return undefined;
    }
    const TOPIC = "performance-stats-service-" + topic;
    let id = this._idcounter++;

    // The number of responses we are expecting. Note that we may
    // not receive all responses if a process is too long to respond.
    let expecting = this.loader.childCount;

    // The responses we have collected, in arbitrary order.
    let collected = [];
    let deferred = PromiseUtils.defer();

    let observer = function({data, target}) {
      if (data.id != id) {
        // Collision between two collections,
        // ignore the other one.
        return;
      }
      if (data.data) {
        collected.push(data.data)
      }
      if (--expecting > 0) {
        // We are still waiting for at least one response.
        return;
      }
      deferred.resolve();
    };
    this.loader.addMessageListener(TOPIC, observer);
    this.loader.broadcastAsyncMessage(
      TOPIC,
      {id, payload}
    );

    // Processes can die/freeze/be busy loading a page..., so don't expect
    // that they will always respond.
    let timeout = setTimeout(() => {
      if (expecting == 0) {
        return;
      }
      deferred.resolve();
    }, MAX_WAIT_FOR_CHILD_PROCESS_MS);

    deferred.promise.then(() => {
      clearTimeout(timeout);
    });

    yield deferred.promise;
    this.loader.removeMessageListener(TOPIC, observer);

    return collected;
  })
};
