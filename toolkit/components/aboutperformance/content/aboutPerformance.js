/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const { AddonWatcher } = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});
const { PerformanceStats } = Cu.import("resource://gre/modules/PerformanceStats.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const { ObjectUtils } = Cu.import("resource://gre/modules/ObjectUtils.jsm", {});

// about:performance observes notifications on this topic.
// if a notification is sent, this causes the page to be updated immediately,
// regardless of whether the page is on pause.
const TEST_DRIVER_TOPIC = "test-about:performance-test-driver";

// about:performance posts notifications on this topic whenever the page
// is updated.
const UPDATE_COMPLETE_TOPIC = "about:performance-update-complete";

// How often we should add a sample to our buffer.
const BUFFER_SAMPLING_RATE_MS = 1000;

// The age of the oldest sample to keep.
const BUFFER_DURATION_MS = 10000;

// How often we should update
const UPDATE_INTERVAL_MS = 5000;

// The name of the application
const BRAND_BUNDLE = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties");
const BRAND_NAME = BRAND_BUNDLE.GetStringFromName("brandShortName");

// The maximal number of items to display before showing a "Show All"
// button.
const MAX_NUMBER_OF_ITEMS_TO_DISPLAY = 3;

// If the frequency of alerts is below this value,
// we consider that the feature has no impact.
const MAX_FREQUENCY_FOR_NO_IMPACT = .05;
// If the frequency of alerts is above `MAX_FREQUENCY_FOR_NO_IMPACT`
// and below this value, we consider that the feature impacts the
// user rarely.
const MAX_FREQUENCY_FOR_RARE = .1;
// If the frequency of alerts is above `MAX_FREQUENCY_FOR_FREQUENT`
// and below this value, we consider that the feature impacts the
// user frequently. Anything above is consider permanent.
const MAX_FREQUENCY_FOR_FREQUENT = .5;

// If the number of high-impact alerts among all alerts is above
// this value, we consider that the feature has a major impact
// on user experience.
const MIN_PROPORTION_FOR_MAJOR_IMPACT = .05;
// Otherwise and if the number of medium-impact alerts among all
// alerts is above this value, we consider that the feature has
// a noticeable impact on user experience.
const MIN_PROPORTION_FOR_NOTICEABLE_IMPACT = .1;

// The current mode. Either `MODE_GLOBAL` to display a summary of results
// since we opened about:performance or `MODE_RECENT` to display the latest
// BUFFER_DURATION_MS ms.
const MODE_GLOBAL = "global";
const MODE_RECENT = "recent";

let tabFinder = {
  update: function() {
    this._map = new Map();
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let win = windows.getNext();
      let tabbrowser = win.gBrowser;
      for (let browser of tabbrowser.browsers) {
        let id = browser.outerWindowID; // May be `null` if the browser isn't loaded yet
        if (id != null) {
          this._map.set(id, browser);
        }
      }
    }
  },

  /**
   * Find the <xul:tab> for a window id.
   *
   * This is useful e.g. for reloading or closing tabs.
   *
   * @return null If the xul:tab could not be found, e.g. if the
   * windowId is that of a chrome window.
   * @return {{tabbrowser: <xul:tabbrowser>, tab: <xul.tab>}} The
   * tabbrowser and tab if the latter could be found.
   */
  get: function(id) {
    let browser = this._map.get(id);
    if (!browser) {
      return null;
    }
    let tabbrowser = browser.getTabBrowser();
    return {tabbrowser, tab:tabbrowser.getTabForBrowser(browser)};
  },

  getAny: function(ids) {
    for (let id of ids) {
      let result = this.get(id);
      if (result) {
        return result;
      }
    }
    return null;
  }
};

/**
 * Returns a Promise that's resolved after the next turn of the event loop.
 *
 * Just returning a resolved Promise would mean that any `then` callbacks
 * would be called right after the end of the current turn, so `setTimeout`
 * is used to delay Promise resolution until the next turn.
 *
 * In mochi tests, it's possible for this to be called after the
 * about:performance window has been torn down, which causes `setTimeout` to
 * throw an NS_ERROR_NOT_INITIALIZED exception. In that case, returning
 * `undefined` is fine.
 */
