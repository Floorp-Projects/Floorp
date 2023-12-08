/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This test may cause intermittents if run at exactly midnight.

"use strict";

const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

ChromeUtils.defineLazyGetter(this, "DB_PATH", function () {
  return PathUtils.join(PathUtils.profileDir, "protections.sqlite");
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
        Ci.nsIWebProgressListener.STATE_LOADED_LEVEL_1_TRACKING_CONTENT,
      true,
      4,
    ],
  ],
  "https://7.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_SOCIALTRACKER, true, 1],
  ],
  "https://8.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_SOCIALTRACKING_CONTENT, true, 1],
  ],

  // The contents below should not add to the database.
  // Cookie loaded but not blocked.
  "https://10.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED, true, 1],
  ],
  // Tracker cookie loaded but not blocked.
  "https://11.unblocked.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_TRACKER, true, 1],
  ],
  // Social tracker cookie loaded but not blocked.
  "https://12.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_LOADED_SOCIALTRACKER, true, 1],
  ],
  // Cookie blocked for other reason (not a tracker)
  "https://13.example.com": [
    [Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION, true, 2],
  ],
  // Fingerprinters set to block, but this one has an exception
  "https://14.example.com": [
    [Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT, false, 1],
  ],
  // Two fingerprinters replaced with a shims script, should be treated as blocked
  // and increment the counter.
  "https://15.example.com": [
    [Ci.nsIWebProgressListener.STATE_REPLACED_FINGERPRINTING_CONTENT, true, 1],
  ],
  "https://16.example.com": [
    [Ci.nsIWebProgressListener.STATE_REPLACED_FINGERPRINTING_CONTENT, true, 1],
  ],
  "https://17.example.com": [
    [
      Ci.nsIWebProgressListener.STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING,
      true,
      1,
    ],
  ],
};

do_get_profile();

Services.prefs.setBoolPref("browser.contentblocking.database.enabled", true);
Services.prefs.setBoolPref(
  "privacy.socialtracking.block_cookies.enabled",
  true
);
Services.prefs.setBoolPref(
  "privacy.trackingprotection.fingerprinting.enabled",
  true
);
Services.prefs.setBoolPref("privacy.fingerprintingProtection", true);
Services.prefs.setBoolPref(
  "browser.contentblocking.cfr-milestone.enabled",
  true
);
Services.prefs.setIntPref(
  "browser.contentblocking.cfr-milestone.update-interval",
  0
);
Services.prefs.setStringPref(
  "browser.contentblocking.cfr-milestone.milestones",
  "[1000, 5000, 10000, 25000, 100000, 500000]"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.contentblocking.database.enabled");
  Services.prefs.clearUserPref("privacy.socialtracking.block_cookies.enabled");
  Services.prefs.clearUserPref(
    "privacy.trackingprotection.fingerprinting.enabled"
  );
  Services.prefs.clearUserPref("privacy.fingerprintingProtection");
  Services.prefs.clearUserPref("browser.contentblocking.cfr-milestone.enabled");
  Services.prefs.clearUserPref(
    "browser.contentblocking.cfr-milestone.update-interval"
  );
  Services.prefs.clearUserPref(
    "browser.contentblocking.cfr-milestone.milestones"
  );
});

// This tests that data is added successfully, different types of events should get
// their own entries, when the type is the same they should be aggregated. Events
// that are not blocking events should not be recorded. Cookie blocking events
// should only be recorded if we can identify the cookie as a tracking cookie.
add_task(async function test_save_and_delete() {
  await TrackingDBService.saveEvents(JSON.stringify(LOG));

  // Peek in the DB to make sure we have the right data.
  let db = await Sqlite.openConnection({ path: DB_PATH });
  // Make sure the items table was created.
  ok(await db.tableExists("events"), "events table exists");

  // make sure we have the correct contents in the database
  let rows = await db.execute(SQL.selectAll);
  equal(
    rows.length,
    6,
    "Events that should not be saved have not been, length is 6"
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
  equal(count, 3, "there are three fingerprinter entries");

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.SOCIAL_ID,
  });
  equal(rows.length, 1, "Only one day has had social entries, length is 1");
  count = rows[0].getResultByName("count");
  equal(count, 2, "there are two social entries");

  rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.SUSPICIOUS_FINGERPRINTERS_ID,
  });
  equal(
    rows.length,
    1,
    "Only one day has had suspicious fingerprinting entries, length is 1"
  );
  count = rows[0].getResultByName("count");
  equal(count, 1, "there is one suspicious fingerprinting entry");

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  // Make sure the data was deleted.
  rows = await db.execute(SQL.selectAll);
  equal(rows.length, 0, "length is 0");
  await db.close();
});

// This tests that content blocking events encountered on the same day get aggregated,
// and those on different days get seperate entries
add_task(async function test_timestamp_aggragation() {
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
      equal(count, 5, "Today's count is 5, new entries were aggregated");
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
});

// This tests that TrackingDBService.sumAllEvents returns the number of
// tracking events in the database, and can handle 0 entries.
add_task(async function test_sumAllEvents() {
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
});

// This tests that TrackingDBService.getEarliestRecordedDate returns the
// earliest date recorded and can handle 0 entries.
add_task(async function test_getEarliestRecordedDate() {
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
});

// This tests that a message to CFR is sent when the amount of saved trackers meets a milestone
add_task(async function test_sendMilestoneNotification() {
  let milestones = JSON.parse(
    Services.prefs.getStringPref(
      "browser.contentblocking.cfr-milestone.milestones"
    )
  );
  // This creates the schema.
  await TrackingDBService.saveEvents(JSON.stringify({}));
  let db = await Sqlite.openConnection({ path: DB_PATH });
  // save number of trackers equal to the first milestone
  await db.execute(SQL.insertCustomTimeEvent, {
    type: TrackingDBService.CRYPTOMINERS_ID,
    count: milestones[0],
    timestamp: new Date().toISOString(),
  });

  let awaitNotification = TestUtils.topicObserved(
    "SiteProtection:ContentBlockingMilestone"
  );

  // trigger a "save" event to compare the trackers with the milestone.
  await TrackingDBService.saveEvents(
    JSON.stringify({
      "https://1.example.com": [
        [Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT, true, 1],
      ],
    })
  );
  await awaitNotification;

  await TrackingDBService.clearAll();
  await db.close();
});

// Ensure we don't record suspicious fingerprinting if the fingerprinting
// protection is disabled.
add_task(async function test_noSuspiciousFingerprintingWithFPPDisabled() {
  Services.prefs.setBoolPref("privacy.fingerprintingProtection", false);

  await TrackingDBService.saveEvents(JSON.stringify(LOG));

  // Peek in the DB to make sure we have the right data.
  let db = await Sqlite.openConnection({ path: DB_PATH });
  // Make sure the items table was created.
  ok(await db.tableExists("events"), "events table exists");

  let rows = await db.execute(SQL.selectAllEntriesOfType, {
    type: TrackingDBService.SUSPICIOUS_FINGERPRINTERS_ID,
  });
  equal(
    rows.length,
    0,
    "Should be no suspicious entry if the fingerprinting protection is disabled"
  );

  // Use the TrackingDBService API to delete the data.
  await TrackingDBService.clearAll();
  await db.close();
});
