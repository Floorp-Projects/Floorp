/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ProfileAge"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const FILE_TIMES = "times.json";

/**
 * Calculate how many days passed between two dates.
 * @param {Object} aStartDate The starting date.
 * @param {Object} aEndDate The ending date.
 * @return {Integer} The number of days between the two dates.
 */
function getElapsedTimeInDays(aStartDate, aEndDate) {
  return TelemetryUtils.millisecondsToDays(aEndDate - aStartDate);
}

/**
 * Traverse the contents of the profile directory, finding the oldest file
 * and returning its creation timestamp.
 */
async function getOldestProfileTimestamp(profilePath, log) {
  let start = Date.now();
  let oldest = start + 1000;
  let iterator = new OS.File.DirectoryIterator(profilePath);
  log.debug("Iterating over profile " + profilePath);
  if (!iterator) {
    throw new Error(
      "Unable to fetch oldest profile entry: no profile iterator."
    );
  }

  Services.telemetry.scalarAdd("telemetry.profile_directory_scans", 1);
  let histogram = Services.telemetry.getHistogramById(
    "PROFILE_DIRECTORY_FILE_AGE"
  );

  try {
    await iterator.forEach(async entry => {
      try {
        let info = await OS.File.stat(entry.path);

        // OS.File doesn't seem to be behaving. See Bug 827148.
        // Let's do the best we can. This whole function is defensive.
        let date = info.winBirthDate || info.macBirthDate;
        if (!date || !date.getTime()) {
          // OS.File will only return file creation times of any kind on Mac
          // and Windows, where birthTime is defined.
          // That means we're unable to function on Linux, so we use mtime
          // instead.
          log.debug("No birth date. Using mtime.");
          date = info.lastModificationDate;
        }

        if (date) {
          let timestamp = date.getTime();
          // Get the age relative to now.
          // We don't care about dates in the future.
          let age_in_days = Math.max(0, getElapsedTimeInDays(timestamp, start));
          histogram.add(age_in_days);

          log.debug("Using date: " + entry.path + " = " + date);
          if (timestamp < oldest) {
            oldest = timestamp;
          }
        }
      } catch (e) {
        // Never mind.
        log.debug("Stat failure", e);
      }
    });
  } catch (reason) {
    throw new Error("Unable to fetch oldest profile entry: " + reason);
  } finally {
    iterator.close();
  }

  return oldest;
}

/**
 * Profile access to times.json (eg, creation/reset time).
 * This is separate from the provider to simplify testing and enable extraction
 * to a shared location in the future.
 */
class ProfileAgeImpl {
  constructor(profile, times) {
    this.profilePath = profile || OS.Constants.Path.profileDir;
    this._times = times;
    this._log = Log.repository.getLogger("Toolkit.ProfileAge");

    if ("firstUse" in this._times && this._times.firstUse === null) {
      // Indicates that this is a new profile that needs a first use timestamp.
      this._times.firstUse = Date.now();
      this.writeTimes();
    }
  }

  /**
   * There are two ways we can get our creation time:
   *
   * 1. From the on-disk JSON file.
   * 2. By calculating it from the filesystem.
   *
   * If we have to calculate, we write out the file; if we have
   * to touch the file, we persist in-memory.
   *
   * @return a promise that resolves to the profile's creation time.
   */
  get created() {
    // This can be an expensive operation so make sure we only do it once.
    if (this._created) {
      return this._created;
    }

    if (!this._times.created) {
      this._created = this.computeAndPersistCreated();
    } else {
      this._created = Promise.resolve(this._times.created);
    }

    return this._created;
  }

  /**
   * Returns a promise to the time of first use of the profile. This may be
   * undefined if the first use time is unknown.
   */
  get firstUse() {
    if ("firstUse" in this._times) {
      return Promise.resolve(this._times.firstUse);
    }
    return Promise.resolve(undefined);
  }

  /**
   * Return a promise representing the writing the current times to the profile.
   */
  writeTimes() {
    return CommonUtils.writeJSON(
      this._times,
      OS.Path.join(this.profilePath, FILE_TIMES)
    );
  }

  /**
   * Calculates the created time by scanning the profile directory, sets it in
   * the current set of times and persists it to the profile. Returns a promise
   * that resolves when all of that is complete.
   */
  async computeAndPersistCreated() {
    let oldest = await getOldestProfileTimestamp(this.profilePath, this._log);
    this._times.created = oldest;
    Services.telemetry.scalarSet(
      "telemetry.profile_directory_scan_date",
      TelemetryUtils.millisecondsToDays(Date.now())
    );
    await this.writeTimes();
    return oldest;
  }

  /**
   * Record (and persist) when a profile reset happened.  We just store a
   * single value - the timestamp of the most recent reset - but there is scope
   * to keep a list of reset times should our health-reporter successor
   * be able to make use of that.
   * Returns a promise that is resolved once the file has been written.
   */
  recordProfileReset(time = Date.now()) {
    this._times.reset = time;
    return this.writeTimes();
  }

  /* Returns a promise that resolves to the time the profile was reset,
   * or undefined if not recorded.
   */
  get reset() {
    if ("reset" in this._times) {
      return Promise.resolve(this._times.reset);
    }
    return Promise.resolve(undefined);
  }
}

// A Map from profile directory to a promise that resolves to the ProfileAgeImpl.
const PROFILES = new Map();

async function initProfileAge(profile) {
  let timesPath = OS.Path.join(profile, FILE_TIMES);

  try {
    let times = await CommonUtils.readJSON(timesPath);
    return new ProfileAgeImpl(profile, times || {});
  } catch (e) {
    // Indicates that the file was missing or broken. In this case we want to
    // record the first use time as now. The constructor will set this and write
    // times.json
    return new ProfileAgeImpl(profile, { firstUse: null });
  }
}

/**
 * Returns a promise that resolves to an instance of ProfileAgeImpl. Will always
 * return the same instance for every call for the same profile.
 *
 * @param {string} profile The path to the profile directory.
 * @return {Promise<ProfileAgeImpl>} Resolves to the ProfileAgeImpl.
 */
function ProfileAge(profile = OS.Constants.Path.profileDir) {
  if (PROFILES.has(profile)) {
    return PROFILES.get(profile);
  }

  let promise = initProfileAge(profile);
  PROFILES.set(profile, promise);
  return promise;
}
