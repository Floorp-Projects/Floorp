/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["TelemetryStopwatch"];

let Telemetry = Cc["@mozilla.org/base/telemetry;1"]
                  .getService(Ci.nsITelemetry);

// simpleTimers are directly associated with a histogram
// name. objectTimers are associated with an object _and_
// a histogram name.
let simpleTimers = {};
let objectTimers = new WeakMap();

this.TelemetryStopwatch = {
  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param aHistogram a string which must be a valid histogram name
   *                   from TelemetryHistograms.h
   *
   * @param aObj Optional parameter. If specified, the timer is associated
   *             with this object, meaning that multiple timers for a same
   *             histogram may be run concurrently, as long as they are
   *             associated with different objects.
   *
   * @return true if the timer was successfully started, false otherwise. If a
   *         timer already exists, it can't be started again, and the existing
   *         one will be cleared in order to avoid measurements errors.
   */
  start: function(aHistogram, aObj) {
    if (!validTypes(aHistogram, aObj))
      return false;

    let timers;
    if (aObj) {
      timers = objectTimers.get(aObj) || {};
      objectTimers.set(aObj, timers);
    } else {
      timers = simpleTimers;
    }

    if (timers.hasOwnProperty(aHistogram)) {
      delete timers[aHistogram];
      Cu.reportError("TelemetryStopwatch: key \"" +
                     aHistogram + "\" was already initialized");
      return false;
    }

    timers[aHistogram] = Components.utils.now();
    return true;
  },

  /**
   * Deletes the timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object. Important: Only use this method when a legitimate cancellation
   * should be done.
   *
   * @param aHistogram a string which must be a valid histogram name
   *                   from TelemetryHistograms.json
   *
   * @param aObj Optional parameter. If specified, the timer is associated
   *             with this object, meaning that multiple timers for a same
   *             histogram may be run concurrently, as long as they are
   *             associated with different objects.
   *
   * @return true if the timer exist and it was cleared, false otherwise.
   */
  cancel: function ts_cancel(aHistogram, aObj) {
    if (!validTypes(aHistogram, aObj))
      return false;

    let timers = aObj
                 ? objectTimers.get(aObj) || {}
                 : simpleTimers;

    if (timers.hasOwnProperty(aHistogram)) {
      delete timers[aHistogram];
      return true;
    }

    return false;
  },

  /**
   * Returns the elapsed time for a particular stopwatch. Primarily for
   * debugging purposes. Must be called prior to finish.
   *
   * @param aHistogram a string which must be a valid histogram name
   *                   from TelemetryHistograms.h. If an invalid name
   *                   is given, the function will throw.
   *
   * @param aObj Optional parameter which associates the histogram timer with
   *             the given object.
   *
   * @return time in milliseconds or -1 if the stopwatch was not found.
   */
  timeElapsed: function(aHistogram, aObj) {
    if (!validTypes(aHistogram, aObj))
      return -1;
    let timers = aObj
                 ? objectTimers.get(aObj) || {}
                 : simpleTimers;
    let start = timers[aHistogram];
    if (start) {
      let delta = Components.utils.now() - start;
      return Math.round(delta);
    }
    return -1;
  },

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param aHistogram a string which must be a valid histogram name
   *                   from TelemetryHistograms.h. If an invalid name
   *                   is given, the function will throw.
   *
   * @param aObj Optional parameter which associates the histogram timer with
   *             the given object.
   *
   * @return true if the timer was succesfully stopped and the data was
   *         added to the histogram, false otherwise.
   */
  finish: function(aHistogram, aObj) {
    if (!validTypes(aHistogram, aObj))
      return false;

    let timers = aObj
                 ? objectTimers.get(aObj) || {}
                 : simpleTimers;

    let start = timers[aHistogram];
    delete timers[aHistogram];

    if (start) {
      let delta = Components.utils.now() - start;
      delta = Math.round(delta);
      let histogram = Telemetry.getHistogramById(aHistogram);
      histogram.add(delta);
      return true;
    }

    return false;
  }
}

function validTypes(aKey, aObj) {
  return (typeof aKey == "string") &&
         (aObj === undefined || typeof aObj == "object");
}
