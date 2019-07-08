add_task(async function setup() {
  await setupPlacesDatabase("places_v35.sqlite");
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_sync_fields() {
  let db = await PlacesUtils.promiseDBConnection();
  let syncFields = await db.executeCached(`
    SELECT guid, syncChangeCounter, syncStatus
    FROM moz_bookmarks`);
  for (let row of syncFields) {
    let syncStatus = row.getResultByName("syncStatus");
    if (syncStatus !== Ci.nsINavBookmarksService.SYNC_STATUS_UNKNOWN) {
      ok(
        false,
        `Wrong sync status for migrated item ${row.getResultByName("guid")}`
      );
    }
    let syncChangeCounter = row.getResultByName("syncChangeCounter");
    if (syncChangeCounter !== 1) {
      ok(
        false,
        `Wrong change counter for migrated item ${row.getResultByName("guid")}`
      );
    }
  }
});
