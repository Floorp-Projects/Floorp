/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

function run_test() {
  // Ensure that our database doesn't already exist.
  let dbFile = gProfD.clone();
  dbFile.append("places.sqlite");
  do_check_false(dbFile.exists());

  dbFile = gProfD.clone();
  dbFile.append("places.sqlite.corrupt");
  do_check_false(dbFile.exists());

  let file = do_get_file("default.sqlite");
  file.copyToFollowingLinks(gProfD, "places.sqlite");
  file = gProfD.clone();
  file.append("places.sqlite");

  // Create some unique stuff to check later.
  let db = Services.storage.openUnsharedDatabase(file);
  db.executeSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)");
  db.close();

  Services.prefs.setBoolPref("places.database.replaceOnStartup", true);
  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_CORRUPT);

  dbFile = gProfD.clone();
  dbFile.append("places.sqlite");
  do_check_true(dbFile.exists());

  // Check the new database is really a new one.
  db = Services.storage.openUnsharedDatabase(file);
  try {
    db.executeSimpleSQL("DELETE * FROM test");
    do_throw("The new database should not have our unique content");
  } catch (ex) {}
  db.close();

  dbFile = gProfD.clone();
  dbFile.append("places.sqlite.corrupt");
  do_check_true(dbFile.exists());
}
