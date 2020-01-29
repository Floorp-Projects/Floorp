/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file provides common and shared functionality to facilitate
 * testing of the Crashes component (CrashManager.jsm).
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "configureLogging",
  "getManager",
  "sleep",
  "TestingCrashManager",
];

ChromeUtils.import("resource://gre/modules/CrashManager.jsm", this);
ChromeUtils.import("resource://gre/modules/Log.jsm", this);
ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);

var loggingConfigured = false;

var configureLogging = function() {
  if (loggingConfigured) {
    return;
  }

  let log = Log.repository.getLogger("Crashes.CrashManager");
  log.level = Log.Level.All;
  let appender = new Log.DumpAppender();
  appender.level = Log.Level.All;
  log.addAppender(appender);
  loggingConfigured = true;
};

var sleep = function(wait) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve();
    }, wait);
  });
};

var TestingCrashManager = function(options) {
  CrashManager.call(this, options);
};

TestingCrashManager.prototype = {
  __proto__: CrashManager.prototype,

  createDummyDump(submitted = false, date = new Date(), hr = false) {
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
      mode =
        OS.Constants.libc.S_IRUSR |
        OS.Constants.libc.S_IWUSR |
        OS.Constants.libc.S_IRGRP |
        OS.Constants.libc.S_IROTH;
    } else {
      path = OS.Path.join(this._pendingDumpsDir, uuid + ".dmp");
      mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
    }

    return (async function() {
      let f = await OS.File.open(path, { create: true }, { unixMode: mode });
      await f.setDates(date, date);
      await f.close();
      dump("Created fake crash: " + path + "\n");

      return uuid;
    })();
  },

  createIgnoredDumpFile(filename, submitted = false) {
    let path;
    if (submitted) {
      path = OS.Path.join(this._submittedDumpsDir, filename);
    } else {
      path = OS.Path.join(this._pendingDumpsDir, filename);
    }

    return (async function() {
      let mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
      await OS.File.open(path, { create: true }, { unixMode: mode });
      dump("Create ignored dump file: " + path + "\n");
    })();
  },

  createEventsFile(filename, type, date, id, content, index = 0) {
    let path = OS.Path.join(this._eventsDirs[index], filename);
    let dateInSecs = Math.floor(date.getTime() / 1000);
    let data = type + "\n" + dateInSecs + "\n" + id + "\n" + content;
    let encoder = new TextEncoder();
    let array = encoder.encode(data);

    return (async function() {
      await OS.File.writeAtomic(path, array);
      await OS.File.setDates(path, date, date);
    })();
  },

  /**
   * Overwrite event file handling to process our test file type.
   *
   * We can probably delete this once we have actual events defined.
   */
  _handleEventFilePayload(store, entry, type, date, payload) {
    if (type == "test.1") {
      if (payload == "malformed") {
        return this.EVENT_FILE_ERROR_MALFORMED;
      } else if (payload == "success") {
        return this.EVENT_FILE_SUCCESS;
      }
      return this.EVENT_FILE_ERROR_UNKNOWN_EVENT;
    }

    return CrashManager.prototype._handleEventFilePayload.call(
      this,
      store,
      entry,
      type,
      date,
      payload
    );
  },
};

var DUMMY_DIR_COUNT = 0;

var getManager = function() {
  return (async function() {
    const dirMode = OS.Constants.libc.S_IRWXU;
    let baseFile = OS.Constants.Path.profileDir;

    function makeDir(create = true) {
      return (async function() {
        let path = OS.Path.join(baseFile, "dummy-dir-" + DUMMY_DIR_COUNT++);

        if (!create) {
          return path;
        }

        dump("Creating directory: " + path + "\n");
        await OS.File.makeDir(path, { unixMode: dirMode });

        return path;
      })();
    }

    let pendingD = await makeDir();
    let submittedD = await makeDir();
    let eventsD1 = await makeDir();
    let eventsD2 = await makeDir();

    // Store directory is created at run-time if needed. Ensure those code
    // paths are triggered.
    let storeD = await makeDir(false);

    let m = new TestingCrashManager({
      pendingDumpsDir: pendingD,
      submittedDumpsDir: submittedD,
      eventsDirs: [eventsD1, eventsD2],
      storeDir: storeD,
      telemetryStoreSizeKey: "CRASH_STORE_COMPRESSED_BYTES",
    });

    return m;
  })();
};
