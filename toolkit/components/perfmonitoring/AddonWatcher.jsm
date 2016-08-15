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
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PerformanceWatcher",
                                  "resource://gre/modules/PerformanceWatcher.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                  "@mozilla.org/base/telemetry;1",
                                  Ci.nsITelemetry);
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "IdleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   Ci.nsIIdleService);

/**
 * Don't notify observers of slow add-ons if at least `SUSPICIOUSLY_MANY_ADDONS`
 * show up at the same time. We assume that this indicates that the system itself
 * is busy, and that add-ons are not responsible.
 */
let SUSPICIOUSLY_MANY_ADDONS = 5;

this.AddonWatcher = {
  /**
   * Watch this topic to be informed when a slow add-on is detected and should
   * be reported to the user.
   *
   * If you need finer-grained control, use PerformanceWatcher.jsm.
   */
  TOPIC_SLOW_ADDON_DETECTED: "addon-watcher-detected-slow-addon",

  init: function() {
    this._initializedTimeStamp = Cu.now();

    try {
      this._ignoreList = new Set(JSON.parse(Preferences.get("browser.addon-watch.ignore", null)));
    } catch (ex) {
      // probably some malformed JSON, ignore and carry on
      this._ignoreList = new Set();
    }

    this._warmupPeriod = Preferences.get("browser.addon-watch.warmup-ms", 60 * 1000 /* 1 minute */);
    this._idleThreshold = Preferences.get("browser.addon-watch.deactivate-after-idle-ms", 3000);
    this.paused = false;
  },
  uninit: function() {
    this.paused = true;
  },
  _initializedTimeStamp: 0,

  set paused(paused) {
    if (paused) {
      if (this._listener) {
        PerformanceWatcher.removePerformanceListener({addonId: "*"}, this._listener);
      }
      this._listener = null;
    } else {
      this._listener = this._onSlowAddons.bind(this);
      PerformanceWatcher.addPerformanceListener({addonId: "*"}, this._listener);
    }
  },
  get paused() {
    return !this._listener;
  },
  _listener: null,

  /**
   * Provide the following object for each addon:
   *  {number} occurrences The total number of performance alerts recorded for this addon.
   *  {number} occurrencesSinceLastNotification The number of performances alerts recorded
   *     since we last notified the user.
   *  {number} latestNotificationTimeStamp The timestamp of the latest user notification
   *     that this add-on is slow.
   */
  _getAlerts: function(addonId) {
    let alerts = this._alerts.get(addonId);
    if (!alerts) {
      alerts = {
        occurrences: 0,
        occurrencesSinceLastNotification: 0,
        latestNotificationTimeStamp: 0,
      };
      this._alerts.set(addonId, alerts);
    }
    return alerts;
  },
  _alerts: new Map(),
  _onSlowAddons: function(addons) {
    try {
      if (IdleService.idleTime >= this._idleThreshold) {
        // The application is idle. Maybe the computer is sleeping, or maybe
        // the user isn't in front of it. Regardless, the user doesn't care
        // about things that slow down her browser while she's not using it.
        return;
      }

      if (addons.length > SUSPICIOUSLY_MANY_ADDONS) {
        // Heuristic: if we are notified of many slow addons at once, the issue
        // is probably not with the add-ons themselves with the system. We may
        // for instance be waking up from hibernation, or the system may be
        // busy swapping.
        return;
      }

      let now = Cu.now();
      if (now - this._initializedTimeStamp < this._warmupPeriod) {
        // Heuristic: do not report slowdowns during or just after startup.
        return;
      }

      // Report immediately to Telemetry, regardless of whether we report to
      // the user.
      for (let {source: {addonId}, details} of addons) {
        Telemetry.getKeyedHistogramById("PERF_MONITORING_SLOW_ADDON_JANK_US").
          add(addonId, details.highestJank);
        Telemetry.getKeyedHistogramById("PERF_MONITORING_SLOW_ADDON_CPOW_US").
          add(addonId, details.highestCPOW);
      }

      // We expect that users don't care about real-time alerts unless their
      // browser is going very, very slowly. Therefore, we use the following
      // heuristic:
      // - if jank is above freezeThreshold (e.g. 5 seconds), report immediately; otherwise
      // - if jank is below jankThreshold (e.g. 128ms), disregard; otherwise
      // - if the latest jank was more than prescriptionDelay (e.g. 5 minutes) ago, reset number of occurrences;
      // - if we have had fewer than occurrencesBetweenAlerts janks (e.g. 3) since last alert, disregard; otherwise
      // - if we have displayed an alert for this add-on less than delayBetweenAlerts ago (e.g. 6h), disregard; otherwise
      // - also, don't report more than highestNumberOfAddonsToReport (e.g. 1) at once.
      let freezeThreshold = Preferences.get("browser.addon-watch.freeze-threshold-micros", /* 5 seconds */ 5000000);
      let jankThreshold = Preferences.get("browser.addon-watch.jank-threshold-micros", /* 256 ms == 8 frames*/ 256000);
      let occurrencesBetweenAlerts = Preferences.get("browser.addon-watch.occurrences-between-alerts", 3);
      let delayBetweenAlerts = Preferences.get("browser.addon-watch.delay-between-alerts-ms", 6 * 3600 * 1000 /* 6h */);
      let delayBetweenFreezeAlerts = Preferences.get("browser.addon-watch.delay-between-freeze-alerts-ms", 2 * 60 * 1000 /* 2 min */);
      let prescriptionDelay = Preferences.get("browser.addon-watch.prescription-delay", 5 * 60 * 1000 /* 5 minutes */);
      let highestNumberOfAddonsToReport = Preferences.get("browser.addon-watch.max-simultaneous-reports", 1);

      addons = addons.filter(x => x.details.highestJank >= jankThreshold).
        sort((a, b) => a.details.highestJank - b.details.highestJank);

      for (let {source: {addonId}, details} of addons) {
        if (highestNumberOfAddonsToReport <= 0) {
          return;
        }
        if (this._ignoreList.has(addonId)) {
          // Add-on is ignored.
          continue;
        }

        let alerts = this._getAlerts(addonId);
        if (now - alerts.latestOccurrence >= prescriptionDelay) {
          // While this add-on has already caused slownesss, this
          // was a long time ago, let's forgive.
          alerts.occurrencesSinceLastNotification = 0;
        }

        alerts.occurrencesSinceLastNotification++;
        alerts.occurrences++;

        if (details.highestJank < freezeThreshold) {
          if (alerts.occurrencesSinceLastNotification <= occurrencesBetweenAlerts) {
            // While the add-on has caused jank at least once, we are only
            // interested in repeat offenders. Store the data for future use.
            continue;
          }
          if (now - alerts.latestNotificationTimeStamp <= delayBetweenAlerts) {
            // We have already displayed an alert for this add-on recently.
            // Wait a little before displaying another one.
            continue;
          }
        } else if (now - alerts.latestNotificationTimeStamp <= delayBetweenFreezeAlerts) {
          // Even in case of freeze, we want to avoid needlessly spamming the user.
          // We have already displayed an alert for this add-on recently.
          // Wait a little before displaying another one.
          continue;
        }

        // Ok, time to inform the user.
        alerts.latestNotificationTimeStamp = now;
        alerts.occurrencesSinceLastNotification = 0;
        Services.obs.notifyObservers(null, this.TOPIC_SLOW_ADDON_DETECTED, addonId);

        highestNumberOfAddonsToReport--;
      }
    } catch (ex) {
      Cu.reportError("Error in AddonWatcher._onSlowAddons " + ex);
      Cu.reportError(Task.Debugging.generateReadableStack(ex.stack));
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
  },

  /**
   * The list of alerts for this session.
   *
   * @type {Map<String, Object>} A map associating addonId to
   *  objects with fields
   *  {number} occurrences The total number of performance alerts recorded for this addon.
   *  {number} occurrencesSinceLastNotification The number of performances alerts recorded
   *     since we last notified the user.
   *  {number} latestNotificationTimeStamp The timestamp of the latest user notification
   *     that this add-on is slow.
   */
  get alerts() {
    let result = new Map();
    for (let [k, v] of this._alerts) {
      result.set(k, Cu.cloneInto(v, this));
    }
    return result;
  },
};
