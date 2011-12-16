/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests migration from a preliminary schema version 6 that
 * lacks frecency column and moz_inputhistory table.
 */

add_test(function database_is_valid() {
  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_UPGRADED);
  // This throws if frecency column does not exist.
  stmt = DBConn().createStatement("SELECT frecency from moz_places");
  stmt.finalize();
  // Check moz_inputhistory is in place.
  do_check_true(DBConn().tableExists("moz_inputhistory"));
  run_next_test();
});

add_test(function corrupt_database_not_exists() {
  let dbFile = gProfD.clone();
  dbFile.append("places.sqlite.corrupt");
  do_check_false(dbFile.exists());
  run_next_test();
});

function run_test()
{
  setPlacesDatabase("places_v6_no_frecency.sqlite");
  run_next_test();
}
