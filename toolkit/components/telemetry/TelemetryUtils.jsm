/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "TelemetryUtils"
];

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils",
                               "resource://gre/modules/UpdateUtils.jsm");

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

const IS_CONTENT_PROCESS = (function() {
  // We cannot use Services.appinfo here because in telemetry xpcshell tests,
  // appinfo is initially unavailable, and becomes available only later on.
  // eslint-disable-next-line mozilla/use-services
  let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  return runtime.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
})();

/**
 * When reflecting a histogram into JS, Telemetry hands us an object
 * with the following properties:
 *
 * - min, max, histogram_type, sum, sum_squares_{lo,hi}: simple integers;
 * - counts: array of counts for histogram buckets;
 * - ranges: array of calculated bucket sizes.
 *
 * This format is not straightforward to read and potentially bulky
 * with lots of zeros in the counts array.  Packing histograms makes
 * raw histograms easier to read and compresses the data a little bit.
 *
 * Returns an object:
 * { range: [min, max], bucket_count: <number of buckets>,
 *   histogram_type: <histogram_type>, sum: <sum>,
 *   values: { bucket1: count1, bucket2: count2, ... } }
 */
function packHistogram(hgram) {
  let r = hgram.ranges;
  let c = hgram.counts;
  let retgram = {
    range: [r[1], r[r.length - 1]],
    bucket_count: r.length,
    histogram_type: hgram.histogram_type,
    values: {},
    sum: hgram.sum
  };

  let first = true;
  let last = 0;

  for (let i = 0; i < c.length; i++) {
    let value = c[i];
    if (!value)
      continue;

    // add a lower bound
    if (i && first) {
      retgram.values[r[i - 1]] = 0;
    }
    first = false;
    last = i + 1;
    retgram.values[r[i]] = value;
  }

  // add an upper bound
  if (last && last < c.length)
    retgram.values[r[last]] = 0;
  return retgram;
}

