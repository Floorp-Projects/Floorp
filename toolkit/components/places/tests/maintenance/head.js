/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

ChromeUtils.defineESModuleGetters(this, {
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.sys.mjs",
});

async function createCorruptDb(filename) {
  let path = PathUtils.join(PathUtils.profileDir, filename);
  await IOUtils.remove(path, { ignoreAbsent: true });
  // Create a corrupt database.
  let dir = do_get_cwd().path;
  let src = PathUtils.join(dir, "corruptDB.sqlite");
  await IOUtils.copy(src, path);
}

/**
 * Used in _replaceOnStartup_ tests as common test code. It checks whether we
 * are properly cloning or replacing a corrupt database.
 *
 * @param {string[]} src
 *        Array of strings which form a path to a test database, relative to
 *        the parent of this test folder.
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
  let dest = PathUtils.join(PathUtils.profileDir, filename);
  Assert.ok(
    !(await IOUtils.exists(dest)),
    `"${filename} should not exist initially`
  );
  let corrupt = PathUtils.join(PathUtils.profileDir, `${filename}.corrupt`);
  Assert.ok(
    !(await IOUtils.exists(corrupt)),
    `${filename}.corrupt should not exist initially`
  );

  let dir = PathUtils.parent(do_get_cwd().path);
  src = PathUtils.join(dir, ...src);
  await IOUtils.copy(src, dest);

  // Create some unique stuff to check later.
  let db = await Sqlite.openConnection({ path: dest });
  await db.execute(`CREATE TABLE moz_cloned (id INTEGER PRIMARY KEY)`);
  await db.execute(`CREATE TABLE not_cloned (id INTEGER PRIMARY KEY)`);
  await db.execute(`DELETE FROM moz_cloned`); // Shouldn't throw.
  await db.close();

  // Open the database with Places.
  Services.prefs.setCharPref(
    "places.database.replaceDatabaseOnStartup",
    filename
  );
  Assert.equal(PlacesUtils.history.databaseStatus, dbStatus);

  Assert.ok(await IOUtils.exists(dest), "The database should exist");

  // Check the new database still contains our special data.
  db = await Sqlite.openConnection({ path: dest });
  if (willClone) {
    await db.execute(`DELETE FROM moz_cloned`); // Shouldn't throw.
  }

  // Check the new database is really a new one.
  await Assert.rejects(
    db.execute(`DELETE FROM not_cloned`),
    /no such table/,
    "The database should have been replaced"
  );
  await db.close();

  if (willClone) {
    Assert.ok(
      !(await IOUtils.exists(corrupt)),
      "The corrupt db should not exist"
    );
  } else {
    Assert.ok(await IOUtils.exists(corrupt), "The corrupt db should exist");
  }
}
