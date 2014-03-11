/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file provides common and shared functionality to facilitate
 * testing of the Crashes component (CrashManager.jsm).
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [
  "getManager",
  "sleep",
  "TestingCrashManager",
];

Cu.import("resource://gre/modules/CrashManager.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

this.sleep = function (wait) {
  let deferred = Promise.defer();

  setTimeout(() => {
    deferred.resolve();
  }, wait);

  return deferred.promise;
};

this.TestingCrashManager = function (options) {
  CrashManager.call(this, options);
}

this.TestingCrashManager.prototype = {
  __proto__: CrashManager.prototype,

  createDummyDump: function (submitted=false, date=new Date(), hr=false) {
    let uuid = Cc["@mozilla.org/uuid-generator;1"]
                .getService(Ci.nsIUUIDGenerator)
                .generateUUID()
                .toString();
    uuid = uuid.substring(1, uuid.length - 1);

    let path;
    let mode;
    if (submitted) {
      if (hr) {
        path = OS.Path.join(this._submittedDumpsDir, "bp-hr-" + uuid + ".txt");
      } else {
        path = OS.Path.join(this._submittedDumpsDir, "bp-" + uuid + ".txt");
      }
      mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR |
            OS.Constants.libc.S_IRGRP | OS.Constants.libc.S_IROTH;
    } else {
      path = OS.Path.join(this._pendingDumpsDir, uuid + ".dmp");
      mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
    }

    return Task.spawn(function* () {
      let f = yield OS.File.open(path, {create: true}, {unixMode: mode});
      yield f.setDates(date, date);
      yield f.close();
      dump("Created fake crash: " + path + "\n");

      return uuid;
    });
  },

  createIgnoredDumpFile: function (filename, submitted=false) {
    let path;
    if (submitted) {
      path = OS.Path.join(this._submittedDumpsDir, filename);
    } else {
      path = OS.Path.join(this._pendingDumpsDir, filename);
    }

    return Task.spawn(function* () {
      let mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
      yield OS.File.open(path, {create: true}, {unixMode: mode});
      dump ("Create ignored dump file: " + path + "\n");
    });
  },

  createEventsFile: function (filename, type, date, content, index=0) {
    let path = OS.Path.join(this._eventsDirs[index], filename);

    let data = type + "\n" +
               Math.floor(date.getTime() / 1000) + "\n" +
               content;
    let encoder = new TextEncoder();
    let array = encoder.encode(data);

    return Task.spawn(function* () {
      yield OS.File.writeAtomic(path, array);
      yield OS.File.setDates(path, date, date);
    });
  },

  /**
   * Overwrite event file handling to process our test file type.
   *
   * We can probably delete this once we have actual events defined.
   */
  _handleEventFilePayload: function (store, entry, type, date, payload) {
    if (type == "test.1") {
      if (payload == "malformed") {
        return this.EVENT_FILE_ERROR_MALFORMED;
      } else if (payload == "success") {
        return this.EVENT_FILE_SUCCESS;
      } else {
        return this.EVENT_FILE_ERROR_UNKNOWN_EVENT;
      }
    }

    return CrashManager.prototype._handleEventFilePayload.call(this,
                                                               store,
                                                               entry,
                                                               type,
                                                               date,
                                                               payload);
  },
};

let DUMMY_DIR_COUNT = 0;

this.getManager = function () {
  return Task.spawn(function* () {
    const dirMode = OS.Constants.libc.S_IRWXU;
    let baseFile = OS.Constants.Path.profileDir;

    function makeDir(create=true) {
      return Task.spawn(function* () {
        let path = OS.Path.join(baseFile, "dummy-dir-" + DUMMY_DIR_COUNT++);

        if (!create) {
          return path;
        }

        dump("Creating directory: " + path + "\n");
        yield OS.File.makeDir(path, {unixMode: dirMode});

        return path;
      });
    }

    let pendingD = yield makeDir();
    let submittedD = yield makeDir();
    let eventsD1 = yield makeDir();
    let eventsD2 = yield makeDir();

    // Store directory is created at run-time if needed. Ensure those code
    // paths are triggered.
    let storeD = yield makeDir(false);

    let m = new TestingCrashManager({
      pendingDumpsDir: pendingD,
      submittedDumpsDir: submittedD,
      eventsDirs: [eventsD1, eventsD2],
      storeDir: storeD,
    });

    return m;
  });
};
