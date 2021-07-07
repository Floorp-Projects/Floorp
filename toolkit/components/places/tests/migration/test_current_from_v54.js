add_task(async function setup() {
  // Since this migration doesn't affect places.sqlite, we can reuse v43.
  await setupPlacesDatabase("places_v54.sqlite");
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
    await db.execute(`SELECT count(*) FROM moz_places_metadata`)
  )[0].getResultByIndex(0);
  Assert.equal(count, 0, "Empty table");
  count = (
    await db.execute(`SELECT count(*) FROM moz_places_metadata_search_queries`)
  )[0].getResultByIndex(0);
  Assert.equal(count, 0, "Empty table");
});

add_task(async function scrolling_fields_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(
    `SELECT scrolling_time,scrolling_distance FROM moz_places_metadata`
  );
});
