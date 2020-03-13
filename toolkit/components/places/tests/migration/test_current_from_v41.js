add_task(async function setup() {
  // Since this migration doesn't affect places.sqlite, we can reuse v38.
  await setupPlacesDatabase("places_v38.sqlite");
  await setupPlacesDatabase("favicons_v41.sqlite", "favicons.sqlite");
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);

  let vacuum = (
    await db.execute(`PRAGMA favicons.auto_vacuum`)
  )[0].getResultByIndex(0);
  Assert.equal(vacuum, 2, "Incremental vacuum is enabled");
});
