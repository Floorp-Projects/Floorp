/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { Sqlite } from "resource://gre/modules/Sqlite.sys.mjs";

const SCHEMA_VERSION = 1;
const TRACKERS_BLOCKED_COUNT = "contentblocking.trackers_blocked_count";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "DB_PATH", function () {
  return PathUtils.join(PathUtils.profileDir, "protections.sqlite");
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "social_enabled",
  "privacy.socialtracking.block_cookies.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "milestoneMessagingEnabled",
  "browser.contentblocking.cfr-milestone.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "milestones",
  "browser.contentblocking.cfr-milestone.milestones",
  "[]",
  null,
  JSON.parse
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "oldMilestone",
  "browser.contentblocking.cfr-milestone.milestone-achieved",
  0
);

// How often we check if the user is eligible for seeing a "milestone"
// doorhanger. 24 hours by default.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MILESTONE_UPDATE_INTERVAL",
  "browser.contentblocking.cfr-milestone.update-interval",
  24 * 60 * 60 * 1000
);

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
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

export function TrackingDBService() {
  this._initPromise = this._initialize();
}

TrackingDBService.prototype = {
  classID: Components.ID("{3c9c43b6-09eb-4ed2-9b87-e29f4221eef0}"),
  QueryInterface: ChromeUtils.generateQI(["nsITrackingDBService"]),
  // This is the connection to the database, opened in _initialize and closed on _shutdown.
  _db: null,
  waitingTasks: new Set(),
  finishedShutdown: true,

  async ensureDB() {
    await this._initPromise;
    return this._db;
  },

  async _initialize() {
    let db = await Sqlite.openConnection({ path: lazy.DB_PATH });

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

    lazy.AsyncShutdown.profileBeforeChange.addBlocker(
      "TrackingDBService: Shutting down the content blocking database.",
      () => this._shutdown()
    );
    this.finishedShutdown = false;
    this._db = db;
  },

  async _shutdown() {
    let db = await this.ensureDB();
    this.finishedShutdown = true;
    await Promise.all(Array.from(this.waitingTasks, task => task.finalize()));
    await db.close();
  },

  async recordContentBlockingLog(data) {
    if (this.finishedShutdown) {
      // The database has already been closed.
      return;
    }
    let task = new lazy.DeferredTask(async () => {
      try {
        await this.saveEvents(data);
      } finally {
        this.waitingTasks.delete(task);
      }
    }, 0);
    task.arm();
    this.waitingTasks.add(task);
  },

  identifyType(events) {
    let result = null;
    let isTracker = false;
    for (let [state, blocked] of events) {
      if (
        state &
          Ci.nsIWebProgressListener.STATE_LOADED_LEVEL_1_TRACKING_CONTENT ||
        state & Ci.nsIWebProgressListener.STATE_LOADED_LEVEL_2_TRACKING_CONTENT
      ) {
        isTracker = true;
      }
      if (blocked) {
        if (
          state &
            Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT ||
          state &
            Ci.nsIWebProgressListener.STATE_REPLACED_FINGERPRINTING_CONTENT
        ) {
          result = Ci.nsITrackingDBService.FINGERPRINTERS_ID;
        } else if (
          // If STP is enabled and either a social tracker or cookie is blocked.
          lazy.social_enabled &&
          (state &
            Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
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
          // If a tracking cookie was blocked attribute it to tracking cookies.
          // This includes social tracking cookies since STP is not enabled.
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER ||
          state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_SOCIALTRACKER
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
      console.error(e);
    }

    // If milestone CFR messaging is not enabled we don't need to update the milestone pref or send the event.
    // We don't do this check too frequently, for performance reasons.
    if (
      !lazy.milestoneMessagingEnabled ||
      (this.lastChecked &&
        Date.now() - this.lastChecked < lazy.MILESTONE_UPDATE_INTERVAL)
    ) {
      return;
    }
    this.lastChecked = Date.now();
    let totalSaved = await this.sumAllEvents();

    let reachedMilestone = null;
    let nextMilestone = null;
    for (let [index, milestone] of lazy.milestones.entries()) {
      if (totalSaved >= milestone) {
        reachedMilestone = milestone;
        nextMilestone = lazy.milestones[index + 1];
      }
    }

    // Show the milestone message if the user is not too close to the next milestone.
    // Or if there is no next milestone.
    if (
      reachedMilestone &&
      (!nextMilestone || nextMilestone - totalSaved > 3000) &&
      (!lazy.oldMilestone || lazy.oldMilestone < reachedMilestone)
    ) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            event: "ContentBlockingMilestone",
          },
        },
        "SiteProtection:ContentBlockingMilestone"
      );
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
