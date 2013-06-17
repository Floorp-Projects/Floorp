/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

#ifndef MERGED_COMPARTMENT

this.EXPORTED_SYMBOLS = [
  "ProfileCreationTimeAccessor",
  "ProfileMetadataProvider",
];

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

Cu.import("resource://gre/modules/Metrics.jsm");

#endif

const DEFAULT_PROFILE_MEASUREMENT_NAME = "age";
const REQUIRED_UINT32_TYPE = {type: "TYPE_UINT32"};

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm")
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");

// Profile creation time access.
// This is separate from the provider to simplify testing and enable extraction
// to a shared location in the future.
function ProfileCreationTimeAccessor(profile, log) {
  this.profilePath = profile || OS.Constants.Path.profileDir;
  if (!this.profilePath) {
    throw new Error("No profile directory.");
  }
  this._log = log || {"debug": function (s) { dump(s + "\n"); }};
}
ProfileCreationTimeAccessor.prototype = {
  /**
   * There are three ways we can get our creation time:
   *
   * 1. From our own saved value (to avoid redundant work).
   * 2. From the on-disk JSON file.
   * 3. By calculating it from the filesystem.
   *
   * If we have to calculate, we write out the file; if we have
   * to touch the file, we persist in-memory.
   *
   * @return a promise that resolves to the profile's creation time.
   */
  get created() {
    if (this._created) {
      return Promise.resolve(this._created);
    }

    function onSuccess(times) {
      if (times && times.created) {
        return this._created = times.created;
      }
      return onFailure.call(this, null, times);
    }

    function onFailure(err, times) {
      return this.computeAndPersistTimes(times)
                 .then(function onSuccess(created) {
                         return this._created = created;
                       }.bind(this));
    }

    return this.readTimes()
               .then(onSuccess.bind(this),
                     onFailure.bind(this));
  },

  /**
   * Explicitly make `file`, a filename, a full path
   * relative to our profile path.
   */
  getPath: function (file) {
    return OS.Path.join(this.profilePath, file);
  },

  /**
   * Return a promise which resolves to the JSON contents
   * of the time file in this accessor's profile.
   */
  readTimes: function (file="times.json") {
    return CommonUtils.readJSON(this.getPath(file));
  },

  /**
   * Return a promise representing the writing of `contents`
   * to `file` in the specified profile.
   */
  writeTimes: function (contents, file="times.json") {
    return CommonUtils.writeJSON(contents, this.getPath(file));
  },

  /**
   * Merge existing contents with a 'created' field, writing them
   * to the specified file. Promise, naturally.
   */
  computeAndPersistTimes: function (existingContents, file="times.json") {
    let path = this.getPath(file);
    function onOldest(oldest) {
      let contents = existingContents || {};
      contents.created = oldest;
      return this.writeTimes(contents, path)
                 .then(function onSuccess() {
                   return oldest;
                 });
    }

    return this.getOldestProfileTimestamp()
               .then(onOldest.bind(this));
  },

  /**
   * Traverse the contents of the profile directory, finding the oldest file
   * and returning its creation timestamp.
   */
  getOldestProfileTimestamp: function () {
    let self = this;
    let oldest = Date.now() + 1000;
    let iterator = new OS.File.DirectoryIterator(this.profilePath);
    self._log.debug("Iterating over profile " + this.profilePath);
    if (!iterator) {
      throw new Error("Unable to fetch oldest profile entry: no profile iterator.");
    }

    function onEntry(entry) {
      function onStatSuccess(info) {
        // OS.File doesn't seem to be behaving. See Bug 827148.
        // Let's do the best we can. This whole function is defensive.
        let date = info.winBirthDate || info.macBirthDate;
        if (!date || !date.getTime()) {
          // OS.File will only return file creation times of any kind on Mac
          // and Windows, where birthTime is defined.
          // That means we're unable to function on Linux, so we use mtime
          // instead.
          self._log.debug("No birth date. Using mtime.");
          date = info.lastModificationDate;
        }

        if (date) {
          let timestamp = date.getTime();
          self._log.debug("Using date: " + entry.path + " = " + date);
          if (timestamp < oldest) {
            oldest = timestamp;
          }
        }
      }

      function onStatFailure(e) {
        // Never mind.
        self._log.debug("Stat failure: " + CommonUtils.exceptionStr(e));
      }

      return OS.File.stat(entry.path)
                    .then(onStatSuccess, onStatFailure);
    }

    let promise = iterator.forEach(onEntry);

    function onSuccess() {
      iterator.close();
      return oldest;
    }

    function onFailure(reason) {
      iterator.close();
      throw new Error("Unable to fetch oldest profile entry: " + reason);
    }

    return promise.then(onSuccess, onFailure);
  },
}

/**
 * Measurements pertaining to the user's profile.
 */
function ProfileMetadataMeasurement() {
  Metrics.Measurement.call(this);
}
ProfileMetadataMeasurement.prototype = {
  __proto__: Metrics.Measurement.prototype,

  name: DEFAULT_PROFILE_MEASUREMENT_NAME,
  version: 1,

  fields: {
    // Profile creation date. Number of days since Unix epoch.
    profileCreation: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
  },
};

/**
 * Turn a millisecond timestamp into a day timestamp.
 *
 * @param msec a number of milliseconds since epoch.
 * @return the number of whole days denoted by the input.
 */
function truncate(msec) {
  return Math.floor(msec / MILLISECONDS_PER_DAY);
}

/**
 * A Metrics.Provider for profile metadata, such as profile creation time.
 */
function ProfileMetadataProvider() {
  Metrics.Provider.call(this);
}
ProfileMetadataProvider.prototype = {
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.profile",

  measurementTypes: [ProfileMetadataMeasurement],

  pullOnly: true,

  getProfileCreationDays: function () {
    let accessor = new ProfileCreationTimeAccessor(null, this._log);

    return accessor.created
                   .then(truncate);
  },

  collectConstantData: function () {
    let m = this.getMeasurement(DEFAULT_PROFILE_MEASUREMENT_NAME, 1);

    return Task.spawn(function collectConstant() {
      let createdDays = yield this.getProfileCreationDays();

      yield this.enqueueStorageOperation(function storeDays() {
        return m.setLastNumeric("profileCreation", createdDays);
      });
    }.bind(this));
  },
};

