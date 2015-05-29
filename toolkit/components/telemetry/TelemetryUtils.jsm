/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryUtils"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

this.TelemetryUtils = {
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
};
