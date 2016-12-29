/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* setup() {
  yield setupPlacesDatabase(`places_v${CURRENT_SCHEMA_VERSION}.sqlite`);
  // Downgrade the schema version to the first supported one.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = yield Sqlite.openConnection({ path });
  yield db.setSchemaVersion(FIRST_UPGRADABLE_SCHEMA_VERSION);
  yield db.close();
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});
