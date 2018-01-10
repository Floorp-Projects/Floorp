/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_task(async function() {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("places.database.cloneOnCorruption");
  });
  test_replacement(true);
  test_replacement(false);
});

async function test_replacement(shouldClone) {
  Services.prefs.setBoolPref("places.database.cloneOnCorruption", shouldClone);
  // Ensure that our databases don't exist yet.
  let sane = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
  Assert.ok(!await OS.File.exists(sane), "The db should not exist initially");
  let corrupt = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite.corrupt");
  Assert.ok(!await OS.File.exists(corrupt), "The corrupt db should not exist initially");

  let file = do_get_file("../migration/places_v38.sqlite");
  file.copyToFollowingLinks(gProfD, "places.sqlite");
  file = gProfD.clone();
  file.append("places.sqlite");

  // Create some unique stuff to check later.
  let db = Services.storage.openUnsharedDatabase(file);
  db.executeSimpleSQL("CREATE TABLE moz_cloned(id INTEGER PRIMARY KEY)");
  db.executeSimpleSQL("CREATE TABLE not_cloned (id INTEGER PRIMARY KEY)");
  db.executeSimpleSQL("DELETE FROM moz_cloned"); // Shouldn't throw.
  db.close();

  // Open the database with Places.
  Services.prefs.setBoolPref("places.database.replaceOnStartup", true);
  let expectedStatus = shouldClone ? PlacesUtils.history.DATABASE_STATUS_UPGRADED
                                   : PlacesUtils.history.DATABASE_STATUS_CORRUPT;
  Assert.equal(PlacesUtils.history.databaseStatus, expectedStatus);

  Assert.ok(await OS.File.exists(sane), "The database should exist");

  // Check the new database still contains our special data.
  db = Services.storage.openUnsharedDatabase(file);
  if (shouldClone) {
    db.executeSimpleSQL("DELETE FROM moz_cloned"); // Shouldn't throw.
  }

  // Check the new database is really a new one.
  Assert.throws(() => {
    db.executeSimpleSQL("DELETE FROM not_cloned");
  }, "The database should have been replaced");
  db.close();

  if (shouldClone) {
    Assert.ok(!await OS.File.exists(corrupt), "The corrupt db should not exist");
  } else {
    Assert.ok(await OS.File.exists(corrupt), "The corrupt db should exist");
  }
}