function wait(ms = 0) {
  try {
    let resolve;
    let p = new Promise(resolve_ => {resolve = resolve_});
    setTimeout(resolve, ms);
    return p;
  } catch (e) {
    dump("WARNING: wait aborted because of an invalid Window state in aboutPerformance.js.\n");
    return undefined;
  }
}

/**
 * The performance of a webpage or an add-on between two instants.
 *
 * Clients should call `promiseInit()` before using the methods of this object.
 *
 * @param {PerformanceDiff} The underlying performance data.
 * @param {"addons"|"webpages"} The kind of delta represented by this object.
 * @param {Map<groupId, timestamp>} ageMap A map containing the oldest known
 *  appearance of each groupId, used to determine how long we have been monitoring
 *  this item.
 * @param {Map<Delta key, Array>} alertMap A map containing the alerts that each
 *  item has already triggered in the past.
 */
function Delta(diff, kind, snapshotDate, ageMap, alertMap) {
  if (kind != "addons" && kind != "webpages") {
    throw new TypeError(`Unknown kind: ${kind}`);
  }

  /**
   * Either "addons" or "webpages".
   */
  this.kind = kind;

  /**
   * The underlying PerformanceDiff.
   * @type {PerformanceDiff}
   */
  this.diff = diff;

  /**
   * A key unique to the item (webpage or add-on), shared by successive
   * instances of `Delta`.
   * @type{string}
   */
  this.key = kind + diff.key;

  // Find the oldest occurrence of this item.
  let creationDate = snapshotDate;
  for (let groupId of diff.groupIds) {
    let date = ageMap.get(groupId);
    if (date && date <= creationDate) {
      creationDate = date;
    }
  }

  /**
   * The timestamp at which the data was measured.
   */
  this.creationDate = creationDate;

  /**
   * Number of milliseconds since the start of the measure.
   */
  this.age = snapshotDate - creationDate;

  /**
   * A UX-friendly, human-readable name for this item.
   */
  this.readableName = null;

  /**
   * A complete name, possibly useful for power users or debugging.
   */
  this.fullName = null;


  // `true` once initialization is complete.
  this._initialized = false;
  // `true` if this item should be displayed
  this._show = false;

  /**
   * All the alerts that this item has caused since about:performance
   * was opened.
   */
  this.alerts = (alertMap.get(this.key) || []).slice();
  switch (this.slowness) {
    case 0: break;
    case 1: this.alerts[0] = (this.alerts[0] || 0) + 1; break;
    case 2: this.alerts[1] = (this.alerts[1] || 0) + 1; break;
    default: throw new Error();
  }
}
Delta.prototype = {
  /**
   * `true` if this item should be displayed, `false` otherwise.
   */
  get show() {
    this._ensureInitialized();
    return this._show;
  },

  /**
   * Estimate the slowness of this item.
   *
   * @return 0 if the item has good performance.
   * @return 1 if the item has average performance.
   * @return 2 if the item has poor performance.
   */
  get slowness() {
    if (Delta.compare(this, Delta.MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE) <= 0) {
      return 0;
    }
    if (Delta.compare(this, Delta.MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE) <= 0) {
      return 1;
    }
    return 2;
  },
  _ensureInitialized() {
    if (!this._initialized) {
      throw new Error();
    }
  },

  /**
   * Initialize, asynchronously.
   */
  promiseInit: function() {
    if (this.kind == "webpages") {
      return this._initWebpage();
    } else if (this.kind == "addons") {
      return this._promiseInitAddon();
    }
    throw new TypeError();
  },
  _initWebpage: function() {
    this._initialized = true;
    let found = tabFinder.getAny(this.diff.windowIds);
    if (!found || found.tab.linkedBrowser.contentTitle == null) {
      // Either this is not a real page or the page isn't restored yet.
      return;
    }

    this.readableName = found.tab.linkedBrowser.contentTitle;
    this.fullName = this.diff.names.join(", ");
    this._show = true;
  },
  _promiseInitAddon: Task.async(function*() {
    let found = yield (new Promise(resolve =>
      AddonManager.getAddonByID(this.diff.addonId, a => {
        if (a) {
          this.readableName = a.name;
          resolve(true);
        } else {
          resolve(false);
        }
      })));

    this._initialized = true;

    // If the add-on manager doesn't know about an add-on, it's
    // probably not a real add-on.
    this._show = found;
    this.fullName = this.diff.addonId;
  }),
  toString: function() {
    return `[Delta] ${this.diff.key} => ${this.readableName}, ${this.fullName}`;
  }
};

