/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures we can pass twice through migration methods without
// failing, that is what happens in case of a downgrade followed by an upgrade.

add_task(async function setup() {
  let dbFile = OS.Path.join(
    do_get_cwd().path,
    `places_v${CURRENT_SCHEMA_VERSION}.sqlite`
  );
  Assert.ok(await OS.File.exists(dbFile));
  await setupPlacesDatabase(`places_v${CURRENT_SCHEMA_VERSION}.sqlite`);
  // Downgrade the schema version to the first supported one.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  await db.setSchemaVersion(FIRST_UPGRADABLE_SCHEMA_VERSION);
  await db.close();
});

add_task(async function database_is_valid() {
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);
});
