// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AddonWatcher"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PerformanceStats",
                                  "resource://gre/modules/PerformanceStats.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                  "@mozilla.org/base/telemetry;1",
                                  Ci.nsITelemetry);
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
const FILTERS = [
  {probe: "jank", field: "longestDuration"},
  {probe: "cpow", field: "totalCPOWTime"},
];

const WAKEUP_IS_SURPRISINGLY_SLOW_FACTOR = 2;
const THREAD_TAKES_LOTS_OF_CPU_FACTOR = .75;

let AddonWatcher = {
  _previousPerformanceIndicators: {},

  /**
   * Stats, designed to be consumed by clients of AddonWatcher.
   *
   */
  _stats: new Map(),
  _timer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
  _callback: null,
  /**
   * A performance monitor used to pull data from SpiderMonkey.
   *
   * @type {PerformanceStats Monitor}
   */
  _monitor: null,
  /**
   * The interval at which we poll the available performance information
   * to find out about possibly slow add-ons, in milliseconds.
   */
  _interval: 15000,
  _ignoreList: null,

  /**
   * The date of the latest wakeup, in milliseconds since an arbitrary
   * epoch.
   *
   * @type {number}
   */
  _latestWakeup: Date.now(),

  /**
   * Initialize and launch the AddonWatcher.
   *
   * @param {function} callback A callback, called whenever we determine
   * that an add-on is causing performance issues. It takes as argument
   *  {string} addonId The identifier of the add-on known to cause issues.
   *  {string} reason The reason for which the add-on has been flagged,
   *     as one of "totalCPOWTime" (the add-on has caused blocking process
   *     communications, which freeze the UX)
   *     Use preference "browser.addon-watch.limits.totalCPOWTime" to control
   *     the maximal amount of CPOW time per watch interval.
   *
   *     or "longestDuration" (the add-on has caused user-visible missed frames).
   *     Use preference "browser.addon-watch.limits.longestDuration" to control
   *     the longest uninterrupted execution of code of an add-on during a watch
   *     interval.
   */
  init: function(callback) {
    if (!callback) {
      return;
    }

    if (this._callback) {
      // Already initialized
      return;
    }

    this._interval = Preferences.get("browser.addon-watch.interval", 15000);
    if (this._interval == -1) {
      // Deactivated by preferences
      return;
    }

    this._callback = callback;
    try {
      this._ignoreList = new Set(JSON.parse(Preferences.get("browser.addon-watch.ignore", null)));
    } catch (ex) {
      // probably some malformed JSON, ignore and carry on
      this._ignoreList = new Set();
    }

    // Start monitoring
    this.paused = false;

    Services.obs.addObserver(() => {
      this.uninit();
    }, "profile-before-change", false);
  },
  uninit: function() {
    this.paused = true;
    this._callback = null;
  },

  /**
   * Interrupt temporarily add-on watching.
   */
  set paused(isPaused) {
    if (!this._callback || this._interval == -1) {
      return;
    }
    if (isPaused) {
      this._timer.cancel();
      if (this._monitor) {
        // We don't need the probes anymore, release them.
        this._monitor.dispose();
      }
      this._monitor = null;
    } else {
      this._monitor = PerformanceStats.getMonitor([for (filter of FILTERS) filter.probe]);
      this._timer.initWithCallback(this._checkAddons.bind(this), this._interval, Ci.nsITimer.TYPE_REPEATING_SLACK);
    }
    this._isPaused = isPaused;
  },
  get paused() {
    return this._isPaused;
  },
  _isPaused: true,

  /**
   * @return {true} If any measure we have for this wakeup is invalid
   * because the system is very busy and/or coming backup from hibernation.
   */
  _isSystemTooBusy: function(deltaT, currentSnapshot, previousSnapshot) {
    if (deltaT <= WAKEUP_IS_SURPRISINGLY_SLOW_FACTOR * this._interval) {
      // The wakeup was reasonably accurate.
      return false;
    }

    // There has been a strangely long delay between two successive
    // wakeups. This can mean one of the following things:
    // 1. we're in the process of initializing the app;
    // 2. the system is not responsive, either because it is very busy
    //   or because it has gone to sleep;
    // 3. the main loop of the application is so clogged that it could
    //   not process timer events.
    //
    // In cases 1. or 2., any alert here is a false positive.
    // In case 3., the application (hopefully an add-on) is misbehaving and we need
    // to identify what's wrong.

    if (!previousSnapshot) {
      // We're initializing, skip.
      return true;
    }

    let diff = snapshot.processData.subtract(previousSnapshot.processData);
    if (diff.totalCPUTime >= deltaT * THREAD_TAKES_LOTS_OF_CPU_FACTOR ) {
      // The main thread itself is using lots of CPU, perhaps because of
      // an add-on. We need to investigate.
      //
      // Note that any measurement based on wallclock time may
      // be affected by the lack of responsiveness of the main event loop,
      // so we may end up with false positives along the way.
      return false;
    }

    // The application is apparently behaving correctly, so the issue must
    // be somehow due to the system.
    return true;
  },

  /**
   * Check the performance of add-ons during the latest slice of time.
   *
   * We consider that an add-on is causing slowdown if it has executed
   * without interruption for at least 64ms (4 frames) at least once
   * during the latest slice, or if it has used any CPOW during the latest
   * slice.
   */
  _checkAddons: function() {
    let previousWakeup = this._latestWakeup;
    let currentWakeup = this._latestWakeup = Date.now();

    return Task.spawn(function*() {
      try {
        let previousSnapshot = this._latestSnapshot; // FIXME: Implement
        let snapshot = this._latestSnapshot = yield this._monitor.promiseSnapshot();
        let isSystemTooBusy = this._isSystemTooBusy(currentWakeup - previousWakeup, snapshot, previousSnapshot);

        let limits = {
          // By default, warn if we have a total time of 1s of CPOW per 15 seconds
          totalCPOWTime: Math.round(Preferences.get("browser.addon-watch.limits.totalCPOWTime", 1000000) * this._interval / 15000),
          // By default, warn if we have skipped 4 consecutive frames
          // at least once during the latest slice.
          longestDuration: Math.round(Math.log2(Preferences.get("browser.addon-watch.limits.longestDuration", 128))),
        };

        // By default, warn only after an add-on has been spotted misbehaving 3 times.
        let tolerance = Preferences.get("browser.addon-watch.tolerance", 3);

        for (let item of snapshot.componentsData) {
          let addonId = item.addonId;
          if (!item.isSystem || !addonId) {
            // We are only interested in add-ons.
            continue;
          }
          if (this._ignoreList.has(addonId)) {
            // This add-on has been explicitly put in the ignore list
            // by the user. Don't waste time with it.
            continue;
          }

          // Store the activity for the group – not the entire add-on, as we
          // can have one group per process for each add-on.
          let previous = this._previousPerformanceIndicators[item.groupId];
          this._previousPerformanceIndicators[item.groupId] = item;

          if (!previous) {
            // This is the first time we see the addon, so we are probably
            // executed right during/just after its startup. Performance is always
            // weird during startup, with the JIT warming up, competition
            // in disk access, etc. so we do not take this as a reason to
            // display the slow addon warning.
            continue;
          }
          if (isSystemTooBusy) {
            // The main event loop is behaving weirdly, most likely because of
            // the system being busy or asleep, so results are not trustworthy.
            // Ignore.
            continue;
          }

          // Report misbehaviors to Telemetry

          let diff = item.subtract(previous);
          if ("jank" in diff && diff.jank.longestDuration > 5) {
            Telemetry.getKeyedHistogramById("MISBEHAVING_ADDONS_JANK_LEVEL").
              add(addonId, diff.jank.longestDuration);
          }
          if ("cpow" in diff && diff.cpow.totalCPOWTime > 0) {
            Telemetry.getKeyedHistogramById("MISBEHAVING_ADDONS_CPOW_TIME_MS").
              add(addonId, diff.cpow.totalCPOWTime / 1000);
          }

          // Store misbehaviors for about:performance and other clients
          let stats = this._stats.get(addonId);
          if (!stats) {
            stats = {
              peaks: {},
              alerts: {},
            };
            this._stats.set(addonId, stats);
          }

          // Report misbehaviors to the user.

          for (let {probe, field: filter} of FILTERS) {
            let peak = stats.peaks[filter] || 0;
            let value = diff[probe][filter];
            stats.peaks[filter] = Math.max(value, peak);

            if (limits[filter] <= 0 || value <= limits[filter]) {
              continue;
            }

            stats.alerts[filter] = (stats.alerts[filter] || 0) + 1;

		    if (stats.alerts[filter] % tolerance != 0) {
              continue;
            }

            try {
              this._callback(addonId, filter);
            } catch (ex) {
              Cu.reportError("Error in AddonWatcher._checkAddons callback " + ex);
              Cu.reportError(ex.stack);
            }
          }
        }
      } catch (ex) {
        Cu.reportError("Error in AddonWatcher._checkAddons " + ex);
        Cu.reportError(Task.Debugging.generateReadableStack(ex.stack));
      }
    }.bind(this));
  },
  ignoreAddonForSession: function(addonid) {
    this._ignoreList.add(addonid);
  },
  ignoreAddonPermanently: function(addonid) {
    this._ignoreList.add(addonid);
    try {
      let ignoreList = JSON.parse(Preferences.get("browser.addon-watch.ignore", "[]"))
      if (!ignoreList.includes(addonid)) {
        ignoreList.push(addonid);
        Preferences.set("browser.addon-watch.ignore", JSON.stringify(ignoreList));
      }
    } catch (ex) {
      Preferences.set("browser.addon-watch.ignore", JSON.stringify([addonid]));
    }
  },
  /**
   * The list of alerts for this session.
   *
   * @type {Map<String, Object>} A map associating addonId to
   *  objects with fields
   *  - {Object} peaks The highest values encountered for each filter.
   *    - {number} longestDuration
   *    - {number} totalCPOWTime
   *  - {Object} alerts The number of alerts for each filter.
   *    - {number} longestDuration
   *    - {number} totalCPOWTime
   */
  get alerts() {
    let result = new Map();
    for (let [k, v] of this._stats) {
      result.set(k, Cu.cloneInto(v, this));
    }
    return result;
  },
};
