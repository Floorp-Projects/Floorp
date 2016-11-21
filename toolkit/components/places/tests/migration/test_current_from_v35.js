add_task(function* setup() {
  yield setupPlacesDatabase("places_v35.sqlite");
});

add_task(function* database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_sync_fields() {
  let db = yield PlacesUtils.promiseDBConnection();
  let syncFields = yield db.executeCached(`
    SELECT guid, syncChangeCounter, syncStatus
    FROM moz_bookmarks`);
  for (let row of syncFields) {
    let syncStatus = row.getResultByName("syncStatus");
    if (syncStatus !== Ci.nsINavBookmarksService.SYNC_STATUS_UNKNOWN) {
      ok(false, `Wrong sync status for migrated item ${row.getResultByName("guid")}`);
    }
    let syncChangeCounter = row.getResultByName("syncChangeCounter");
    if (syncChangeCounter !== 1) {
      ok(false, `Wrong change counter for migrated item ${row.getResultByName("guid")}`);
    }
  }
});
