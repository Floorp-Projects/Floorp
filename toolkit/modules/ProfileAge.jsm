/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ProfileAge"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm")
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");

/**
 * Profile access to times.json (eg, creation/reset time).
 * This is separate from the provider to simplify testing and enable extraction
 * to a shared location in the future.
 */
this.ProfileAge = function(profile, log) {
  this.profilePath = profile || OS.Constants.Path.profileDir;
  if (!this.profilePath) {
    throw new Error("No profile directory.");
  }
  if (!log) {
    log = Log.repository.getLogger("Toolkit.ProfileAge");
  }
  this._log = log;
}
this.ProfileAge.prototype = {
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
    function onSuccess(times) {
      if (times.created) {
        return times.created;
      }
      return onFailure.call(this, null, times);
    }

    function onFailure(err, times) {
      return this.computeAndPersistCreated(times)
                 .then(function onSuccess(created) {
                         return created;
                       });
    }

    return this.getTimes()
               .then(onSuccess.bind(this),
                     onFailure.bind(this));
  },

  /**
   * Explicitly make `file`, a filename, a full path
   * relative to our profile path.
   */
  getPath(file) {
    return OS.Path.join(this.profilePath, file);
  },

  /**
   * Return a promise which resolves to the JSON contents
   * of the time file, using the already read value if possible.
   */
  getTimes(file = "times.json") {
    if (this._times) {
      return Promise.resolve(this._times);
    }
    return this.readTimes(file).then(
      times => {
        return this.times = times || {};
      }
    );
  },

  /**
   * Return a promise which resolves to the JSON contents
   * of the time file in this accessor's profile.
   */
  readTimes(file = "times.json") {
    return CommonUtils.readJSON(this.getPath(file));
  },

  /**
   * Return a promise representing the writing of `contents`
   * to `file` in the specified profile.
   */
  writeTimes(contents, file = "times.json") {
    return CommonUtils.writeJSON(contents, this.getPath(file));
  },

  /**
   * Merge existing contents with a 'created' field, writing them
   * to the specified file. Promise, naturally.
   */
  computeAndPersistCreated(existingContents, file = "times.json") {
    let path = this.getPath(file);
    function onOldest(oldest) {
      let contents = existingContents || {};
      contents.created = oldest;
      this._times = contents;
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
  getOldestProfileTimestamp() {
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
        self._log.debug("Stat failure", e);
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

  /**
   * Record (and persist) when a profile reset happened.  We just store a
   * single value - the timestamp of the most recent reset - but there is scope
   * to keep a list of reset times should our health-reporter successor
   * be able to make use of that.
   * Returns a promise that is resolved once the file has been written.
   */
  recordProfileReset(time = Date.now(), file = "times.json") {
    return this.getTimes(file).then(
      times => {
        times.reset = time;
        return this.writeTimes(times, file);
      }
    );
  },

  /* Returns a promise that resolves to the time the profile was reset,
   * or undefined if not recorded.
   */
  get reset() {
    return this.getTimes().then(
      times => times.reset
    );
  },
}