Delta.compare = function(a, b) {
  return (
    (a.diff.jank.longestDuration - b.diff.jank.longestDuration) ||
    (a.diff.jank.totalUserTime - b.diff.jank.totalUserTime) ||
    (a.diff.jank.totalSystemTime - b.diff.jank.totalSystemTime) ||
    (a.diff.cpow.totalCPOWTime - b.diff.cpow.totalCPOWTime) ||
    (a.diff.ticks.ticks - b.diff.ticks.ticks) ||
    0
  );
};

Delta.revCompare = function(a, b) {
  return -Delta.compare(a, b);
};

/**
 * The highest value considered "good performance".
 */
Delta.MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE = {
  diff: {
    cpow: {
      totalCPOWTime: 0,
    },
    jank: {
      longestDuration: 3,
      totalUserTime: Number.POSITIVE_INFINITY,
      totalSystemTime: Number.POSITIVE_INFINITY
    },
    ticks: {
      ticks: Number.POSITIVE_INFINITY,
    }
  }
};

/**
 * The highest value considered "average performance".
 */
Delta.MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE = {
  diff: {
    cpow: {
      totalCPOWTime: Number.POSITIVE_INFINITY,
    },
    jank: {
      longestDuration: 7,
      totalUserTime: Number.POSITIVE_INFINITY,
      totalSystemTime: Number.POSITIVE_INFINITY
    },
    ticks: {
      ticks: Number.POSITIVE_INFINITY,
    }
  }
};

/**
 * Utilities for dealing with state
 */
