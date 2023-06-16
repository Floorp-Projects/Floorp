/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file provides common and shared functionality to facilitate
 * testing of the Crashes component (CrashManager.sys.mjs).
 */

import { CrashManager } from "resource://gre/modules/CrashManager.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "resource://gre/modules/Log.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

var loggingConfigured = false;

export var configureLogging = function () {
  if (loggingConfigured) {
    return;
  }

  let log = lazy.Log.repository.getLogger("Crashes.CrashManager");
  log.level = lazy.Log.Level.All;
  let appender = new lazy.Log.DumpAppender();
  appender.level = lazy.Log.Level.All;
  log.addAppender(appender);
  loggingConfigured = true;
};

export var sleep = function (wait) {
  return new Promise(resolve => {
    lazy.setTimeout(() => {
      resolve();
    }, wait);
  });
};

export var TestingCrashManager = function (options) {
  CrashManager.call(this, options);
};

TestingCrashManager.prototype = {
  createDummyDump(submitted = false, date = new Date(), hr = false) {
    let uuid = Services.uuid.generateUUID().toString();
    uuid = uuid.substring(1, uuid.length - 1);

    let path;
    let mode;
    if (submitted) {
      if (hr) {
        path = PathUtils.join(
          this._submittedDumpsDir,
          "bp-hr-" + uuid + ".txt"
        );
      } else {
        path = PathUtils.join(this._submittedDumpsDir, "bp-" + uuid + ".txt");
      }
      mode = 0o644;
    } else {
      path = PathUtils.join(this._pendingDumpsDir, uuid + ".dmp");
      mode = 0o600;
    }

    return (async function () {
      await IOUtils.writeUTF8(path, "");
      await IOUtils.setPermissions(path, mode);
      await IOUtils.setModificationTime(path, date.valueOf());
      await IOUtils.setAccessTime(path, date.valueOf());
      dump(`Created fake crash: ${path}\n`);

      return uuid;
    })();
  },

  createIgnoredDumpFile(filename, submitted = false) {
    let path;
    if (submitted) {
      path = PathUtils.join(this._submittedDumpsDir, filename);
    } else {
      path = PathUtils.join(this._pendingDumpsDir, filename);
    }

    return (async function () {
      let mode = 0o600;
      await IOUtils.writeUTF8(path, "");
      await IOUtils.setPermissions(path, mode);
      dump(`Create ignored dump file: ${path}\n`);
    })();
  },

  createEventsFile(filename, type, date, id, content, index = 0) {
    let path = PathUtils.join(this._eventsDirs[index], filename);
    let dateInSecs = Math.floor(date.getTime() / 1000);
    let data = type + "\n" + dateInSecs + "\n" + id + "\n" + content;

    return (async function () {
      await IOUtils.writeUTF8(path, data);
      await IOUtils.setModificationTime(path, date.valueOf());
      await IOUtils.setAccessTime(path, date.valueOf());
    })();
  },

  deleteEventsDirs() {
    let dirs = this._eventsDirs;

    return (async function () {
      for (let dir of dirs) {
        await IOUtils.remove(dir, { recursive: true });
      }
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
Object.setPrototypeOf(TestingCrashManager.prototype, CrashManager.prototype);

var DUMMY_DIR_COUNT = 0;

export var getManager = function () {
  return (async function () {
    const dirMode = 0o700;
    let baseFile = PathUtils.profileDir;

    function makeDir(create = true) {
      return (async function () {
        let path = PathUtils.join(baseFile, "dummy-dir-" + DUMMY_DIR_COUNT++);

        if (!create) {
          return path;
        }

        dump("Creating directory: " + path + "\n");
        await IOUtils.makeDirectory(path, { permissions: dirMode });

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
