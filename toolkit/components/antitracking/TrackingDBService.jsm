/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Sqlite } = ChromeUtils.import("resource://gre/modules/Sqlite.jsm");
const { requestIdleCallback } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const SCHEMA_VERSION = 1;
const TRACKERS_BLOCKED_COUNT = "contentblocking.trackers_blocked_count";

XPCOMUtils.defineLazyGetter(this, "DB_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "protections.sqlite");
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "social_enabled",
  "privacy.socialtracking.block_cookies.enabled",
  false
);

ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  readAsyncStream: "resource://gre/modules/AsyncStreamReader.jsm",
});

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

  selectByDateRange:
    "SELECT * FROM events " +
    "WHERE timestamp BETWEEN date(:dateFrom) AND date(:dateTo);",

  sumAllEvents: "SELECT sum(count) FROM events;",

  getEarliestDate:
    "SELECT timestamp FROM events ORDER BY timestamp ASC LIMIT 1;",
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

  async recordContentBlockingLog(inputStream) {
    /* import-globals-from AsyncStreamReader.jsm */
    let json = await readAsyncStream(inputStream);
    requestIdleCallback(this.saveEvents.bind(this, json));
  },

  identifyType(events) {
    let result = null;
    let isTracker = false;
    let isSocialTracker = false;
    for (let [state, blocked] of events) {
      if (state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) {
        isTracker = true;
      }
      if (
        state & Ci.nsIWebProgressListener.STATE_LOADED_SOCIALTRACKING_CONTENT
      ) {
        isSocialTracker = true;
      }
      if (blocked) {
        if (
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.FINGERPRINTERS_ID;
        } else if (
          // If STP is enabled and either a social tracker is blocked,
          // or a cookie was blocked with a social tracking event
          social_enabled &&
          ((isSocialTracker &&
            state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) ||
            state &
              Ci.nsIWebProgressListener.STATE_BLOCKED_SOCIALTRACKING_CONTENT)
        ) {
          result = Ci.nsITrackingDBService.SOCIAL_ID;
        } else if (
          // If there is a tracker blocked. If there is a social tracker blocked, but STP is not enabled.
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT ||
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_SOCIALTRACKING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.TRACKERS_ID;
        } else if (
          // If a tracking cookie was blocked attribute it to tracking cookies. Possible social tracking content,
          // but STP is not enabled.
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER
        ) {
          result = Ci.nsITrackingDBService.TRACKING_COOKIES_ID;
        } else if (
          state &
            Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION ||
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL ||
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN
        ) {
          result = Ci.nsITrackingDBService.OTHER_COOKIES_BLOCKED_ID;
        } else if (
          state & Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.CRYPTOMINERS_ID;
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
            // Send the blocked event to Telemetry
            Services.telemetry.scalarAdd(TRACKERS_BLOCKED_COUNT, 1);

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
    date = new Date(date).toISOString();
    await removeRecordsSince(db, date);
  },

  async getEventsByDateRange(dateFrom, dateTo) {
    let db = await this.ensureDB();
    dateFrom = new Date(dateFrom).toISOString();
    dateTo = new Date(dateTo).toISOString();
    return db.execute(SQL.selectByDateRange, { dateFrom, dateTo });
  },

  async sumAllEvents() {
    let db = await this.ensureDB();
    let results = await db.execute(SQL.sumAllEvents);
    if (!results[0]) {
      return 0;
    }
    let total = results[0].getResultByName("sum(count)");
    return total || 0;
  },

  async getEarliestRecordedDate() {
    let db = await this.ensureDB();
    let date = await db.execute(SQL.getEarliestDate);
    if (!date[0]) {
      return null;
    }
    let earliestDate = date[0].getResultByName("timestamp");

    // All of our dates are recorded as 00:00 GMT, add 12 hours to the timestamp
    // to ensure we display the correct date no matter the user's location.
    let hoursInMS12 = 12 * 60 * 60 * 1000;
    let earliestDateInMS = new Date(earliestDate).getTime() + hoursInMS12;

    return earliestDateInMS || null;
  },
};

var EXPORTED_SYMBOLS = ["TrackingDBService"];
