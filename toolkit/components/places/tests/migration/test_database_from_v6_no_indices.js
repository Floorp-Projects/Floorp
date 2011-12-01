/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests migration from a preliminary schema version 6 that
 * lacks important indices.  The database should be replaced.
 */

add_test(function corrupt_database_not_exists() {
  let dbFile = gProfD.clone();
  dbFile.append("places.sqlite.corrupt");
  do_check_false(dbFile.exists());
  run_next_test();
});

add_test(function database_is_valid() {
  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_CORRUPT);
  do_check_eq(DBConn().schemaVersion, CURRENT_SCHEMA_VERSION);
  run_next_test();
});

add_test(function corrupt_database_exists() {
  let dbFile = gProfD.clone();
  dbFile.append("places.sqlite.corrupt");
  do_check_true(dbFile.exists());
  run_next_test();
});

function run_test()
{
  setPlacesDatabase("places_v6_no_indices.sqlite");
  run_next_test();
}
