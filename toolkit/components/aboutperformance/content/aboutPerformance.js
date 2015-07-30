/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const { AddonWatcher } = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});
const { PerformanceStats } = Cu.import("resource://gre/modules/PerformanceStats.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

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
function findTabFromWindow(windowId) {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    let tabbrowser = win.gBrowser;
    let foundBrowser = tabbrowser.getBrowserForOuterWindowID(windowId);
    if (foundBrowser) {
      return {tabbrowser, tab: tabbrowser.getTabForBrowser(foundBrowser)};
    }
  }
  return null;
}

/**
 * Utilities for dealing with PerformanceDiff
 */
let Delta = {
  compare: function(a, b) {
    // By definition, if either `a` or `b` has CPOW time and the other
    // doesn't, it is larger.
    if (a.cpow.totalCPOWTime && !b.cpow.totalCPOWTime) {
      return 1;
    }
    if (b.cpow.totalCPOWTime && !a.cpow.totalCPOWTime) {
      return -1;
    }
    return (
      (a.jank.longestDuration - b.jank.longestDuration) ||
      (a.jank.totalUserTime - b.jank.totalUserTime) ||
      (a.jank.totalSystemTime - b.jank.totalSystemTime) ||
      (a.cpow.totalCPOWTime - b.cpow.totalCPOWTime) ||
      (a.ticks.ticks - b.ticks.ticks) ||
      0
    );
  },
  revCompare: function(a, b) {
    return -Delta.compare(a, b);
  },
  /**
   * The highest value considered "good performance".
   */
  MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE: {
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
  },
  /**
   * The highest value considered "average performance".
   */
  MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE: {
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
}

/**
 * Utilities for dealing with state
 */
let State = {
  _monitor: PerformanceStats.getMonitor(["jank", "cpow", "ticks"]),

  /**
   * Indexed by the number of minutes since the snapshot was taken.
   *
   * @type {Array<{snapshot: PerformanceData, date: Date}>}
   */
  _buffer: [],
  /**
   * The first snapshot since opening the page.
   *
   * @type {snapshot: PerformnceData, date: Date}
   */
  _oldest: null,

  /**
   * The latest snapshot.
   *
   * @type As _oldest
   */
  _latest: null,

  /**
   * The performance alerts for each group.
   *
   * This map is cleaned up during each update to avoid leaking references
   * to groups that have been gc-ed.
   *
   * @type{Map<string, Array<number>} A map in which keys are
   * values for `delta.groupId` and values are arrays
   * [number of moderate-impact alerts, number of high-impact alerts]
   */
  _alerts: new Map(),

  /**
   * The date at which each group was first seen.
   *
   * This map is cleaned up during each update to avoid leaking references
   * to groups that have been gc-ed.
   *
   * @type{Map<string, Date} A map in which keys are
   * values for `delta.groupId` and values are approximate
   * dates at which the group was first encountered.
   */
  _firstSeen: new Map(),

  /**
   * Produce an annotated performance snapshot.
   *
   * @return {Object} An object extending `PerformanceDiff`
   * with the following fields:
   *  {Date} date The current date.
   *  {Map<groupId, performanceData>} A map of performance data,
   *  for faster access.
   */
  _promiseSnapshot: Task.async(function*() {
    let snapshot = yield this._monitor.promiseSnapshot();
    snapshot.date = Date.now();
    let componentsMap = new Map();
    snapshot.componentsMap = componentsMap;
    for (let component of snapshot.componentsData) {
      componentsMap.set(component.groupId, component);
    }
    return snapshot;
  }),

  /**
   * Classify the "slowness" of a component.
   *
   * @return {number} 0 For a "fast" component.
   * @return {number} 1 For an "average" component.
   * @return {number} 2 For a "slow" component.
   */
  _getSlowness: function(component) {
    if (Delta.compare(component, Delta.MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE) <= 0) {
      return 0;
    }
    if (Delta.compare(component, Delta.MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE) <= 0) {
      return 1;
    }
    return 2;
  },

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
      this._latest = this._oldest = yield this._promiseSnapshot();
      this._buffer.push(this._oldest);
      yield new Promise(resolve => setTimeout(resolve, BUFFER_SAMPLING_RATE_MS * 1.1));
    }


    let now = Date.now();
    
    // If we haven't sampled in a while, add a sample to the buffer.
    let latestInBuffer = this._buffer[this._buffer.length - 1];
    let deltaT = now - latestInBuffer.date;
    if (deltaT > BUFFER_SAMPLING_RATE_MS) {
      this._latest = this._oldest = yield this._promiseSnapshot();
      this._buffer.push(this._latest);

      // Update alerts
      let cleanedUpAlerts = new Map(); // A new map of alerts, without dead components.
      for (let component of this._latest.componentsData) {
        let slowness = this._getSlowness(component, deltaT);
        let myAlerts = this._alerts.get(component.groupId) || [0, 0];
        if (slowness > 0) {
          myAlerts[slowness - 1]++;
        }
        cleanedUpAlerts.set(component.groupId, myAlerts);
        this._alerts = cleanedUpAlerts;
      }
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
   */
  _promiseDeltaSince: Task.async(function*(oldest) {
    let current = this._latest;
    if (!oldest) {
      throw new TypeError();
    }
    if (!current) {
      throw new TypeError();
    }

    let addons = [];
    let webpages = [];
    let system = [];
    let groups = new Set();

    // We rebuild the map during each iteration to make sure that
    // we do not maintain references to groups that has been removed
    // (e.g. pages that have been closed).
    let oldFirstSeen = this._firstSeen;
    let cleanedUpFirstSeen = new Map();
    this._firstSeen = cleanedUpFirstSeen;
    for (let component of current.componentsData) {
      // Enrich `delta` with `alerts`.
      let delta = component.subtract(oldest.componentsMap.get(component.groupId));
      delta.alerts = (this._alerts.get(component.groupId) || []).slice();

      // Enrich `delta` with `age`.
      let creationDate = oldFirstSeen.get(component.groupId) || current.date;
      cleanedUpFirstSeen.set(component.groupId, creationDate);
      delta.age = current.date - creationDate;

      groups.add(component.groupId);

      // Enrich `delta` with `fullName` and `readableName`.
      delta.fullName = delta.name;
      delta.readableName = delta.name;
      if (component.addonId) {
        let found = yield new Promise(resolve =>
          AddonManager.getAddonByID(delta.addonId, a => {
            if (a) {
              delta.readableName = a.name;
              resolve(true);
            } else {
              resolve(false);
            }
          }));
        delta.fullName = delta.addonId;
        if (found) {
          // If the add-on manager doesn't know about an add-on, it's
          // probably not a real add-on.
          addons.push(delta);
        }
      } else if (!delta.isSystem || delta.title) {
        // Wallpaper hack. For some reason, about:performance (and only about:performance)
        // appears twice in the list. Only one of them is a window.
        if (delta.title == document.title) {
          if (!findTabFromWindow(delta.windowId)) {
            // Not a real page.
            system.push(delta);
            continue;
          }
        }
        delta.readableName = delta.title || delta.name;
        webpages.push(delta);
      } else {
        system.push(delta);
      }
    }

    return {
      addons,
      webpages,
      system,
      groups,
      duration: current.date - oldest.date
    };
  }),
};

let View = {
  /**
   * A cache for all the per-item DOM elements that are reused across refreshes.
   *
   * Reusing the same elements means that elements that were hidden (respectively
   * visible) in an iteration remain hidden (resp visible) in the next iteration.
   */
  DOMCache: {
    _map: new Map(),
    /**
     * @param {string} groupId The groupId for the item that we are displaying.
     * @return {null} If the `groupId` doesn't have a component cached yet.
     * Otherwise, the value stored with `set`.
     */
    get: function(groupId) {
      return this._map.get(groupId);
    },
    set: function(groupId, value) {
      this._map.set(groupId, value);
    },
    /**
     * Remove all the elements whose key does not appear in `set`.
     *
     * @param {Set} set a set of groupId.
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
  updateCategory: function(subset, id, nature, deltaT, currentMode) {
    subset = subset.slice().sort(Delta.revCompare);

    // Grab everything from the DOM before cleaning up
    let eltContainer = this._setupStructure(id);

    // An array of `cachedElements` that need to be added
    let toAdd = [];
    for (let delta of subset) {
      let cachedElements = this._grabOrCreateElements(delta, nature);
      toAdd.push(cachedElements);
      cachedElements.eltTitle.textContent = delta.readableName;
      cachedElements.eltName.textContent = `Full name: ${delta.fullName}.`;
      cachedElements.eltLoaded.textContent = `Measure start: ${Math.round(delta.age/1000)} seconds ago.`
      cachedElements.eltProcess.textContent = `Process: ${delta.processId} (${delta.isChildProcess?"child":"parent"}).`;
      let eltImpact = cachedElements.eltImpact;
      if (currentMode == MODE_RECENT) {
        cachedElements.eltRoot.setAttribute("impact", delta.jank.longestDuration + 1);
        if (Delta.compare(delta, Delta.MAX_DELTA_FOR_GOOD_RECENT_PERFORMANCE) <= 0) {
          eltImpact.textContent = ` currently performs well.`;
        } else if (Delta.compare(delta, Delta.MAX_DELTA_FOR_AVERAGE_RECENT_PERFORMANCE)) {
          eltImpact.textContent = ` may currently be slowing down ${BRAND_NAME}.`;
        } else {
          eltImpact.textContent = ` is currently considerably slowing down ${BRAND_NAME}.`;
        }
        cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.jank.longestDuration + 1}/${delta.jank.durations.length}.`;
        cachedElements.eltCPU.textContent = `CPU usage: ${Math.min(100, Math.ceil(delta.jank.totalUserTime/deltaT))}%.`;
        cachedElements.eltSystem.textContent = `System usage: ${Math.min(100, Math.ceil(delta.jank.totalSystemTime/deltaT))}%.`;
        cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.cpow.totalCPOWTime/deltaT)}%.`;
      } else {
        if (delta.alerts.length == 0) {
          eltImpact.textContent = " has performed well so far.";
          cachedElements.eltFPS.textContent = `Impact on framerate: no impact.`;
        } else {
          let sum = /* medium impact */ delta.alerts[0] + /* high impact */ delta.alerts[1];
          let frequency = sum * 1000 / delta.age;

          let describeFrequency;
          if (frequency <= MAX_FREQUENCY_FOR_NO_IMPACT) {
            describeFrequency = `has no impact on the performance of ${BRAND_NAME}.`
            cachedElements.eltRoot.classList.add("impact0");
          } else {
            let describeImpact;
            if (frequency <= MAX_FREQUENCY_FOR_RARE) {
              describeFrequency = `rarely slows down ${BRAND_NAME}.`;
            } else if (frequency <= MAX_FREQUENCY_FOR_FREQUENT) {
              describeFrequency = `has slown down ${BRAND_NAME} frequently.`;
            } else {
              describeFrequency = `seems to have slown down ${BRAND_NAME} very often.`;
            }
            // At this stage, `sum != 0`
            if (delta.alerts[1] / sum > MIN_PROPORTION_FOR_MAJOR_IMPACT) {
              describeImpact = "When this happens, the slowdown is generally important."
            } else {
              describeImpact = "When this happens, the slowdown is generally noticeable."
            }

            eltImpact.textContent = ` ${describeFrequency} ${describeImpact}`;
            cachedElements.eltFPS.textContent = `Impact on framerate: ${delta.alerts[1]} high-impacts, ${delta.alerts[0]} medium-impact.`;
          }
          cachedElements.eltCPU.textContent = `CPU usage: ${Math.min(100, Math.ceil(delta.jank.totalUserTime/delta.age))}% (total ${delta.jank.totalUserTime}ms).`;
          cachedElements.eltSystem.textContent = `System usage: ${Math.min(100, Math.ceil(delta.jank.totalSystemTime/delta.age))}% (total ${delta.jank.totalSystemTime}ms).`;
          cachedElements.eltCPOW.textContent = `Blocking process calls: ${Math.ceil(delta.cpow.totalCPOWTime/delta.age)}% (total ${delta.cpow.totalCPOWTime}ms).`;
        }
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
    let cachedElements = this.DOMCache.get(delta.groupId);
    if (cachedElements) {
      if (cachedElements.eltRoot.parentElement) {
        cachedElements.eltRoot.parentElement.removeChild(cachedElements.eltRoot);
      }
    } else {
      this.DOMCache.set(delta.groupId, cachedElements = {});

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
        AddonManager.getAddonByID(delta.addonId, addon => {
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
        let windowId = delta.windowId;
        eltCloseTab.addEventListener("click", () => {
          let found = findTabFromWindow(windowId);
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
          let found = findTabFromWindow(windowId);
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
        ["eltProcess", "process"]
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

let Control = {
  init: function() {
    this._initAutorefresh();
    this._initDisplayMode();
  },
  update: Task.async(function*() {
    let mode = this._displayMode;
    if (this._autoRefreshInterval) {
      // Update the state only if we are not on pause.
      yield State.update();
    }
    let state = yield (mode == MODE_GLOBAL?
      State.promiseDeltaSinceStartOfTime():
      State.promiseDeltaSinceStartOfBuffer());

    for (let category of ["webpages", "addons"]) {
      yield Promise.resolve();
      yield View.updateCategory(state[category], category, category, state.duration, mode);
    }
    yield Promise.resolve();

    // Make sure that we do not keep obsolete stuff around.
    View.DOMCache.trimTo(state.groups);

    // Inform watchers
    Services.obs.notifyObservers(null, UPDATE_COMPLETE_TOPIC, mode);
  }),
  _setOptions: function(options) {
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

let go = Task.async(function*() {
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
  yield new Promise(resolve => setTimeout(resolve, BUFFER_SAMPLING_RATE_MS * 1.1));
  yield Control.update();
});
