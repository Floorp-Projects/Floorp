/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Sqlite } = ChromeUtils.import("resource://gre/modules/Sqlite.jsm");
const { requestIdleCallback } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

const SCHEMA_VERSION = 1;

XPCOMUtils.defineLazyGetter(this, "DB_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "protections.sqlite");
});

ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);

/**
 * All SQL statements should be defined here.
 */
const SQL = {
  createEvents:
    "CREATE TABLE events (" +
    "id INTEGER PRIMARY KEY, " +
    "type INTEGER NOT NULL, " +
    "count INTEGER NOT NULL, " +
    "timestamp DATE " +
    ");",

  addEvent:
    "INSERT INTO events (type, count, timestamp) " +
    "VALUES (:type, 1, date(:date));",

  incrementEvent: "UPDATE events SET count = count + 1 WHERE id = :id;",

  selectByTypeAndDate:
    "SELECT * FROM events " +
    "WHERE type = :type " +
    "AND timestamp = date(:date);",

  deleteEventsRecords: "DELETE FROM events;",

  removeRecordsSince: "DELETE FROM events WHERE timestamp >= date(:date);",
};

/**
 * Creates the database schema.
 */
async function createDatabase(db) {
  await db.execute(SQL.createEvents);
}

async function removeAllRecords(db) {
  await db.execute(SQL.deleteEventsRecords);
}

async function removeRecordsSince(db, date) {
  await db.execute(SQL.removeRecordsSince, { date });
}

this.TrackingDBService = function() {
  this._initPromise = this._initialize();
};

TrackingDBService.prototype = {
  classID: Components.ID("{3c9c43b6-09eb-4ed2-9b87-e29f4221eef0}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsITrackingDBService]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(TrackingDBService),
  // This is the connection to the database, opened in _initialize and closed on _shutdown.
  _db: null,

  async ensureDB() {
    await this._initPromise;
    return this._db;
  },

  async _initialize() {
    let db = await Sqlite.openConnection({ path: DB_PATH });

    try {
      // Check to see if we need to perform any migrations.
      let dbVersion = parseInt(await db.getSchemaVersion());

      // getSchemaVersion() returns a 0 int if the schema
      // version is undefined.
      if (dbVersion === 0) {
        await createDatabase(db);
      } else if (dbVersion < SCHEMA_VERSION) {
        // TODO
        // await upgradeDatabase(db, dbVersion, SCHEMA_VERSION);
      }

      await db.setSchemaVersion(SCHEMA_VERSION);
    } catch (e) {
      // Close the DB connection before passing the exception to the consumer.
      await db.close();
      throw e;
    }

    AsyncShutdown.profileBeforeChange.addBlocker(
      "TrackingDBService: Shutting down the content blocking database.",
      () => this._shutdown()
    );

    this._db = db;
  },

  async _shutdown() {
    let db = await this.ensureDB();
    await db.close();
  },

  _readAsyncStream(stream) {
    return new Promise(function(resolve, reject) {
      let result = "";
      let source = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
        Ci.nsIBinaryInputStream
      );
      source.setInputStream(stream);
      function readData() {
        try {
          result += source.readBytes(source.available());
          stream.asyncWait(readData, 0, 0, Services.tm.currentThread);
        } catch (e) {
          if (e.result == Cr.NS_BASE_STREAM_CLOSED) {
            resolve(result);
          } else {
            reject(e);
          }
        }
      }
      stream.asyncWait(readData, 0, 0, Services.tm.currentThread);
    });
  },

  async recordContentBlockingLog(inputStream) {
    let json = await this._readAsyncStream(inputStream);
    requestIdleCallback(this.saveEvents.bind(this, json));
  },

  identifyType(events) {
    let result = null;
    let isTracker = false;
    for (let [state, blocked] of events) {
      if (state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) {
        isTracker = true;
      }
      if (blocked) {
        if (state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) {
          result = Ci.nsITrackingDBService.TRACKERS_ID;
        }
        if (
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.FINGERPRINTERS_ID;
        }
        if (
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.CRYPTOMINERS_ID;
        }
        if (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) {
          result = Ci.nsITrackingDBService.TRACKING_COOKIES_ID;
        }
        if (
          state &
            Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION ||
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL ||
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN
        ) {
          result = Ci.nsITrackingDBService.OTHER_COOKIES_BLOCKED_ID;
        }
      }
    }
    // if a cookie is blocked for any reason, and it is identified as a tracker,
    // then add to the tracking cookies count.
    if (
      result == Ci.nsITrackingDBService.OTHER_COOKIES_BLOCKED_ID &&
      isTracker
    ) {
      result = Ci.nsITrackingDBService.TRACKING_COOKIES_ID;
    }

    return result;
  },

  /**
   * Saves data rows to the DB.
   * @param data
   *        An array of JS objects representing row items to save.
   */
  async saveEvents(data) {
    let db = await this.ensureDB();
    let log = JSON.parse(data);
    try {
      await db.executeTransaction(async () => {
        for (let thirdParty in log) {
          // "type" will be undefined if there is no blocking event, or 0 if it is a
          // cookie which is not a tracking cookie. These should not be added to the database.
          let type = this.identifyType(log[thirdParty]);
          if (type) {
            // today is a date "YYY-MM-DD" which can compare with what is
            // already saved in the database.
            let today = new Date().toISOString().split("T")[0];
            let row = await db.executeCached(SQL.selectByTypeAndDate, {
              type,
              date: today,
            });
            let todayEntry = row[0];

            // If previous events happened today (local time), aggregate them.
            if (todayEntry) {
              let id = todayEntry.getResultByName("id");
              await db.executeCached(SQL.incrementEvent, { id });
            } else {
              // Event is created on a new day, add a new entry.
              await db.executeCached(SQL.addEvent, { type, date: today });
            }
          }
        }
      });
    } catch (e) {
      Cu.reportError(e);
    }
  },

  async clearAll() {
    let db = await this.ensureDB();
    await removeAllRecords(db);
  },

  async clearSince(date) {
    let db = await this.ensureDB();
    await removeRecordsSince(db, date);
  },
};

var EXPORTED_SYMBOLS = ["TrackingDBService"];