var State = {
  _monitor: PerformanceStats.getMonitor([
    "jank", "cpow", "ticks",
  ]),

  /**
   * Indexed by the number of minutes since the snapshot was taken.
   *
   * @type {Array<ApplicationSnapshot>}
   */
  _buffer: [],
  /**
   * The first snapshot since opening the page.
   *
   * @type ApplicationSnapshot
   */
  _oldest: null,

  /**
   * The latest snapshot.
   *
   * @type ApplicationSnapshot
   */
  _latest: null,

  /**
   * The performance alerts for each group.
   *
   * This map is cleaned up during each update to avoid leaking references
   * to groups that have been gc-ed.
   *
   * @type{Map<Delta key, Array<number>} A map in which the keys are provided
   * by property `key` of instances of `Delta` and the values are arrays
   * [number of moderate-impact alerts, number of high-impact alerts]
   */
  _alerts: new Map(),

  /**
   * The date at which each group was first seen.
   *
   * This map is cleaned up during each update to avoid leaking references
   * to groups that have been gc-ed.
   *
   * @type{Map<string, timestamp} A map in which keys are
   * values for `delta.groupId` and values are approximate
   * dates at which the group was first encountered, as provided
   * by `Cu.now()``.
   */
  _firstSeen: new Map(),

  /**
   * Update the internal state.
   *
   * @return {Promise}
   */
  update: Task.async(function*() {
    // If the buffer is empty, add one value for bootstraping purposes.
    if (this._buffer.length == 0) {
      if (this._oldest) {
        throw new Error("Internal Error, we shouldn't have a `_oldest` value yet.");
      }
      this._latest = this._oldest = yield this._monitor.promiseSnapshot();
      this._buffer.push(this._oldest);
      yield wait(BUFFER_SAMPLING_RATE_MS * 1.1);
    }


    let now = Cu.now();

    // If we haven't sampled in a while, add a sample to the buffer.
    let latestInBuffer = this._buffer[this._buffer.length - 1];
    let deltaT = now - latestInBuffer.date;
    if (deltaT > BUFFER_SAMPLING_RATE_MS) {
      this._latest = yield this._monitor.promiseSnapshot();
      this._buffer.push(this._latest);
    }

    // If we have too many samples, remove the oldest sample.
    let oldestInBuffer = this._buffer[0];
    if (oldestInBuffer.date + BUFFER_DURATION_MS < this._latest.date) {
      this._buffer.shift();
    }
  }),

  /**
   * @return {Promise}
   */
  promiseDeltaSinceStartOfTime: function() {
    return this._promiseDeltaSince(this._oldest);
  },

  /**
   * @return {Promise}
   */
  promiseDeltaSinceStartOfBuffer: function() {
    return this._promiseDeltaSince(this._buffer[0]);
  },

  /**
   * @return {Promise}
   * @resolve {{
   *  addons: Array<Delta>,
   *  webpages: Array<Delta>,
   *  deltas: Set<Delta key>,
   *  duration: number of milliseconds
   * }}
   */
  _promiseDeltaSince: Task.async(function*(oldest) {
    let current = this._latest;
    if (!oldest) {
      throw new TypeError();
    }
    if (!current) {
      throw new TypeError();
    }

    tabFinder.update();
    // We rebuild the maps during each iteration to make sure that
    // we do not maintain references to groups that has been removed
    // (e.g. pages that have been closed).
    let oldFirstSeen = this._firstSeen;
    let cleanedUpFirstSeen = new Map();

    let oldAlerts = this._alerts;
    let cleanedUpAlerts = new Map();

    let result = {
      addons: [],
      webpages: [],
      deltas: new Set(),
      duration: current.date - oldest.date
    };

    for (let kind of ["webpages", "addons"]) {
      for (let [key, value] of current[kind]) {
        let item = ObjectUtils.strict(new Delta(value.subtract(oldest[kind].get(key)), kind, current.date, oldFirstSeen, oldAlerts));
        yield item.promiseInit();

        if (!item.show) {
          continue;
        }
        result[kind].push(item);
        result.deltas.add(item.key);

        for (let groupId of item.diff.groupIds) {
          cleanedUpFirstSeen.set(groupId, item.creationDate);
        }
        cleanedUpAlerts.set(item.key, item.alerts);
      }
    }

    this._firstSeen = cleanedUpFirstSeen;
    this._alerts = cleanedUpAlerts;
    return result;
  }),
};