var TelemetryUtils = {
  Preferences: Object.freeze({
    // General Preferences
    ArchiveEnabled: "toolkit.telemetry.archive.enabled",
    CachedClientId: "toolkit.telemetry.cachedClientID",
    FirstRun: "toolkit.telemetry.reportingpolicy.firstRun",
    FirstShutdownPingEnabled: "toolkit.telemetry.firstShutdownPing.enabled",
    HealthPingEnabled: "toolkit.telemetry.healthping.enabled",
    HybridContentEnabled: "toolkit.telemetry.hybridContent.enabled",
    OverrideOfficialCheck: "toolkit.telemetry.send.overrideOfficialCheck",
    OverridePreRelease: "toolkit.telemetry.testing.overridePreRelease",
    OverrideUpdateChannel: "toolkit.telemetry.overrideUpdateChannel",
    Server: "toolkit.telemetry.server",
    ShutdownPingSender: "toolkit.telemetry.shutdownPingSender.enabled",
    ShutdownPingSenderFirstSession: "toolkit.telemetry.shutdownPingSender.enabledFirstSession",
    TelemetryEnabled: "toolkit.telemetry.enabled",
    Unified: "toolkit.telemetry.unified",
    UpdatePing: "toolkit.telemetry.updatePing.enabled",
    NewProfilePingEnabled: "toolkit.telemetry.newProfilePing.enabled",
    NewProfilePingDelay: "toolkit.telemetry.newProfilePing.delay",
    PreviousBuildID: "toolkit.telemetry.previousBuildID",

    // Event Ping Preferences
    EventPingEnabled: "toolkit.telemetry.eventping.enabled",
    EventPingEventLimit: "toolkit.telemetry.eventping.eventLimit",
    EventPingMinimumFrequency: "toolkit.telemetry.eventping.minimumFrequency",
    EventPingMaximumFrequency: "toolkit.telemetry.eventping.maximumFrequency",

    // Log Preferences
    LogLevel: "toolkit.telemetry.log.level",
    LogDump: "toolkit.telemetry.log.dump",

    // Data reporting Preferences
    AcceptedPolicyDate: "datareporting.policy.dataSubmissionPolicyNotifiedTime",
    AcceptedPolicyVersion: "datareporting.policy.dataSubmissionPolicyAcceptedVersion",
    BypassNotification: "datareporting.policy.dataSubmissionPolicyBypassNotification",
    CurrentPolicyVersion: "datareporting.policy.currentPolicyVersion",
    DataSubmissionEnabled: "datareporting.policy.dataSubmissionEnabled",
    FhrUploadEnabled: "datareporting.healthreport.uploadEnabled",
    MinimumPolicyVersion: "datareporting.policy.minimumPolicyVersion",
    FirstRunURL: "datareporting.policy.firstRunURL",
  }),

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
    return Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED, false) === true;
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
   * Takes a date and returns it truncated to a date with daily precision.
   */
  truncateToDays(date) {
    return new Date(date.getFullYear(),
                    date.getMonth(),
                    date.getDate(),
                    0, 0, 0, 0);
  },

  /**
   * Takes a date and returns it truncated to a date with hourly precision.
   */
  truncateToHours(date) {
    return new Date(date.getFullYear(),
                    date.getMonth(),
                    date.getDate(),
                    date.getHours(),
                    0, 0, 0);
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
    function padNumber(number, length) {
      return number.toString().padStart(length, "0");
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

  /**
   * @returns {number} The monotonic time since the process start
   * or (non-monotonic) Date value if this fails back.
   */
  monotonicNow() {
    try {
      return Services.telemetry.msSinceProcessStart();
    } catch (ex) {
      return Date.now();
    }
  },

  /**
   * Set the Telemetry core recording flag for Unified Telemetry.
   */
  setTelemetryRecordingFlags() {
    // Enable extended Telemetry on pre-release channels and disable it
    // on Release/ESR.
    let prereleaseChannels = ["nightly", "aurora", "beta"];
    if (!AppConstants.MOZILLA_OFFICIAL) {
      // Turn extended telemetry for local developer builds.
      prereleaseChannels.push("default");
    }
    const isPrereleaseChannel =
      prereleaseChannels.includes(AppConstants.MOZ_UPDATE_CHANNEL);
    const isReleaseCandidateOnBeta =
      AppConstants.MOZ_UPDATE_CHANNEL === "release" &&
      Services.prefs.getCharPref("app.update.channel", null) === "beta";
    Services.telemetry.canRecordBase = true;
    Services.telemetry.canRecordExtended = isPrereleaseChannel ||
      isReleaseCandidateOnBeta ||
      Services.prefs.getBoolPref(this.Preferences.OverridePreRelease, false);
  },

  /**
   * Converts histograms from the raw to the packed format.
   * This additionally filters TELEMETRY_TEST_ histograms.
   *
   * @param {Object} snapshot - The histogram snapshot.
   * @param {Boolean} [testingMode=false] - Whether or not testing histograms
   *        should be filtered.
   * @returns {Object}
   *
   * {
   *  "<process>": {
   *    "<histogram>": {
   *      range: [min, max],
   *      bucket_count: <number of buckets>,
   *      histogram_type: <histogram_type>,
   *      sum: <sum>,
   *      values: { bucket1: count1, bucket2: count2, ... }
   *    },
   *   ..
   *   },
   *  ..
   * }
   */
  packHistograms(snapshot, testingMode = false) {
    let ret = {};

    for (let [process, histograms] of Object.entries(snapshot)) {
      ret[process] = {};
      for (let [name, value] of Object.entries(histograms)) {
        if (testingMode || !name.startsWith("TELEMETRY_TEST_")) {
          ret[process][name] = packHistogram(value);
        }
      }
    }

    return ret;
  },

  /**
   * Converts keyed histograms from the raw to the packed format.
   * This additionally filters TELEMETRY_TEST_ histograms and skips
   * empty keyed histograms.
   *
   * @param {Object} snapshot - The keyed histogram snapshot.
   * @param {Boolean} [testingMode=false] - Whether or not testing histograms should
   *        be filtered.
   * @returns {Object}
   *
   * {
   *  "<process>": {
   *    "<histogram>": {
   *      "<key>": {
   *        range: [min, max],
   *        bucket_count: <number of buckets>,
   *        histogram_type: <histogram_type>,
   *        sum: <sum>,
   *        values: { bucket1: count1, bucket2: count2, ... }
   *      },
   *      ..
   *    },
   *   ..
   *   },
   *  ..
   * }
   */
  packKeyedHistograms(snapshot, testingMode = false) {
    let ret = {};

    for (let [process, histograms] of Object.entries(snapshot)) {
      ret[process] = {};
      for (let [name, value] of Object.entries(histograms)) {
        if (testingMode || !name.startsWith("TELEMETRY_TEST_")) {
          let keys = Object.keys(value);
          if (keys.length == 0) {
            // Skip empty keyed histogram
            continue;
          }
          ret[process][name] = {};
          for (let [key, hgram] of Object.entries(value)) {
            ret[process][name][key] = packHistogram(hgram);
          }
        }
      }
    }

    return ret;
  },

  /**
   * @returns {string} The name of the update channel to report
   * in telemetry.
   * By default, this is the same as the name of the channel that
   * the browser uses to download its updates. However in certain
   * situations, a single update channel provides multiple (distinct)
   * build types, that need to be distinguishable on Telemetry.
   */
  getUpdateChannel() {
    let overrideChannel = Services.prefs.getCharPref(this.Preferences.OverrideUpdateChannel, undefined);
    if (overrideChannel) {
      return overrideChannel;
    }

    return UpdateUtils.getUpdateChannel(false);
  },
};
