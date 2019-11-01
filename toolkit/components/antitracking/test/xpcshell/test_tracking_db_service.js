/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This test may cause intermittents if run at exactly midnight.

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Sqlite } = ChromeUtils.import("resource://gre/modules/Sqlite.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

XPCOMUtils.defineLazyGetter(this, "DB_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "protections.sqlite");
});

const SQL = {
  insertCustomTimeEvent:
    "INSERT INTO events (type, count, timestamp)" +
    "VALUES (:type, :count, date(:timestamp));",

  selectAllEntriesOfType: "SELECT * FROM events WHERE type = :type;",

  selectAll: "SELECT * FROM events",
};

// Emulate the content blocking log. We do not record the url key, nor
// do we use the aggregated event number (the last element in the array).
const LOG = {
  "https://1.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT, true, 1],
  ],
  "https://2.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT, true, 1],
  ],
  "https://3.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT, true, 2],
  ],
  "https://4.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, true, 3],
  ],
  "https://5.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, true, 1],
  ],
  // Cookie blocked for other reason, then identified as a tracker
  "https://6.example.com": [
    [
      Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL |
        Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT,
      true,
      4,
    ],
  ],

  // The contents below should not add to the database.
  // Cookie loaded but not blocked.
  "https://7.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED, true, 1],
  ],
  // Tracker cookie loaded but not blocked.
  "https://8.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_TRACKER, true, 1],
  ],
  // Social tracker cookie loaded but not blocked.
  "https://9.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_SOCIALTRACKER, true, 1],
  ],
  // Cookie blocked for other reason (not a tracker)
  "https://10.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION, true, 2],
  ],
  // Fingerprinters set to block, but this one has an exception
  "https://11.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT, false, 1],
  ],
};

do_get_profile();

// This tests that data is added successfully, different types of events should get
// their own entries, when the type is the same they should be aggregated. Events
// that are not blocking events should not be recorded. Cookie blocking events
// should only be recorded if we can identify the cookie as a tracking cookie.
add_task(async function test_save_and_delete() {
  Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
  await TrackingDBService.saveEvents(JSON.stringify(LOG));

  // Peek in the DB to make sure we have the right data.
  let db = await Sqlite.openConnection({ path: DB_PATH });
  // Make sure the items table was created.
  ok(await db.tableExists("events"), "events table exists");

  // make sure we have the correct contents in the database
  let rows = await db.execute(SQL.selectAll);
  equal(
    rows.length,
    4,
    "Events that should not be saved have not been, length is 4"
  );
  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.TRACKERS_ID,
  });
  equal(rows.length, 1, "Only one day has had tracker entries, length is 1");
  let count = rows[0].getResultByName("count");
  equal(count, 1, "there is only one tracker entry");

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
  });
  equal(rows.length, 1, "Only one day has had cookies entries, length is 1");
  count = rows[0].getResultByName("count");
  equal(count, 3, "Cookie entries were aggregated");

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.CRYPTOMINERS_ID,
  });
  equal(
    rows.length,
    1,
    "Only one day has had cryptominer entries, length is 1"
  );
  count = rows[0].getResultByName("count");
  equal(count, 1, "there is only one cryptominer entry");

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.FINGERPRINTERS_ID,
  });
  equal(
    rows.length,
    1,
    "Only one day has had fingerprinters entries, length is 1"
  );
  count = rows[0].getResultByName("count");
  equal(count, 1, "there is only one fingerprinter entry");

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  rows = await db.execute(SQL.selectAll);
  equal(rows.length, 0, "length is 0");
  await db.close();
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
});

