// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/HomeProvider.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const TEST_DATASET_ID = "test-dataset-id";
const TEST_URL = "http://test.com";

const DB_PATH = OS.Path.join(OS.Constants.Path.profileDir, "home.sqlite");

add_task(function test_save_and_delete() {
  // Use the HomeProvider API to save some data.
  let storage = HomeProvider.getStorage(TEST_DATASET_ID);
  yield storage.save([{ url: TEST_URL }]);

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

run_next_test();
