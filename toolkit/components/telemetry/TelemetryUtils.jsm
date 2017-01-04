/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryUtils"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm", this);

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

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
   * Returns the state of the Telemetry enabled preference, making sure
   * it correctly evaluates to a boolean type.
   */
  get isTelemetryEnabled() {
    return Preferences.get(PREF_TELEMETRY_ENABLED, false) === true;
  },

  /**
   * Turn a millisecond timestamp into a day timestamp.
   *
   * @param aMsec A number of milliseconds since Unix epoch.
   * @return The number of whole days since Unix epoch.
   */
  millisecondsToDays(aMsec) {
    return Math.floor(aMsec / MILLISECONDS_PER_DAY);
  },

  /**
   * Takes a date and returns it trunctated to a date with daily precision.
   */
  truncateToDays(date) {
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
  areTimesClose(t1, t2, tolerance) {
    return Math.abs(t1 - t2) <= tolerance;
  },

  /**
   * Get the next midnight for a date.
   * @param {Object} date The date object to check.
   * @return {Object} The Date object representing the next midnight.
   */
  getNextMidnight(date) {
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
  getNearestMidnight(date, tolerance) {
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

  generateUUID() {
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
  getElapsedTimeInMonths(aStartDate, aEndDate) {
    return (aEndDate.getMonth() - aStartDate.getMonth())
           + 12 * (aEndDate.getFullYear() - aStartDate.getFullYear());
  },

  /**
   * Date.toISOString() gives us UTC times, this gives us local times in
   * the ISO date format. See http://www.w3.org/TR/NOTE-datetime
   * @param {Object} date The input date.
   * @return {String} The local time ISO string.
   */
  toLocalTimeISOString(date) {
    function padNumber(number, places) {
      number = number.toString();
      while (number.length < places) {
        number = "0" + number;
      }
      return number;
    }

    let sign = (n) => n >= 0 ? "+" : "-";
    // getTimezoneOffset counter-intuitively returns -60 for UTC+1.
    let tzOffset = -date.getTimezoneOffset();

    // YYYY-MM-DDThh:mm:ss.sTZD (eg 1997-07-16T19:20:30.45+01:00)
    return padNumber(date.getFullYear(), 4)
      + "-" + padNumber(date.getMonth() + 1, 2)
      + "-" + padNumber(date.getDate(), 2)
      + "T" + padNumber(date.getHours(), 2)
      + ":" + padNumber(date.getMinutes(), 2)
      + ":" + padNumber(date.getSeconds(), 2)
      + "." + date.getMilliseconds()
      + sign(tzOffset) + padNumber(Math.floor(Math.abs(tzOffset / 60)), 2)
      + ":" + padNumber(Math.abs(tzOffset % 60), 2);
  },
};