// This tests that content blocking events encountered on the same day get aggregated,
// and those on different days get seperate entries
add_task(async function test_timestamp_aggragation() {
  Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  let yesterday = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
  let today = new Date().toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 4,
    timestamp: yesterday,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 3,
    timestamp: yesterday,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: yesterday,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 1,
    timestamp: yesterday,
  });

  // Add some events for today which must get aggregated
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 2,
    timestamp: today,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 2,
    timestamp: today,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: today,
  });
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 2,
    timestamp: today,
  });

  // Add new events, they will have today's timestamp.
  await TrackingDBService.saveEvents(JSON.stringify(LOG));

  // Ensure events that are inserted today are not aggregated with past events.
  let rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.TRACKERS_ID,
  });
  equal(rows.length, 2, "Tracker entries for today and yesterday, length is 2");
  for (let i = 0; i < rows.length; i++) {
    let count = rows[i].getResultByName("count");
    if (i == 0) {
      equal(count, 4, "Yesterday's count is 4");
    } else if (i == 1) {
      equal(count, 3, "Today's count is 3, new entries were aggregated");
    }
  }

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.CRYPTOMINERS_ID,
  });
  equal(
    rows.length,
    2,
    "Cryptominer entries for today and yesterday, length is 2"
  );
  for (let i = 0; i < rows.length; i++) {
    let count = rows[i].getResultByName("count");
    if (i == 0) {
      equal(count, 3, "Yesterday's count is 3");
    } else if (i == 1) {
      equal(count, 3, "Today's count is 3, new entries were aggregated");
    }
  }

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.FINGERPRINTERS_ID,
  });
  equal(
    rows.length,
    2,
    "Fingerprinter entries for today and yesterday, length is 2"
  );
  for (let i = 0; i < rows.length; i++) {
    let count = rows[i].getResultByName("count");
    if (i == 0) {
      equal(count, 2, "Yesterday's count is 2");
    } else if (i == 1) {
      equal(count, 3, "Today's count is 3, new entries were aggregated");
    }
  }

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
  });
  equal(
    rows.length,
    2,
    "Tracking Cookies entries for today and yesterday, length is 2"
  );
  for (let i = 0; i < rows.length; i++) {
    let count = rows[i].getResultByName("count");
    if (i == 0) {
      equal(count, 1, "Yesterday's count is 1");
    } else if (i == 1) {
      equal(count, 5, "Today's count is 5, new entries were aggregated");
    }
  }

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  rows = await db.execute(SQL.selectAll);
  equal(rows.length, 0, "length is 0");
  await db.close();
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
});

let addEventsToDB = async db => {
  let d = new Date(1521009000000);
  let date = d.toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: 3,
    timestamp: date,
  });

  date = new Date(d - 2 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKERS_ID,
    count: 2,
    timestamp: date,
  });

  date = new Date(d - 3 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 2,
    timestamp: date,
  });

  date = new Date(d - 4 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.TRACKING_COOKIES_ID,
    count: 2,
    timestamp: date,
  });

  date = new Date(d - 9 * 24 * 60 * 60 * 1000).toISOString();
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.FINGERPRINTERS_ID,
    count: 2,
    timestamp: date,
  });
};

// This tests that TrackingDBService.getEventsByDateRange can accept two timestamps in unix epoch time
// and return entries that occur within the timestamps, rounded to the nearest day and inclusive.
add_task(async function test_getEventsByDateRange() {
  Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });
  await addEventsToDB(db);

  let d = new Date(1521009000000);
  let daysBefore1 = new Date(d - 24 * 60 * 60 * 1000);
  let daysBefore4 = new Date(d - 4 * 24 * 60 * 60 * 1000);
  let daysBefore9 = new Date(d - 9 * 24 * 60 * 60 * 1000);

  let events = await TrackingDBService.getEventsByDateRange(daysBefore1, d);
  equal(
    events.length,
    1,
    "There is 1 event entry between the date and one day before, inclusive"
  );

  events = await TrackingDBService.getEventsByDateRange(daysBefore4, d);
  equal(
    events.length,
    4,
    "There is 4 event entries between the date and four days before, inclusive"
  );

  events = await TrackingDBService.getEventsByDateRange(
    daysBefore9,
    daysBefore4
  );
  equal(
    events.length,
    2,
    "There is 2 event entries between nine and four days before, inclusive"
  );

  await TrackingDBService.clearAll();
  await db.close();
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
});

// This tests that TrackingDBService.sumAllEvents returns the number of
// tracking events in the database, and can handle 0 entries.
add_task(async function test_sumAllEvents() {
  Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  let sum = await TrackingDBService.sumAllEvents();
  equal(sum, 0, "There have been 0 events recorded");

  // populate the database
  await addEventsToDB(db);

  sum = await TrackingDBService.sumAllEvents();
  equal(sum, 11, "There have been 11 events recorded");

  await TrackingDBService.clearAll();
  await db.close();
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
});

// This tests that TrackingDBService.getEarliestRecordedDate returns the
// earliest date recorded and can handle 0 entries.
add_task(async function test_getEarliestRecordedDate() {
  Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });

  let timestamp = await TrackingDBService.getEarliestRecordedDate();
  equal(timestamp, null, "There is no earliest recorded date");

  // populate the database
  await addEventsToDB(db);
  let d = new Date(1521009000000);
  let daysBefore9 = new Date(d - 9 * 24 * 60 * 60 * 1000)
    .toISOString()
    .split("T")[0];

  timestamp = await TrackingDBService.getEarliestRecordedDate();
  let date = new Date(timestamp).toISOString().split("T")[0];
  equal(date, daysBefore9, "The earliest recorded event is nine days before.");

  await TrackingDBService.clearAll();
  await db.close();
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
});
