add_task(async function setup() {
  // Since this migration doesn't affect places.sqlite, we can reuse v43.
  await setupPlacesDatabase("places_v52.sqlite");
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

  let count = (
    await db.execute(
      `SELECT count(*) FROM moz_icons_to_pages WHERE expire_ms = 0`
    )
  )[0].getResultByIndex(0);
  Assert.equal(count, 0, "All the expirations should be set");
});
