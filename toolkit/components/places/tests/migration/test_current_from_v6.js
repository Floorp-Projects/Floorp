/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests migration from a preliminary schema version 6 that
 * lacks frecency column and moz_inputhistory table.
 */

add_task(function* setup() {
  yield setupPlacesDatabase("places_v6.sqlite");
});

add_task(function* corrupt_database_not_exists() {
  let corruptPath = OS.Path.join(OS.Constants.Path.profileDir,
                                 "places.sqlite.corrupt");
  Assert.ok(!(yield OS.File.exists(corruptPath)), "Corrupt file should not exist");
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CORRUPT);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* check_columns() {
  // Check the database has been replaced, these would throw otherwise.
  let db = yield PlacesUtils.promiseDBConnection();
  yield db.execute("SELECT frecency from moz_places");
  yield db.execute("SELECT 1 from moz_inputhistory");
});

add_task(function* corrupt_database_exists() {
  let corruptPath = OS.Path.join(OS.Constants.Path.profileDir,
                                 "places.sqlite.corrupt");
  Assert.ok((yield OS.File.exists(corruptPath)), "Corrupt file should exist");
});
