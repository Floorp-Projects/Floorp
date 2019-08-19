/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  // Find the latest available version, this allows to skip .sqlite files when
  // the migration was trivial and uninsteresting to test.
  let version = CURRENT_SCHEMA_VERSION;
  while (version > 0) {
    let dbFile = OS.Path.join(do_get_cwd().path, `places_v${version}.sqlite`);
    if (await OS.File.exists(dbFile)) {
      info("Using database version " + version);
      break;
    }
    version--;
  }
  Assert.greater(version, 0, "Found a valid database version");
  await setupPlacesDatabase(`places_v${version}.sqlite`);
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
