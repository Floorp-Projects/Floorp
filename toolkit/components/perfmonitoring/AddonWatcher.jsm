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
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PerformanceStats",
                                  "resource://gre/modules/PerformanceStats.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                  "@mozilla.org/base/telemetry;1",
                                  Ci.nsITelemetry);
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

let AddonWatcher = {
  _previousPerformanceIndicators: {},
  _timer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
  _callback: null,
  /**
   * The interval at which we poll the available performance information
   * to find out about possibly slow add-ons, in milliseconds.
   */
  _interval: 15000,
  _ignoreList: null,
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
    } else {
      PerformanceStats.init();
      this._timer.initWithCallback(this._checkAddons.bind(this), this._interval, Ci.nsITimer.TYPE_REPEATING_SLACK);
    }
    this._isPaused = isPaused;
  },
  get paused() {
    return this._isPaused;
  },
  _isPaused: true,

  /**
   * Check the performance of add-ons during the latest slice of time.
   *
   * We consider that an add-on is causing slowdown if it has executed
   * without interruption for at least 64ms (4 frames) at least once
   * during the latest slice, or if it has used any CPOW during the latest
   * slice.
   */
  _checkAddons: function() {
    try {
      let snapshot = PerformanceStats.getSnapshot();

      let limits = {
        // By default, warn if we have a total time of 1s of CPOW per 15 seconds
        totalCPOWTime: Math.round(Preferences.get("browser.addon-watch.limits.totalCPOWTime", 1000000) * this._interval / 15000),
        // By default, warn if we have skipped 4 consecutive frames
        // at least once during the latest slice.
        longestDuration: Math.round(Math.log2(Preferences.get("browser.addon-watch.limits.longestDuration", 128))),
      };

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
        let previous = this._previousPerformanceIndicators[addonId];
        this._previousPerformanceIndicators[addonId] = item;

        if (!previous) {
          // This is the first time we see the addon, so we are probably
          // executed right during/after startup. Performance is always
          // weird during startup, with the JIT warming up, competition
          // in disk access, etc. so we do not take this as a reason to
          // display the slow addon warning.
          continue;
        }

        // Report misbehaviors to Telemetry

        let diff = item.substract(previous);
        if (diff.longestDuration > 5) {
          Telemetry.getKeyedHistogramById("MISBEHAVING_ADDONS_JANK_LEVEL").
            add(addonId, diff.longestDuration);
        }
        if (diff.totalCPOWTime > 0) {
          Telemetry.getKeyedHistogramById("MISBEHAVING_ADDONS_CPOW_TIME_MS").
            add(addonId, diff.totalCPOWTime / 1000);
        }

        // Report mibehaviors to the user.
        let reason = null;

        for (let k of ["longestDuration", "totalCPOWTime"]) {
          if (limits[k] > 0 && diff[k] > limits[k]) {
            reason = k;
          }
        }

        if (!reason) {
          continue;
        }

        try {
          this._callback(addonId, reason);
        } catch (ex) {
          Cu.reportError("Error in AddonWatcher._checkAddons callback " + ex);
          Cu.reportError(ex.stack);
        }
      }
    } catch (ex) {
      Cu.reportError("Error in AddonWatcher._checkAddons " + ex);
      Cu.reportError(ex.stack);
    }
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
  }
};
