// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/HomeProvider.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const TEST_DATASET_ID = "test-dataset-id";
const TEST_URL = "http://test.com";
const TEST_TITLE = "Test";

const PREF_SYNC_CHECK_INTERVAL_SECS = "home.sync.checkIntervalSecs";
const TEST_INTERVAL_SECS = 1;

const DB_PATH = OS.Path.join(OS.Constants.Path.profileDir, "home.sqlite");

add_test(function test_request_sync() {
  // The current implementation of requestSync is synchronous.
  let success = HomeProvider.requestSync(TEST_DATASET_ID, function callback(datasetId) {
    do_check_eq(datasetId, TEST_DATASET_ID);
  });

  do_check_true(success);
  run_next_test();
});

add_test(function test_periodic_sync() {
  do_register_cleanup(function cleanup() {
    Services.prefs.clearUserPref(PREF_SYNC_CHECK_INTERVAL_SECS);
    HomeProvider.removePeriodicSync(TEST_DATASET_ID);
  });

  // Lower the check interval for testing purposes.
  Services.prefs.setIntPref(PREF_SYNC_CHECK_INTERVAL_SECS, TEST_INTERVAL_SECS);

  HomeProvider.addPeriodicSync(TEST_DATASET_ID, TEST_INTERVAL_SECS, function callback(datasetId) {
    do_check_eq(datasetId, TEST_DATASET_ID);
    run_next_test();
  });
});

add_task(function test_save_and_delete() {
  // Use the HomeProvider API to save some data.
  let storage = HomeProvider.getStorage(TEST_DATASET_ID);
  yield storage.save([{ title: TEST_TITLE, url: TEST_URL }]);

  // Peek in the DB to make sure we have the right data.
  let db = yield Sqlite.openConnection({ path: DB_PATH });

  // Make sure the items table was created.
  do_check_true(yield db.tableExists("items"));

  // Make sure the correct values for the item ended up in there.
  let result = yield db.execute("SELECT * FROM items", null, function onRow(row){
    do_check_eq(row.getResultByName("dataset_id"), TEST_DATASET_ID);
    do_check_eq(row.getResultByName("url"), TEST_URL);
  });

  // Use the HomeProvider API to delete the data.
  yield storage.deleteAll();

  // Make sure the data was deleted.
  let result = yield db.execute("SELECT * FROM items");
  do_check_eq(result.length, 0);

  db.close();
});

add_task(function test_row_validation() {
  // Use the HomeProvider API to save some data.
  let storage = HomeProvider.getStorage(TEST_DATASET_ID);

  let invalidRows = [
    { url: "url" },
    { title: "title" },
    { description: "description" },
    { image_url: "image_url" }
  ];

  // None of these save calls should save anything
  for (let row of invalidRows) {
    try {
      yield storage.save([row]);
    } catch (e if e instanceof HomeProvider.ValidationError) {
      // Just catch and ignore validation errors
    }
  }

  // Peek in the DB to make sure we have the right data.
  let db = yield Sqlite.openConnection({ path: DB_PATH });

  // Make sure no data has been saved.
  let result = yield db.execute("SELECT * FROM items");
  do_check_eq(result.length, 0);

  db.close();
});

add_task(function test_save_transaction() {
  // Use the HomeProvider API to save some data.
  let storage = HomeProvider.getStorage(TEST_DATASET_ID);

  // One valid, one invalid
  let rows = [
    { title: TEST_TITLE, url: TEST_URL },
    { image_url: "image_url" }
  ];

  // Try to save all the rows at once
  try {
    yield storage.save(rows);
  } catch (e if e instanceof HomeProvider.ValidationError) {
    // Just catch and ignore validation errors
  }

  // Peek in the DB to make sure we have the right data.
  let db = yield Sqlite.openConnection({ path: DB_PATH });

  // Make sure no data has been saved.
  let result = yield db.execute("SELECT * FROM items");
  do_check_eq(result.length, 0);

  db.close();
});

run_next_test();
