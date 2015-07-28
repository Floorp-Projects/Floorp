/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryUtils"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const IS_CONTENT_PROCESS = (function() {
  // We cannot use Services.appinfo here because in telemetry xpcshell tests,
  // appinfo is initially unavailable, and becomes available only later on.
  let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  return runtime.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
})();

this.TelemetryUtils = {
  /**
   * True if this is a content process.
   */
  get isContentProcess() {
    return IS_CONTENT_PROCESS;
  },

  /**
   * Turn a millisecond timestamp into a day timestamp.
   *
   * @param aMsec A number of milliseconds since Unix epoch.
   * @return The number of whole days since Unix epoch.
   */
  millisecondsToDays: function(aMsec) {
    return Math.floor(aMsec / MILLISECONDS_PER_DAY);
  },

  /**
   * Takes a date and returns it trunctated to a date with daily precision.
   */
  truncateToDays: function(date) {
    return new Date(date.getFullYear(),
                    date.getMonth(),
                    date.getDate(),
                    0, 0, 0, 0);
  },

  /**
   * Check if the difference between the times is within the provided tolerance.
   * @param {Number} t1 A time in milliseconds.
   * @param {Number} t2 A time in milliseconds.
   * @param {Number} tolerance The tolerance, in milliseconds.
   * @return {Boolean} True if the absolute time difference is within the tolerance, false
   *                   otherwise.
   */
  areTimesClose: function(t1, t2, tolerance) {
    return Math.abs(t1 - t2) <= tolerance;
  },

  /**
   * Get the next midnight for a date.
   * @param {Object} date The date object to check.
   * @return {Object} The Date object representing the next midnight.
   */
  getNextMidnight: function(date) {
    let nextMidnight = new Date(this.truncateToDays(date));
    nextMidnight.setDate(nextMidnight.getDate() + 1);
    return nextMidnight;
  },

  /**
   * Get the midnight which is closer to the provided date.
   * @param {Object} date The date object to check.
   * @param {Number} tolerance The tolerance within we find the closest midnight.
   * @return {Object} The Date object representing the closes midnight, or null if midnight
   *                  is not within the midnight tolerance.
   */
  getNearestMidnight: function(date, tolerance) {
    let lastMidnight = this.truncateToDays(date);
    if (this.areTimesClose(date.getTime(), lastMidnight.getTime(), tolerance)) {
      return lastMidnight;
    }

    const nextMidnightDate = this.getNextMidnight(date);
    if (this.areTimesClose(date.getTime(), nextMidnightDate.getTime(), tolerance)) {
      return nextMidnightDate;
    }
    return null;
  },

  generateUUID: function() {
    let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
    // strip {}
    return str.substring(1, str.length - 1);
  },

  /**
   * Find how many months passed between two dates.
   * @param {Object} aStartDate The starting date.
   * @param {Object} aEndDate The ending date.
   * @return {Integer} The number of months between the two dates.
   */
  getElapsedTimeInMonths: function(aStartDate, aEndDate) {
    return (aEndDate.getMonth() - aStartDate.getMonth())
           + 12 * (aEndDate.getFullYear() - aStartDate.getFullYear());
  },
};
