/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm", this)
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

this.EXPORTED_SYMBOLS = [
  "CrashManager",
];

/**
 * A gateway to crash-related data.
 *
 * This type is generic and can be instantiated any number of times.
 * However, most applications will typically only have one instance
 * instantiated and that instance will point to profile and user appdata
 * directories.
 *
 * Instances are created by passing an object with properties.
 * Recognized properties are:
 *
 *   pendingDumpsDir (string) (required)
 *     Where dump files that haven't been uploaded are located.
 *
 *   submittedDumpsDir (string) (required)
 *     Where records of uploaded dumps are located.
 */
this.CrashManager = function (options) {
  for (let k of ["pendingDumpsDir", "submittedDumpsDir"]) {
    if (!(k in options)) {
      throw new Error("Required key not present in options: " + k);
    }
  }

  for (let [k, v] of Iterator(options)) {
    switch (k) {
      case "pendingDumpsDir":
        this._pendingDumpsDir = v;
        break;

      case "submittedDumpsDir":
        this._submittedDumpsDir = v;
        break;

      default:
        throw new Error("Unknown property in options: " + k);
    }
  }
};

this.CrashManager.prototype = Object.freeze({
  DUMP_REGEX: /^([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.dmp$/i,
  SUBMITTED_REGEX: /^bp-(?:hr-)?([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.txt$/i,

  /**
   * Obtain a list of all dumps pending upload.
   *
   * The returned value is a promise that resolves to an array of objects
   * on success. Each element in the array has the following properties:
   *
   *   id (string)
   *      The ID of the crash (a UUID).
   *
   *   path (string)
   *      The filename of the crash (<UUID.dmp>)
   *
   *   date (Date)
   *      When this dump was created
   *
   * The returned arry is sorted by the modified time of the file backing
   * the entry, oldest to newest.
   *
   * @return Promise<Array>
   */
  pendingDumps: function () {
    return this._getDirectoryEntries(this._pendingDumpsDir, this.DUMP_REGEX);
  },

  /**
   * Obtain a list of all dump files corresponding to submitted crashes.
   *
   * The returned value is a promise that resolves to an Array of
   * objects. Each object has the following properties:
   *
   *   path (string)
   *     The path of the file this entry comes from.
   *
   *   id (string)
   *     The crash UUID.
   *
   *   date (Date)
   *     The (estimated) date this crash was submitted.
   *
   * The returned array is sorted by the modified time of the file backing
   * the entry, oldest to newest.
   *
   * @return Promise<Array>
   */
  submittedDumps: function () {
    return this._getDirectoryEntries(this._submittedDumpsDir,
                                     this.SUBMITTED_REGEX);
  },

  /**
   * Helper to obtain all directory entries in a path that match a regexp.
   *
   * The resolved promise is an array of objects with the properties:
   *
   *   path -- String filename
   *   id -- regexp.match()[1] (likely the crash ID)
   *   date -- Date mtime of the file
   */
  _getDirectoryEntries: function (path, re) {
    return Task.spawn(function* () {
      try {
        yield OS.File.stat(path);
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          return [];
      }

      let it = new OS.File.DirectoryIterator(path);
      let entries = [];

      try {
        yield it.forEach((entry, index, it) => {
          if (entry.isDir) {
            return;
          }

          let match = re.exec(entry.name);
          if (!match) {
            return;
          }

          return OS.File.stat(entry.path).then((info) => {
            entries.push({
              path: entry.path,
              id: match[1],
              date: info.lastModificationDate,
            });
          });
        });
      } finally {
        it.close();
      }

      entries.sort((a, b) => { return a.date - b.date; });

      return entries;
    }.bind(this));
  },
});

let gCrashManager;

/**
 * Obtain the global CrashManager instance used by the running application.
 *
 * CrashManager is likely only ever instantiated once per application lifetime.
 * The main reason it's implemented as a reusable type is to facilitate testing.
 */
XPCOMUtils.defineLazyGetter(this.CrashManager, "Singleton", function () {
  Cu.reportError("CrashManager.Singleton accessed!");
  if (gCrashManager) {
    return gCrashManager;
  }

  let crPath = OS.Path.join(OS.Constants.Path.userApplicationDataDir,
                            "Crash Reports");
  gCrashManager = new CrashManager({
    pendingDumpsDir: OS.Path.join(crPath, "pending"),
    submittedDumpsDir: OS.Path.join(crPath, "submitted"),
  });

  return gCrashManager;
});
