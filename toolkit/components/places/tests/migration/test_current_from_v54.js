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

  for (let table of [
    "moz_places_metadata",
    "moz_places_metadata_search_queries",
  ]) {
    let count = (
      await db.execute(`SELECT count(*) FROM ${table}`)
    )[0].getResultByIndex(0);
    Assert.equal(count, 0, `Empty table ${table}`);
  }

  for (let table of [
    "moz_places_metadata_snapshots",
    "moz_places_metadata_snapshots_extra",
    "moz_places_metadata_snapshots_groups",
    "moz_places_metadata_groups_to_snapshots",
    "moz_session_metadata",
    "moz_session_to_places",
  ]) {
    await Assert.rejects(
      db.execute(`SELECT count(*) FROM ${table}`),
      /no such table/,
      `Table ${table} should not exist`
    );
  }
});

add_task(async function scrolling_fields_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(
    `SELECT scrolling_time,scrolling_distance FROM moz_places_metadata`
  );
});

add_task(async function site_name_field_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(`SELECT site_name FROM moz_places`);
});

add_task(async function previews_tombstones_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(`SELECT hash FROM moz_previews_tombstones`);
});
