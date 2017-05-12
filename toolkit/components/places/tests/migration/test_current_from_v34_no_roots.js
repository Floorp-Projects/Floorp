/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await setupPlacesDatabase("places_v34.sqlite");
  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  // Remove all the roots.
  await db.execute("DELETE FROM moz_bookmarks");
  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});