var View = {
  /**
   * A cache for all the per-item DOM elements that are reused across refreshes.
   *
   * Reusing the same elements means that elements that were hidden (respectively
   * visible) in an iteration remain hidden (resp visible) in the next iteration.
   */
  DOMCache: {
    _map: new Map(),
    /**
     * @param {string} deltaKey The key for the item that we are displaying.
     * @return {null} If the `deltaKey` doesn't have a component cached yet.
     * Otherwise, the value stored with `set`.
     */
    get: function(deltaKey) {
      return this._map.get(deltaKey);
    },
    set: function(deltaKey, value) {
      this._map.set(deltaKey, value);
    },
    /**
     * Remove all the elements whose key does not appear in `set`.
     *
     * @param {Set} set a set of deltaKey.
     */
    trimTo: function(set) {
      let remove = [];
      for (let key of this._map.keys()) {
        if (!set.has(key)) {
          remove.push(key);
        }
      }
      for (let key of remove) {
        this._map.delete(key);
      }
    }
  },
  /**
   * Display the items in a category.
   *
   * @param {Array<PerformanceDiff>} subset The items to display. They will
   * be displayed in the order of `subset`.
   * @param {string} id The id of the DOM element that will contain the items.
   * @param {string} nature The nature of the subset. One of "addons", "webpages" or "system".
   * @param {string} currentMode The current display mode. One of MODE_GLOBAL or MODE_RECENT.
   */
  updateCategory: function(subset, id, nature, currentMode) {
    subset = subset.slice().sort(Delta.revCompare);

    let watcherAlerts = null;
    if (nature == "addons") {
      watcherAlerts = AddonWatcher.alerts;
    }

    // Grab everything from the DOM before cleaning up
    let eltContainer = this._setupStructure(id);

    // An array of `cachedElements` that need to be added
    let toAdd = [];
    for (let delta of subset) {
      if (!(delta instanceof Delta)) {
        throw new TypeError();
      }
      let cachedElements = this._grabOrCreateElements(delta, nature);
      toAdd.push(cachedElements);
      cachedElements.eltTitle.textContent = delta.readableName;
      cachedElements.eltName.textContent = `Full name: ${delta.fullName}.`;
      cachedElements.eltLoaded.textContent = `Measure start: ${Math.round(delta.age/1000)} seconds ago.`

      let processes = delta.diff.processes.map(proc => `${proc.processId} (${proc.isChildProcess?"child":"parent"})`);
      cachedElements.eltProcess.textContent = `Processes: ${processes.join(", ")}`;
      let jankSuffix = "";
      if (watcherAlerts) {
        let deltaAlerts = watcherAlerts.get(delta.diff.addonId);
        if (deltaAlerts) {
          if (deltaAlerts.occurrences) {
            jankSuffix = ` (${deltaAlerts.occurrences} alerts)`;
          }
        }
      }

      let eltImpact = cachedElements.eltImpact;
      if (currentMode == MODE_RECENT) {
        cachedElements.eltRoot.setAttribute("impact", delta.diff.jank.longestDuration + 1);
        if (Delta.compare(delta, Delta.MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE) <= 0) {
          eltImpact.textContent = ` currently performs well.`;
        } else if (Delta.compare(delta, Delta.MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE)) {
          eltImpact.textContent = ` may currently be slowing down ${BRAND_NAME}.`;
        } else {
          eltImpact.textContent = ` is currently considerably slowing down ${BRAND_NAME}.`;
        }

        cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.diff.jank.longestDuration + 1}/${delta.diff.jank.durations.length}${jankSuffix}.`;
        cachedElements.eltCPU.textContent = `CPU usage: ${Math.ceil(delta.diff.jank.totalCPUTime/delta.diff.deltaT/10)}%.`;
        cachedElements.eltSystem.textContent = `System usage: ${Math.ceil(delta.diff.jank.totalSystemTime/delta.diff.deltaT/10)}%.`;
        cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.diff.cpow.totalCPOWTime/delta.diff.deltaT/10)}%.`;
      } else {
        if (delta.alerts.length == 0) {
          eltImpact.textContent = " has performed well so far.";
          cachedElements.eltFPS.textContent = `Impact on framerate: no impact.`;
          cachedElements.eltRoot.setAttribute("impact", 0);
        } else {
          let impact = 0;
          let sum = /* medium impact */ delta.alerts[0] + /* high impact */ delta.alerts[1];
          let frequency = sum * 1000 / delta.diff.deltaT;

          let describeFrequency;
          if (frequency <= MAX_FREQUENCY_FOR_NO_IMPACT) {
            describeFrequency = `has no impact on the performance of ${BRAND_NAME}.`
          } else {
            let describeImpact;
            if (frequency <= MAX_FREQUENCY_FOR_RARE) {
              describeFrequency = `rarely slows down ${BRAND_NAME}.`;
              impact += 1;
            } else if (frequency <= MAX_FREQUENCY_FOR_FREQUENT) {
              describeFrequency = `has slown down ${BRAND_NAME} frequently.`;
              impact += 2.5;
            } else {
              describeFrequency = `seems to have slown down ${BRAND_NAME} very often.`;
              impact += 5;
            }
            // At this stage, `sum != 0`
            if (delta.alerts[1] / sum > MIN_PROPORTION_FOR_MAJOR_IMPACT) {
              describeImpact = "When this happens, the slowdown is generally important."
              impact *= 2;
            } else {
              describeImpact = "When this happens, the slowdown is generally noticeable."
            }

            eltImpact.textContent = ` ${describeFrequency} ${describeImpact}`;
            cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.alerts[1] || 0} high-impacts, ${delta.alerts[0] || 0} medium-impact${jankSuffix}.`;
          }
          cachedElements.eltRoot.setAttribute("impact", Math.round(impact));
        }

        let result = delta.diff.jank.totalCPUTime/delta.diff.deltaT;
        cachedElements.eltCPU.textContent = `CPU usage: ${Math.ceil(delta.diff.jank.totalCPUTime/delta.diff.deltaT/10)}% (total ${delta.diff.jank.totalUserTime}ms).`;
        cachedElements.eltSystem.textContent = `System usage: ${Math.ceil(delta.diff.jank.totalSystemTime/delta.diff.deltaT/10)}% (total ${delta.diff.jank.totalSystemTime}ms).`;
        cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.diff.cpow.totalCPOWTime/delta.diff.deltaT/10)}% (total ${delta.diff.cpow.totalCPOWTime}ms).`;
      }
    }
    this._insertElements(toAdd, id);
  },

  _insertElements: function(elements, id) {
    let eltContainer = document.getElementById(id);
    eltContainer.classList.remove("measuring");
    eltContainer.eltVisibleContent.innerHTML = "";
    eltContainer.eltHiddenContent.innerHTML = "";
    eltContainer.appendChild(eltContainer.eltShowMore);

    for (let i = 0; i < elements.length && i < MAX_NUMBER_OF_ITEMS_TO_DISPLAY; ++i) {
      let cachedElements = elements[i];
      eltContainer.eltVisibleContent.appendChild(cachedElements.eltRoot);
    }
    for (let i = MAX_NUMBER_OF_ITEMS_TO_DISPLAY; i < elements.length; ++i) {
      let cachedElements = elements[i];
      eltContainer.eltHiddenContent.appendChild(cachedElements.eltRoot);
    }
    if (elements.length <= MAX_NUMBER_OF_ITEMS_TO_DISPLAY) {
      eltContainer.eltShowMore.classList.add("hidden");
    } else {
      eltContainer.eltShowMore.classList.remove("hidden");
    }
    if (elements.length == 0) {
      eltContainer.textContent = "Nothing";
    }
  },
  _setupStructure: function(id) {
    let eltContainer = document.getElementById(id);
    if (!eltContainer.eltVisibleContent) {
      eltContainer.eltVisibleContent = document.createElement("ul");
      eltContainer.eltVisibleContent.classList.add("visible_items");
      eltContainer.appendChild(eltContainer.eltVisibleContent);
    }
    if (!eltContainer.eltHiddenContent) {
      eltContainer.eltHiddenContent = document.createElement("ul");
      eltContainer.eltHiddenContent.classList.add("hidden");
      eltContainer.eltHiddenContent.classList.add("hidden_additional_items");
      eltContainer.appendChild(eltContainer.eltHiddenContent);
    }
    if (!eltContainer.eltShowMore) {
      eltContainer.eltShowMore = document.createElement("button");
      eltContainer.eltShowMore.textContent = "Show all";
      eltContainer.eltShowMore.classList.add("show_all_items");
      eltContainer.appendChild(eltContainer.eltShowMore);
      eltContainer.eltShowMore.addEventListener("click", function() {
        if (eltContainer.eltHiddenContent.classList.contains("hidden")) {
          eltContainer.eltHiddenContent.classList.remove("hidden");
          eltContainer.eltShowMore.textContent = "Hide";
        } else {
          eltContainer.eltHiddenContent.classList.add("hidden");
          eltContainer.eltShowMore.textContent = "Show all";
        }
      });
    }
    return eltContainer;
  },

  _grabOrCreateElements: function(delta, nature) {
    let cachedElements = this.DOMCache.get(delta.key);
    if (cachedElements) {
      if (cachedElements.eltRoot.parentElement) {
        cachedElements.eltRoot.parentElement.removeChild(cachedElements.eltRoot);
      }
    } else {
      this.DOMCache.set(delta.key, cachedElements = {});

      let eltDelta = document.createElement("li");
      eltDelta.classList.add("delta");
      cachedElements.eltRoot = eltDelta;

      let eltSpan = document.createElement("span");
      eltDelta.appendChild(eltSpan);

      let eltSummary = document.createElement("span");
      eltSummary.classList.add("summary");
      eltSpan.appendChild(eltSummary);

      let eltTitle = document.createElement("span");
      eltTitle.classList.add("title");
      eltSummary.appendChild(eltTitle);
      cachedElements.eltTitle = eltTitle;

      let eltImpact = document.createElement("span");
      eltImpact.classList.add("impact");
      eltSummary.appendChild(eltImpact);
      cachedElements.eltImpact = eltImpact;

      let eltShowMore = document.createElement("a");
      eltShowMore.classList.add("more");
      eltSpan.appendChild(eltShowMore);
      eltShowMore.textContent = "more";
      eltShowMore.href = "";
      eltShowMore.addEventListener("click", () => {
        if (eltDetails.classList.contains("hidden")) {
          eltDetails.classList.remove("hidden");
          eltShowMore.textContent = "less";
        } else {
          eltDetails.classList.add("hidden");
          eltShowMore.textContent = "more";
        }
      });

      // Add buttons
      if (nature == "addons") {
        eltSpan.appendChild(document.createElement("br"));
        let eltDisable = document.createElement("button");
        eltDisable.textContent = "Disable";
        eltSpan.appendChild(eltDisable);

        let eltUninstall = document.createElement("button");
        eltUninstall.textContent = "Uninstall";
        eltSpan.appendChild(eltUninstall);

        let eltRestart = document.createElement("button");
        eltRestart.textContent = `Restart ${BRAND_NAME} to apply your changes.`
        eltRestart.classList.add("hidden");
        eltSpan.appendChild(eltRestart);

        eltRestart.addEventListener("click", () => {
          Services.startup.quit(Services.startup.eForceQuit | Services.startup.eRestart);
        });
        AddonManager.getAddonByID(delta.diff.addonId, addon => {
          eltDisable.addEventListener("click", () => {
            addon.userDisabled = true;
            if (addon.pendingOperations == addon.PENDING_NONE) {
              // Restartless add-on is now disabled.
              return;
            }
            eltDisable.classList.add("hidden");
            eltUninstall.classList.add("hidden");
            eltRestart.classList.remove("hidden");
          });

          eltUninstall.addEventListener("click", () => {
            addon.uninstall();
            if (addon.pendingOperations == addon.PENDING_NONE) {
              // Restartless add-on is now disabled.
              return;
            }
            eltDisable.classList.add("hidden");
            eltUninstall.classList.add("hidden");
            eltRestart.classList.remove("hidden");
          });
        });
      } else if (nature == "webpages") {
        eltSpan.appendChild(document.createElement("br"));

        let eltCloseTab = document.createElement("button");
        eltCloseTab.textContent = "Close tab";
        eltSpan.appendChild(eltCloseTab);
        let windowIds = delta.diff.windowIds;
        eltCloseTab.addEventListener("click", () => {
          let found = tabFinder.getAny(windowIds);
          if (!found) {
            // Cannot find the tab. Maybe it is closed already?
            return;
          }
          let {tabbrowser, tab} = found;
          tabbrowser.removeTab(tab);
        });

        let eltReloadTab = document.createElement("button");
        eltReloadTab.textContent = "Reload tab";
        eltSpan.appendChild(eltReloadTab);
        eltReloadTab.addEventListener("click", () => {
          let found = tabFinder.getAny(windowIds);
          if (!found) {
            // Cannot find the tab. Maybe it is closed already?
            return;
          }
          let {tabbrowser, tab} = found;
          tabbrowser.reloadTab(tab);
        });
      }

      // Prepare details
      let eltDetails = document.createElement("ul");
      eltDetails.classList.add("details");
      eltDetails.classList.add("hidden");
      eltSpan.appendChild(eltDetails);

      for (let [name, className] of [
        ["eltName", "name"],
        ["eltFPS", "fps"],
        ["eltCPU", "cpu"],
        ["eltSystem", "system"],
        ["eltCPOW", "cpow"],
        ["eltLoaded", "loaded"],
        ["eltProcess", "process"],
      ]) {
        let elt = document.createElement("li");
        elt.classList.add(className);
        eltDetails.appendChild(elt);
        cachedElements[name] = elt;
      }
    }

    return cachedElements;
  },
};

