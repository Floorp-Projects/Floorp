add_task(async function setup() {
  await setupPlacesDatabase("places_v61.sqlite");
  // Since this migration doesn't affect favicons.sqlite, we can reuse v41.
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
});

add_task(async function builder_fields_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(
    `SELECT builder, builder_data FROM moz_places_metadata_snapshots_groups`
  );
});

add_task(async function indexes_in_database() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(
    `SELECT * FROM sqlite_master WHERE type = "index"`
  );

  let indexes = rows.map(r => r.getResultByName("name"));

  Assert.ok(
    indexes.includes("moz_places_metadata_referrerindex"),
    "Should contain the referrer index"
  );
  Assert.ok(
    indexes.includes("moz_places_metadata_snapshots_pinnedindex"),
    "Should contain the pinned index"
  );
  Assert.ok(
    indexes.includes("moz_places_metadata_snapshots_extra_typeindex"),
    "Should contain the type index"
  );
});
