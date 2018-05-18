/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.jsm",
});

async function createCorruptDb(filename) {
  let path = OS.Path.join(OS.Constants.Path.profileDir, filename);
  await OS.File.remove(path, {ignoreAbsent: true});
  // Create a corrupt database.
  let dir = await OS.File.getCurrentDirectory();
  let src = OS.Path.join(dir, "corruptDB.sqlite");
  await OS.File.copy(src, path);
}

/**
 * Used in _replaceOnStartup_ tests as common test code. It checks whether we
 * are properly cloning or replacing a corrupt database.
 *
 * @param {string} src
 *        Path to a test database, relative to this test folder
 * @param {string} filename
 *        Database file name
 * @param {boolean} shouldClone
 *        Whether we expect the database to be cloned
 * @param {boolean} dbStatus
 *        The expected final database status
 */
async function test_database_replacement(src, filename, shouldClone, dbStatus) {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("places.database.cloneOnCorruption");
  });
  Services.prefs.setBoolPref("places.database.cloneOnCorruption", shouldClone);

  // Only the main database file (places.sqlite) will be cloned, because
  // attached databased would break due to OS file lockings.
  let willClone = shouldClone && filename == DB_FILENAME;

  // Ensure that our databases don't exist yet.
  let dest = OS.Path.join(OS.Constants.Path.profileDir, filename);
  Assert.ok(!await OS.File.exists(dest), `"${filename} should not exist initially`);
  let corrupt = OS.Path.join(OS.Constants.Path.profileDir, `${filename}.corrupt`);
  Assert.ok(!await OS.File.exists(corrupt), `${filename}.corrupt should not exist initially`);

  let dir = await OS.File.getCurrentDirectory();
  src = OS.Path.join(dir, src);
  await OS.File.copy(src, dest);

  // Create some unique stuff to check later.
  let db = await Sqlite.openConnection({ path: dest });
  await db.execute(`CREATE TABLE moz_cloned (id INTEGER PRIMARY KEY)`);
  await db.execute(`CREATE TABLE not_cloned (id INTEGER PRIMARY KEY)`);
  await db.execute(`DELETE FROM moz_cloned`); // Shouldn't throw.
  await db.close();

  // Open the database with Places.
  Services.prefs.setCharPref("places.database.replaceDatabaseOnStartup", filename);
  Assert.equal(PlacesUtils.history.databaseStatus, dbStatus);

  Assert.ok(await OS.File.exists(dest), "The database should exist");

  // Check the new database still contains our special data.
  db = await Sqlite.openConnection({ path: dest });
  if (willClone) {
    await db.execute(`DELETE FROM moz_cloned`); // Shouldn't throw.
  }

  // Check the new database is really a new one.
  await Assert.rejects(db.execute(`DELETE FROM not_cloned`),
                       /no such table/,
                       "The database should have been replaced");
  await db.close();

  if (willClone) {
    Assert.ok(!await OS.File.exists(corrupt), "The corrupt db should not exist");
  } else {
    Assert.ok(await OS.File.exists(corrupt), "The corrupt db should exist");
  }
}