var Control = {
  init: function() {
    this._initAutorefresh();
    this._initDisplayMode();
  },
  update: Task.async(function*() {
    let mode = this._displayMode;
    if (this._autoRefreshInterval || !State._buffer[0]) {
      // Update the state only if we are not on pause.
      yield State.update();
    }
    yield wait(0);
    let state = yield (mode == MODE_GLOBAL?
      State.promiseDeltaSinceStartOfTime():
      State.promiseDeltaSinceStartOfBuffer());

    for (let category of ["webpages", "addons"]) {
      yield wait(0);
      yield View.updateCategory(state[category], category, category, mode);
    }
    yield wait(0);

    // Make sure that we do not keep obsolete stuff around.
    View.DOMCache.trimTo(state.deltas);

    yield wait(0);

    // Inform watchers
    Services.obs.notifyObservers(null, UPDATE_COMPLETE_TOPIC, mode);
  }),
  _setOptions: function(options) {
    dump(`about:performance _setOptions ${JSON.stringify(options)}\n`);
    let eltRefresh = document.getElementById("check-autorefresh");
    if ((options.autoRefresh > 0) != eltRefresh.checked) {
      eltRefresh.click();
    }
    let eltCheckRecent = document.getElementById("check-display-recent");
    if (!!options.displayRecent != eltCheckRecent.checked) {
      eltCheckRecent.click();
    }
  },
  _initAutorefresh: function() {
    let onRefreshChange = (shouldUpdateNow = false) => {
      if (eltRefresh.checked == !!this._autoRefreshInterval) {
        // Nothing to change.
        return;
      }
      if (eltRefresh.checked) {
        this._autoRefreshInterval = window.setInterval(() => Control.update(), UPDATE_INTERVAL_MS);
        if (shouldUpdateNow) {
          Control.update();
        }
      } else {
        window.clearInterval(this._autoRefreshInterval);
        this._autoRefreshInterval = null;
      }
    }

    let eltRefresh = document.getElementById("check-autorefresh");
    eltRefresh.addEventListener("change", () => onRefreshChange(true));

    onRefreshChange(false);
  },
  _autoRefreshInterval: null,
  _initDisplayMode: function() {
    let onModeChange = (shouldUpdateNow) => {
      if (eltCheckRecent.checked) {
        this._displayMode = MODE_RECENT;
      } else {
        this._displayMode = MODE_GLOBAL;
      }
      if (shouldUpdateNow) {
        Control.update();
      }
    };

    let eltCheckRecent = document.getElementById("check-display-recent");
    let eltLabelRecent = document.getElementById("label-display-recent");
    eltCheckRecent.addEventListener("click", () => onModeChange(true));
    eltLabelRecent.textContent = `Display only the latest ${Math.round(BUFFER_DURATION_MS/1000)}s`;

    onModeChange(false);
  },
  // The display mode. One of `MODE_GLOBAL` or `MODE_RECENT`.
  _displayMode: MODE_GLOBAL,
};

var go = Task.async(function*() {
  Control.init();

  // Setup a hook to allow tests to configure and control this page
  let testUpdate = function(subject, topic, value) {
    let options = JSON.parse(value);
    Control._setOptions(options);
    Control.update();
  };
  Services.obs.addObserver(testUpdate, TEST_DRIVER_TOPIC, false);
  window.addEventListener("unload", () => Services.obs.removeObserver(testUpdate, TEST_DRIVER_TOPIC));

  yield Control.update();
  yield wait(BUFFER_SAMPLING_RATE_MS * 1.1);
  yield Control.update();
});
