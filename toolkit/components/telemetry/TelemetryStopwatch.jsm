/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["TelemetryStopwatch"];

Cu.import("resource://gre/modules/Log.jsm", this);
var Telemetry = Cc["@mozilla.org/base/telemetry;1"]
                  .getService(Ci.nsITelemetry);

// Weak map does not allow using null objects as keys. These objects are used
// as 'null' placeholders.
const NULL_OBJECT = {};
const NULL_KEY = {};

/**
 * Timers is a variation of a Map used for storing information about running
 * Stopwatches. Timers has the following data structure:
 *
 * {
 *    "HISTOGRAM_NAME": WeakMap {
 *      Object || NULL_OBJECT: Map {
 *        "KEY" || NULL_KEY: startTime
 *        ...
 *      }
 *      ...
 *    }
 *    ...
 * }
 *
 *
 * @example
 * // Stores current time for a keyed histogram "PLAYING_WITH_CUTE_ANIMALS".
 * Timers.put("PLAYING_WITH_CUTE_ANIMALS", null, "CATS", Date.now());
 *
 * @example
 * // Returns information about a simple Stopwatch.
 * let startTime = Timers.get("PLAYING_WITH_CUTE_ANIMALS", null, "CATS");
 */
let Timers = {
  _timers: new Map(),

  _validTypes: function(histogram, obj, key) {
    let nonEmptyString = value => {
      return typeof value === "string" && value !== "" && value.length > 0;
    };
    return  nonEmptyString(histogram) &&
            typeof obj == "object" &&
           (key === NULL_KEY || nonEmptyString(key));
  },

  get: function(histogram, obj, key) {
    key = key === null ? NULL_KEY : key;
    obj = obj || NULL_OBJECT;

    if (!this.has(histogram, obj, key)) {
      return null;
    }

    return this._timers.get(histogram).get(obj).get(key);
  },

  put: function(histogram, obj, key, startTime) {
    key = key === null ? NULL_KEY : key;
    obj = obj || NULL_OBJECT;

    if (!this._validTypes(histogram, obj, key)) {
      return false;
    }

    let objectMap = this._timers.get(histogram) || new WeakMap();
    let keyedInfo = objectMap.get(obj) || new Map();
    keyedInfo.set(key, startTime);
    objectMap.set(obj, keyedInfo);
    this._timers.set(histogram, objectMap);
    return true;
  },

  has: function(histogram, obj, key) {
    key = key === null ? NULL_KEY : key;
    obj = obj || NULL_OBJECT;

    return this._timers.has(histogram) &&
      this._timers.get(histogram).has(obj) &&
      this._timers.get(histogram).get(obj).has(key);
  },

  delete: function(histogram, obj, key) {
    key = key === null ? NULL_KEY : key;
    obj = obj || NULL_OBJECT;

    if (!this.has(histogram, obj, key)) {
      return false;
    }
    let objectMap = this._timers.get(histogram);
    let keyedInfo = objectMap.get(obj);
    if (keyedInfo.size > 1) {
      keyedInfo.delete(key);
      return true;
    }
    objectMap.delete(obj);
    // NOTE:
    // We never delete empty objecMaps from this._timers because there is no
    // nice solution for tracking the number of objects in a WeakMap.
    // WeakMap is not enumerable, so we can't deterministically say when it's
    // empty. We accept that trade-off here, given that entries for short-lived
    // objects will go away when they are no longer referenced
    return true;
  }
};

