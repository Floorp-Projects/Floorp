/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PerformanceStats } = ChromeUtils.import("resource://gre/modules/PerformanceStats.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { ObjectUtils } = ChromeUtils.import("resource://gre/modules/ObjectUtils.jsm", {});
const { ExtensionParent } = ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm", {});

const {WebExtensionPolicy} = Cu.getGlobalForObject(Services);

// Time in ms before we start changing the sort order again after receiving a
// mousemove event.
const TIME_BEFORE_SORTING_AGAIN = 5000;

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
const UPDATE_INTERVAL_MS = 2000;

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

// Decide if we show the old style about:performance or if we can show data
// based on the new performance counters.
function performanceCountersEnabled() {
  return Services.prefs.getBoolPref("dom.performance.enable_scheduler_timing", false);
}

function extensionCountersEnabled() {
  return Services.prefs.getBoolPref("extensions.webextensions.enablePerformanceCounters", false);
}

let tabFinder = {
  update() {
    this._map = new Map();
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      let tabbrowser = win.gBrowser;
      for (let browser of tabbrowser.browsers) {
        let id = browser.outerWindowID; // May be `null` if the browser isn't loaded yet
        if (id != null) {
          this._map.set(id, browser);
        }
      }
      if (tabbrowser._preloadedBrowser) {
        let browser = tabbrowser._preloadedBrowser;
        if (browser.outerWindowID)
          this._map.set(browser.outerWindowID, browser);
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
  get(id) {
    let browser = this._map.get(id);
    if (!browser) {
      return null;
    }
    let tabbrowser = browser.getTabBrowser();
    if (!tabbrowser)
      return {tabbrowser: null,
              tab: {getAttribute() { return ""; },
                    linkedBrowser: browser}};
    return {tabbrowser, tab: tabbrowser.getTabForBrowser(browser)};
  },

  getAny(ids) {
    for (let id of ids) {
      let result = this.get(id);
      if (result) {
        return result;
      }
    }
    return null;
  },
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
    let p = new Promise(resolve_ => { resolve = resolve_; });
    setTimeout(resolve, ms);
    return p;
  } catch (e) {
    dump("WARNING: wait aborted because of an invalid Window state in aboutPerformance.js.\n");
    return undefined;
  }
}

/**
 * The performance of a webpage between two instants.
 *
 * Clients should call `promiseInit()` before using the methods of this object.
 *
 * @param {PerformanceDiff} The underlying performance data.
 * @param {"webpages"} The kind of delta represented by this object.
 * @param {Map<groupId, timestamp>} ageMap A map containing the oldest known
 *  appearance of each groupId, used to determine how long we have been monitoring
 *  this item.
 * @param {Map<Delta key, Array>} alertMap A map containing the alerts that each
 *  item has already triggered in the past.
 */
function Delta(diff, kind, snapshotDate, ageMap, alertMap) {
  if (kind != "webpages") {
    throw new TypeError(`Unknown kind: ${kind}`);
  }

  /**
   * We only understand "webpages" right now.
   */
  this.kind = kind;

  /**
   * The underlying PerformanceDiff.
   * @type {PerformanceDiff}
   */
  this.diff = diff;

  /**
   * A key unique to the item (webpage), shared by successive
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
  promiseInit() {
    if (this.kind == "webpages") {
      return this._initWebpage();
    }
    throw new TypeError();
  },
  _initWebpage() {
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
  toString() {
    return `[Delta] ${this.diff.key} => ${this.readableName}, ${this.fullName}`;
  },
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
      totalSystemTime: Number.POSITIVE_INFINITY,
    },
    ticks: {
      ticks: Number.POSITIVE_INFINITY,
    },
  },
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
      totalSystemTime: Number.POSITIVE_INFINITY,
    },
    ticks: {
      ticks: Number.POSITIVE_INFINITY,
    },
  },
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

  async _promiseSnapshot() {
    if (!performanceCountersEnabled()) {
      return this._monitor.promiseSnapshot();
    }

    let addons = WebExtensionPolicy.getActiveExtensions();
    let addonHosts = new Map();
    for (let addon of addons)
      addonHosts.set(addon.mozExtensionHostname, addon.id);

    let counters = await ChromeUtils.requestPerformanceMetrics();
    let tabs = {};
    for (let counter of counters) {
      let {items, host, pid, counterId, windowId, duration, isWorker,
           isTopLevel} = counter;
      // If a worker has a windowId of 0 or max uint64, attach it to the
      // browser UI (doc group with id 1).
      if (isWorker && (windowId == 18446744073709552000 || !windowId))
        windowId = 1;
      let dispatchCount = 0;
      for (let {count} of items) {
        dispatchCount += count;
      }

      let tab;
      let id = windowId;
      if (addonHosts.has(host)) {
        id = addonHosts.get(host);
      }
      if (id in tabs) {
        tab = tabs[id];
      } else {
        tab = {windowId, host, dispatchCount: 0, duration: 0, children: []};
        tabs[id] = tab;
      }
      tab.dispatchCount += dispatchCount;
      tab.duration += duration;
      if (!isTopLevel || isWorker) {
        tab.children.push({host, isWorker, dispatchCount, duration,
                           counterId: pid + ":" + counterId});
      }
    }

    if (extensionCountersEnabled()) {
      let extCounters = await ExtensionParent.ParentAPIManager.retrievePerformanceCounters();
      for (let [id, apiMap] of extCounters) {
        let dispatchCount = 0, duration = 0;
        for (let [, counter] of apiMap) {
          dispatchCount += counter.calls;
          duration += counter.duration;
        }

        let tab;
        if (id in tabs) {
          tab = tabs[id];
        } else {
          tab = {windowId: 0, host: id, dispatchCount: 0, duration: 0, children: []};
          tabs[id] = tab;
        }
        tab.dispatchCount += dispatchCount;
        tab.duration += duration;
      }
    }

    return {tabs, date: Cu.now()};
  },

  /**
   * Update the internal state.
   *
   * @return {Promise}
   */
  async update() {
    // If the buffer is empty, add one value for bootstraping purposes.
    if (this._buffer.length == 0) {
      if (this._oldest) {
        throw new Error("Internal Error, we shouldn't have a `_oldest` value yet.");
      }
      this._latest = this._oldest = await this._promiseSnapshot();
      this._buffer.push(this._oldest);
      await wait(BUFFER_SAMPLING_RATE_MS * 1.1);
    }


    let now = Cu.now();

    // If we haven't sampled in a while, add a sample to the buffer.
    let latestInBuffer = this._buffer[this._buffer.length - 1];
    let deltaT = now - latestInBuffer.date;
    if (deltaT > BUFFER_SAMPLING_RATE_MS) {
      this._latest = await this._promiseSnapshot();
      this._buffer.push(this._latest);
    }

    // If we have too many samples, remove the oldest sample.
    let oldestInBuffer = this._buffer[0];
    if (oldestInBuffer.date + BUFFER_DURATION_MS < this._latest.date) {
      this._buffer.shift();
    }
  },

  /**
   * @return {Promise}
   */
  promiseDeltaSinceStartOfTime() {
    return this._promiseDeltaSince(this._oldest);
  },

  /**
   * @return {Promise}
   */
  promiseDeltaSinceStartOfBuffer() {
    return this._promiseDeltaSince(this._buffer[0]);
  },

  /**
   * @return {Promise}
   * @resolve {{
   *  webpages: Array<Delta>,
   *  deltas: Set<Delta key>,
   *  duration: number of milliseconds
   * }}
   */
  async _promiseDeltaSince(oldest) {
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
      webpages: [],
      deltas: new Set(),
      duration: current.date - oldest.date,
    };

    for (let kind of ["webpages"]) {
      for (let [key, value] of current[kind]) {
        let item = ObjectUtils.strict(new Delta(value.subtract(oldest[kind].get(key)), kind, current.date, oldFirstSeen, oldAlerts));
        await item.promiseInit();

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
  },

  // We can only know asynchronously if an origin is matched by the tracking
  // protection list, so we cache the result for faster future lookups.
  _trackingState: new Map(),
  isTracker(host) {
    if (!this._trackingState.has(host)) {
      // Temporarily set to false to avoid doing several lookups if a site has
      // several subframes on the same domain.
      this._trackingState.set(host, false);
      if (host.startsWith("about:") || host.startsWith("moz-nullprincipal"))
        return false;

      let principal =
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("http://" + host);
      let classifier =
        Cc["@mozilla.org/url-classifier/dbservice;1"].getService(Ci.nsIURIClassifier);
      classifier.classify(principal, null, true,
                          (aErrorCode, aList, aProvider, aFullHash) => {
        this._trackingState.set(host, aErrorCode == Cr.NS_ERROR_TRACKING_URI);
      });
    }
    return this._trackingState.get(host);
  },

  getCounters() {
    tabFinder.update();
    // We rebuild the maps during each iteration to make sure that
    // we do not maintain references to groups that has been removed
    // (e.g. pages that have been closed).

    let previous = this._buffer[Math.max(this._buffer.length - 2, 0)].tabs;
    let current = this._latest.tabs;
    return Object.keys(current).map(id => {
      let tab = current[id];
      let oldest;
      for (let index = 0; index <= this._buffer.length - 2; ++index) {
        if (id in this._buffer[index].tabs) {
          oldest = this._buffer[index].tabs[id];
          break;
        }
      }
      let prev = previous[id];
      let host = tab.host;

      let type = "tab";
      let name = `${host} (${id})`;
      let image = "chrome://mozapps/skin/places/defaultFavicon.svg";
      let found = tabFinder.get(parseInt(id));
      if (found) {
        if (found.tabbrowser) {
          name = found.tab.getAttribute("label");
          image = found.tab.getAttribute("image");
        } else {
          name = {id: "preloaded-tab",
                  title: found.tab.linkedBrowser.contentTitle};
          type = "other";
        }
      } else if (id == 1) {
        name = BRAND_NAME;
        image = "chrome://branding/content/icon32.png";
        type = "browser";
      } else if (/^[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12}$/.test(host)) {
        let addon = WebExtensionPolicy.getByHostname(host);
        name = `${addon.name} (${addon.id})`;
        image = "chrome://mozapps/skin/extensions/extensionGeneric-16.svg";
        type = "addon";
      } else if (id == 0 && !tab.isWorker) {
        name = {id: "ghost-windows"};
        type = "other";
      }

      // Create a map of all the child items from the previous time we read the
      // counters, indexed by counterId so that we can quickly find the previous
      // value for any subitem.
      let prevChildren = new Map();
      if (prev) {
        for (let child of prev.children) {
          prevChildren.set(child.counterId, child);
        }
      }
      // For each subitem, create a new object including the deltas since the previous time.
      let children = tab.children.map(child => {
        let {host, dispatchCount, duration, isWorker, counterId} = child;

        let dispatchesSincePrevious = dispatchCount;
        let durationSincePrevious = duration;
        if (prevChildren.has(counterId)) {
          let prevCounter = prevChildren.get(counterId);
          dispatchesSincePrevious -= prevCounter.dispatchCount;
          durationSincePrevious -= prevCounter.duration;
          prevChildren.delete(counterId);
        }

        return {host, dispatchCount, duration, isWorker,
                dispatchesSincePrevious, durationSincePrevious};
      });

      // Any item that remains in prevChildren is a subitem that no longer
      // exists in the current sample; remember the values of its counters
      // so that the values don't go down for the parent item.
      tab.dispatchesFromFormerChildren = prev && prev.dispatchesFromFormerChildren || 0;
      tab.durationFromFormerChildren = prev && prev.durationFromFormerChildren || 0;
      for (let [, counter] of prevChildren) {
        tab.dispatchesFromFormerChildren += counter.dispatchCount;
        tab.durationFromFormerChildren += counter.duration;
      }

      // Create the object representing the counters of the parent item including
      // the deltas from the previous times.
      let dispatches = tab.dispatchCount + tab.dispatchesFromFormerChildren;
      let duration = tab.duration + tab.durationFromFormerChildren;
      let durationSincePrevious = NaN;
      let dispatchesSincePrevious = NaN;
      let dispatchesSinceStartOfBuffer = NaN;
      let durationSinceStartOfBuffer = NaN;
      if (prev) {
        durationSincePrevious =
          duration - prev.duration - (prev.durationFromFormerChildren || 0);
        dispatchesSincePrevious =
          dispatches - prev.dispatchCount - (prev.dispatchesFromFormerChildren || 0);
      }
      if (oldest) {
        dispatchesSinceStartOfBuffer =
          dispatches - oldest.dispatchCount - (oldest.dispatchesFromFormerChildren || 0);
        durationSinceStartOfBuffer =
          duration - oldest.duration - (oldest.durationFromFormerChildren || 0);
      }
      return ({id, name, image, type,
               totalDispatches: dispatches, totalDuration: duration,
               durationSincePrevious, dispatchesSincePrevious,
               durationSinceStartOfBuffer, dispatchesSinceStartOfBuffer,
               children});
    });
  },
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
    get(deltaKey) {
      return this._map.get(deltaKey);
    },
    set(deltaKey, value) {
      this._map.set(deltaKey, value);
    },
    /**
     * Remove all the elements whose key does not appear in `set`.
     *
     * @param {Set} set a set of deltaKey.
     */
    trimTo(set) {
      let remove = [];
      for (let key of this._map.keys()) {
        if (!set.has(key)) {
          remove.push(key);
        }
      }
      for (let key of remove) {
        this._map.delete(key);
      }
    },
  },
  /**
   * Display the items in a category.
   *
   * @param {Array<PerformanceDiff>} subset The items to display. They will
   * be displayed in the order of `subset`.
   * @param {string} id The id of the DOM element that will contain the items.
   * @param {string} nature The nature of the subset. One of "webpages" or "system".
   * @param {string} currentMode The current display mode. One of MODE_GLOBAL or MODE_RECENT.
   */
  updateCategory(subset, id, nature, currentMode) {
    subset = subset.slice().sort(Delta.revCompare);


    // Grab everything from the DOM before cleaning up
    this._setupStructure(id);

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
      cachedElements.eltLoaded.textContent = `Measure start: ${Math.round(delta.age / 1000)} seconds ago.`;

      let processes = delta.diff.processes.map(proc => `${proc.processId} (${proc.isChildProcess ? "child" : "parent"})`);
      cachedElements.eltProcess.textContent = `Processes: ${processes.join(", ")}`;

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

        cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.diff.jank.longestDuration + 1}/${delta.diff.jank.durations.length}`;
        cachedElements.eltCPU.textContent = `CPU usage: ${Math.ceil(delta.diff.jank.totalCPUTime / delta.diff.deltaT / 10)}%.`;
        cachedElements.eltSystem.textContent = `System usage: ${Math.ceil(delta.diff.jank.totalSystemTime / delta.diff.deltaT / 10)}%.`;
        cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.diff.cpow.totalCPOWTime / delta.diff.deltaT / 10)}%.`;
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
            describeFrequency = `has no impact on the performance of ${BRAND_NAME}.`;
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
              describeImpact = "When this happens, the slowdown is generally important.";
              impact *= 2;
            } else {
              describeImpact = "When this happens, the slowdown is generally noticeable.";
            }

            eltImpact.textContent = ` ${describeFrequency} ${describeImpact}`;
            cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.alerts[1] || 0} high-impacts, ${delta.alerts[0] || 0} medium-impact.`;
          }
          cachedElements.eltRoot.setAttribute("impact", Math.round(impact));
        }

        cachedElements.eltCPU.textContent = `CPU usage: ${Math.ceil(delta.diff.jank.totalCPUTime / delta.diff.deltaT / 10)}% (total ${delta.diff.jank.totalUserTime}ms).`;
        cachedElements.eltSystem.textContent = `System usage: ${Math.ceil(delta.diff.jank.totalSystemTime / delta.diff.deltaT / 10)}% (total ${delta.diff.jank.totalSystemTime}ms).`;
        cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.diff.cpow.totalCPOWTime / delta.diff.deltaT / 10)}% (total ${delta.diff.cpow.totalCPOWTime}ms).`;
      }
    }
    this._insertElements(toAdd, id);
  },

  _insertElements(elements, id) {
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
  _setupStructure(id) {
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

  _grabOrCreateElements(delta, nature) {
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
      if (nature == "webpages") {
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

  _fragment: document.createDocumentFragment(),
  commit() {
    let tbody = document.getElementById("dispatch-tbody");

    while (tbody.firstChild)
      tbody.firstChild.remove();
    tbody.appendChild(this._fragment);
    this._fragment = document.createDocumentFragment();
  },
  insertAfterRow(row) {
    row.parentNode.insertBefore(this._fragment, row.nextSibling);
    this._fragment = document.createDocumentFragment();
  },
  displayEnergyImpact(elt, energyImpact) {
    if (!energyImpact)
      elt.textContent = "–";
    else {
      let impact = "high";
      if (energyImpact < 1)
        impact = "low";
      else if (energyImpact < 25)
        impact = "medium";
      document.l10n.setAttributes(elt, "energy-impact-" + impact,
                                  {value: energyImpact});
    }
  },
  appendRow(name, energyImpact, tooltip, type, image = "") {
    let row = document.createElement("tr");

    let elt = document.createElement("td");
    if (typeof name == "string") {
      elt.textContent = name;
    } else if (name.title) {
      document.l10n.setAttributes(elt, name.id, {title: name.title});
    } else {
      document.l10n.setAttributes(elt, name.id);
    }
    if (image)
      elt.style.backgroundImage = `url('${image}')`;

    if (["subframe", "tracker", "worker"].includes(type))
      elt.classList.add("indent");
    else
      elt.classList.add("root");
    if (["tracker", "worker"].includes(type))
      elt.classList.add(type);
    row.appendChild(elt);

    elt = document.createElement("td");
    document.l10n.setAttributes(elt, "type-" + type);
    row.appendChild(elt);

    elt = document.createElement("td");
    this.displayEnergyImpact(elt, energyImpact);
    row.appendChild(elt);

    if (tooltip)
      document.l10n.setAttributes(row, "item", tooltip);

    elt = document.createElement("td");
    if (type == "tab") {
      let img = document.createElement("img");
      img.className = "action-icon close-icon";
      document.l10n.setAttributes(img, "close-tab");
      elt.appendChild(img);
    } else if (type == "addon") {
      let img = document.createElement("img");
      img.className = "action-icon addon-icon";
      document.l10n.setAttributes(img, "show-addon");
      elt.appendChild(img);
    }
    row.appendChild(elt);

    this._fragment.appendChild(row);
    return row;
  },
};

var Control = {
  _openItems: new Set(),
  init() {
    this._initAutorefresh();
    this._initDisplayMode();
    let tbody = document.getElementById("dispatch-tbody");
    tbody.addEventListener("click", event => {
      this._updateLastMouseEvent();

      // Handle showing or hiding subitems of a row.
      let target = event.target;
      if (target.classList.contains("twisty")) {
        let row = target.parentNode.parentNode;
        let id = row.windowId;
        if (target.classList.toggle("open")) {
          this._openItems.add(id);
          this._showChildren(row);
          View.insertAfterRow(row);
        } else {
          this._openItems.delete(id);
          while (row.nextSibling.firstChild.classList.contains("indent"))
            row.nextSibling.remove();
        }
        return;
      }

      // Handle closing a tab.
      if (target.classList.contains("close-icon")) {
        let row = target.parentNode.parentNode;
        let id = parseInt(row.windowId);
        let found = tabFinder.get(id);
        if (!found || !found.tabbrowser)
          return;
        let {tabbrowser, tab} = found;
        tabbrowser.removeTab(tab);
        while (row.nextSibling.firstChild.classList.contains("indent"))
          row.nextSibling.remove();
        row.remove();
        return;
      }

      if (target.classList.contains("addon-icon")) {
        let row = target.parentNode.parentNode;
        let id = row.windowId;
        let parentWin = window.docShell.rootTreeItem.domWindow;
        parentWin.BrowserOpenAddonsMgr("addons://detail/" + encodeURIComponent(id));
        return;
      }

      // Handle selection changes
      let row = target.parentNode;
      if (this.selectedRow) {
        this.selectedRow.removeAttribute("selected");
      }
      if (row.windowId) {
        row.setAttribute("selected", "true");
        this.selectedRow = row;
      } else if (this.selectedRow) {
        this.selectedRow = null;
      }
    });

    // Select the tab of double clicked items.
    tbody.addEventListener("dblclick", event => {
      let id = parseInt(event.target.parentNode.windowId);
      if (isNaN(id))
        return;
      let found = tabFinder.get(id);
      if (!found || !found.tabbrowser)
        return;
      let {tabbrowser, tab} = found;
      tabbrowser.selectedTab = tab;
      tabbrowser.ownerGlobal.focus();
    });

    tbody.addEventListener("mousemove", () => {
      this._updateLastMouseEvent();
    });
  },
  _lastMouseEvent: 0,
  _updateLastMouseEvent() {
    this._lastMouseEvent = Date.now();
  },
  async update() {
    let mode = this._displayMode;
    if (this._autoRefreshInterval || !State._buffer[0]) {
      // Update the state only if we are not on pause.
      await State.update();
      if (document.hidden)
        return;
    }
    await wait(0);
    if (!performanceCountersEnabled()) {
      let state = await (mode == MODE_GLOBAL ?
        State.promiseDeltaSinceStartOfTime() :
        State.promiseDeltaSinceStartOfBuffer());

      for (let category of ["webpages"]) {
        await wait(0);
        await View.updateCategory(state[category], category, category, mode);
      }
      await wait(0);

      // Make sure that we do not keep obsolete stuff around.
      View.DOMCache.trimTo(state.deltas);
    } else {
      // If the mouse has been moved recently, update the data displayed
      // without moving any item to avoid the risk of users clicking an action
      // button for the wrong item.
      if (Date.now() - this._lastMouseEvent < TIME_BEFORE_SORTING_AGAIN) {
        let energyImpactPerId = new Map();
        for (let {id, dispatchesSincePrevious,
                  durationSincePrevious} of State.getCounters()) {
          let energyImpact = this._computeEnergyImpact(dispatchesSincePrevious,
                                                       durationSincePrevious);
          energyImpactPerId.set(id, energyImpact);
        }

        let row = document.getElementById("dispatch-tbody").firstChild;
        while (row) {
          if (row.windowId && energyImpactPerId.has(row.windowId)) {
            // We update the value in the Energy Impact column, but don't
            // update the children, as if the child count changes there's a
            // risk of making other rows move up or down.
            const kEnergyImpactColumn = 2;
            let elt = row.childNodes[kEnergyImpactColumn];
            View.displayEnergyImpact(elt, energyImpactPerId.get(row.windowId));
          }
          row = row.nextSibling;
        }
        return;
      }

      let selectedId = -1;
      // Reset the selectedRow field and the _openItems set each time we redraw
      // to avoid keeping forever references to closed window ids.
      if (this.selectedRow) {
        selectedId = this.selectedRow.windowId;
        this.selectedRow = null;
      }
      let openItems = this._openItems;
      this._openItems = new Set();

      let counters = this._sortCounters(State.getCounters());
      for (let {id, name, image, type, totalDispatches, dispatchesSincePrevious,
                totalDuration, durationSincePrevious, children} of counters) {
        let row =
          View.appendRow(name,
                         this._computeEnergyImpact(dispatchesSincePrevious,
                                                   durationSincePrevious),
                         {totalDispatches, totalDuration: Math.ceil(totalDuration / 1000),
                          dispatchesSincePrevious,
                          durationSincePrevious: Math.ceil(durationSincePrevious / 1000)},
                         type, image);
        row.windowId = id;
        if (id == selectedId) {
          row.setAttribute("selected", "true");
          this.selectedRow = row;
        }

        if (!children.length)
          continue;

        // Show the twisty image.
        let elt = row.firstChild;
        let img = document.createElement("img");
        img.className = "twisty";
        let open = openItems.has(id);
        if (open) {
          img.classList.add("open");
          this._openItems.add(id);
        }

        // If there's an l10n id on our <td> node, any image we add will be
        // removed during localization, so move the l10n id to a <span>
        let l10nAttrs = document.l10n.getAttributes(elt);
        if (l10nAttrs.id) {
          let span = document.createElement("span");
          document.l10n.setAttributes(span, l10nAttrs.id, l10nAttrs.args);
          elt.removeAttribute("data-l10n-id");
          elt.removeAttribute("data-l10n-args");
          elt.insertBefore(span, elt.firstChild);
        }

        elt.insertBefore(img, elt.firstChild);

        row._children = children;
        if (open)
          this._showChildren(row);
      }

      View.commit();
    }

    await wait(0);

    // Inform watchers
    Services.obs.notifyObservers(null, UPDATE_COMPLETE_TOPIC, mode);
  },
  _showChildren(row) {
    let children = row._children;
    children.sort((a, b) => b.dispatchesSincePrevious - a.dispatchesSincePrevious);
    for (let row of children) {
      let host = row.host.replace(/^blob:https?:\/\//, "");
      let type = "subframe";
      if (State.isTracker(host))
        type = "tracker";
      if (row.isWorker)
        type = "worker";
      View.appendRow(row.host,
                     this._computeEnergyImpact(row.dispatchesSincePrevious,
                                               row.durationSincePrevious),
                     {totalDispatches: row.dispatchCount,
                      totalDuration: Math.ceil(row.duration / 1000),
                      dispatchesSincePrevious: row.dispatchesSincePrevious,
                      durationSincePrevious: Math.ceil(row.durationSincePrevious / 1000)},
                     type);
    }
  },
  _computeEnergyImpact(dispatches, duration) {
    // 'Dispatches' doesn't make sense to users, and it's difficult to present
    // two numbers in a meaningful way, so we need to somehow aggregate the
    // dispatches and duration values we have.
    // The current formula to aggregate the numbers assumes that the cost of
    // a dispatch is equivalent to 1ms of CPU time.
    // Dividing the result by the sampling interval and by 10 gives a number that
    // looks like a familiar percentage to users, as fullying using one core will
    // result in a number close to 100.
    let energyImpact =
      Math.max(duration || 0, dispatches * 1000) / UPDATE_INTERVAL_MS / 10;
    // Keep only 2 digits after the decimal point.
    return Math.ceil(energyImpact * 100) / 100;
  },
  _sortCounters(counters) {
    return counters.sort((a, b) => {
      // Force 'Recently Closed Tabs' to be always at the bottom, because it'll
      // never be actionable.
      if (a.name.id && a.name.id == "ghost-windows")
        return 1;

      // Note: _computeEnergyImpact uses UPDATE_INTERVAL_MS which doesn't match
      // the time between the most recent sample and the start of the buffer,
      // BUFFER_DURATION_MS would be better, but the values is never displayed
      // so this is OK.
      let aEI = this._computeEnergyImpact(a.dispatchesSinceStartOfBuffer,
                                          a.durationSinceStartOfBuffer);
      let bEI = this._computeEnergyImpact(b.dispatchesSinceStartOfBuffer,
                                          b.durationSinceStartOfBuffer);
      if (aEI != bEI)
        return bEI - aEI;

      // a.name is sometimes an object, so we can't use a.name.localeCompare.
      return String.prototype.localeCompare.call(a.name, b.name);
    });
  },
  _setOptions(options) {
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
  _initAutorefresh() {
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
    };

    let eltRefresh = document.getElementById("check-autorefresh");
    eltRefresh.addEventListener("change", () => onRefreshChange(true));

    onRefreshChange(false);
  },
  _autoRefreshInterval: null,
  _initDisplayMode() {
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
    eltLabelRecent.textContent = `Display only the latest ${Math.round(BUFFER_DURATION_MS / 1000)}s`;

    onModeChange(false);
  },
  // The display mode. One of `MODE_GLOBAL` or `MODE_RECENT`.
  _displayMode: MODE_GLOBAL,
};

var go = async function() {

  Control.init();

  if (performanceCountersEnabled()) {
    let opt = document.querySelector(".options");
    opt.style.display = "none";
    opt.nextElementSibling.style.display = "none";
  } else {
    document.getElementById("dispatch-table").parentNode.style.display = "none";
  }

  // Setup a hook to allow tests to configure and control this page
  let testUpdate = function(subject, topic, value) {
    let options = JSON.parse(value);
    Control._setOptions(options);
    Control.update();
  };
  Services.obs.addObserver(testUpdate, TEST_DRIVER_TOPIC);
  window.addEventListener("unload", () => Services.obs.removeObserver(testUpdate, TEST_DRIVER_TOPIC));

  await Control.update();
  await wait(BUFFER_SAMPLING_RATE_MS * 1.1);
  await Control.update();
};