this.TelemetryStopwatch = {
  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {Object} aObj - Optional parameter. If specified, the timer is
   *                        associated with this object, meaning that multiple
   *                        timers for the same histogram may be run
   *                        concurrently, as long as they are associated with
   *                        different objects.
   *
   * @returns {Boolean} True if the timer was successfully started, false
   *                    otherwise. If a timer already exists, it can't be
   *                    started again, and the existing one will be cleared in
   *                    order to avoid measurements errors.
   */
  start: function(aHistogram, aObj) {
    return TelemetryStopwatchImpl.start(aHistogram, aObj, null);
  },

  /**
   * Deletes the timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object. Important: Only use this method when a legitimate cancellation
   * should be done.
   *
  * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {Object} aObj - Optional parameter. If specified, the timer is
   *                        associated with this object, meaning that multiple
   *                        timers or a same histogram may be run concurrently,
   *                        as long as they are associated with different
   *                        objects.
   *
   * @returns {Boolean} True if the timer exist and it was cleared, False
   *                   otherwise.
   */
  cancel: function(aHistogram, aObj) {
    return TelemetryStopwatchImpl.cancel(aHistogram, aObj, null);
  },

  /**
   * Returns the elapsed time for a particular stopwatch. Primarily for
   * debugging purposes. Must be called prior to finish.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *                              If an invalid name is given, the function will
   *                              throw.
   *
   * @param (Object) aObj - Optional parameter which associates the histogram
   *                        timer with the given object.
   *
   * @returns {Integer} time in milliseconds or -1 if the stopwatch was not
   *                   found.
   */
  timeElapsed: function(aHistogram, aObj) {
    return TelemetryStopwatchImpl.timeElapsed(aHistogram, aObj, null);
  },

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {Object} aObj - Optional parameter which associates the histogram
   *                        timer with the given object.
   *
   * @returns {Boolean} True if the timer was succesfully stopped and the data
   *                    was added to the histogram, False otherwise.
   */
  finish: function(aHistogram, aObj) {
    return TelemetryStopwatchImpl.finish(aHistogram, aObj, null);
  },

  /**
   * Starts a timer associated with a keyed telemetry histogram. The timer can
   * be directly associated with a histogram and its key. Similarly to
   * @see{TelemetryStopwatch.stat} the histogram and its key can be associated
   * with an object. Each key may have multiple associated objects and each
   * object can be associated with multiple keys.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {String} aKey - a string which must be a valid histgram key.
   *
   * @param {Object} aObj - Optional parameter. If specified, the timer is
   *                        associated with this object, meaning that multiple
   *                        timers for the same histogram may be run
   *                        concurrently,as long as they are associated with
   *                        different objects.
   *
   * @returns {Boolean} True if the timer was successfully started, false
   *                    otherwise. If a timer already exists, it can't be
   *                    started again, and the existing one will be cleared in
   *                    order to avoid measurements errors.
   */
  startKeyed: function(aHistogram, aKey, aObj) {
    return TelemetryStopwatchImpl.start(aHistogram, aObj, aKey);
  },

  /**
   * Deletes the timer associated with a keyed histogram. Important: Only use
   * this method when a legitimate cancellation should be done.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {String} aKey - a string which must be a valid histgram key.
   *
   * @param {Object} aObj - Optional parameter. If specified, the timer
   *                        associated with this object is deleted.
   *
   * @return {Boolean} True if the timer exist and it was cleared, False
   *                   otherwise.
   */
  cancelKeyed: function(aHistogram, aKey, aObj) {
    return TelemetryStopwatchImpl.cancel(aHistogram, aObj, aKey);
  },

  /**
   * Returns the elapsed time for a particular stopwatch. Primarily for
   * debugging purposes. Must be called prior to finish.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {String} aKey - a string which must be a valid histgram key.
   *
   * @param {Object} aObj - Optional parameter. If specified, the timer
   *                        associated with this object is used to calculate
   *                        the elapsed time.
   *
   * @return {Integer} time in milliseconds or -1 if the stopwatch was not
   *                   found.
   */
  timeElapsedKeyed: function(aHistogram, aKey, aObj) {
    return TelemetryStopwatchImpl.timeElapsed(aHistogram, aObj, aKey);
  },

  /**
   * Stops the timer associated with the given keyed histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the keyed histogram.
   *
   * @param {String} aHistogram - a string which must be a valid histogram name.
   *
   * @param {String} aKey - a string which must be a valid histgram key.
   *
   * @param {Object} aObj - optional parameter which associates the histogram
   *                        timer with the given object.
   *
   * @returns {Boolean} True if the timer was succesfully stopped and the data
   *                   was added to the histogram, False otherwise.
   */
  finishKeyed: function(aHistogram, aKey, aObj) {
    return TelemetryStopwatchImpl.finish(aHistogram, aObj, aKey);
  }
};

this.TelemetryStopwatchImpl = {
  start: function(histogram, object, key) {
    if (Timers.has(histogram, object, key)) {
      Timers.delete(histogram, object, key);
      Cu.reportError(`TelemetryStopwatch: key "${histogram}" was already ` +
                     "initialized");
      return false;
    }

    return Timers.put(histogram, object, key, Components.utils.now());
  },

  cancel: function (histogram, object, key) {
    return Timers.delete(histogram, object, key);
  },

  timeElapsed: function(histogram, object, key) {
    let startTime = Timers.get(histogram, object, key);
    if (startTime === null) {
      Cu.reportError("TelemetryStopwatch: requesting elapsed time for " +
                     `nonexisting stopwatch. Histogram: "${histogram}", ` +
                     `key: "${key}"`);
      return -1;
    }

    try {
      let delta = Components.utils.now() - startTime
      return Math.round(delta);
    } catch (e) {
      Cu.reportError("TelemetryStopwatch: failed to calculate elapsed time " +
                     `for Histogram: "${histogram}", key: "${key}", ` +
                     `exception: ${Log.exceptionStr(e)}`);
      return -1;
    }
  },

  finish: function(histogram, object, key) {
    let delta = this.timeElapsed(histogram, object, key);
    if (delta == -1) {
      return false;
    }

    try {
      if (key) {
        Telemetry.getKeyedHistogramById(histogram).add(key, delta);
      } else {
        Telemetry.getHistogramById(histogram).add(delta);
      }
    } catch (e) {
      Cu.reportError("TelemetryStopwatch: failed to update the Histogram " +
                     `"${histogram}", using key: "${key}", ` +
                     `exception: ${Log.exceptionStr(e)}`);
      return false;
    }

    return Timers.delete(histogram, object, key);
  }
}
