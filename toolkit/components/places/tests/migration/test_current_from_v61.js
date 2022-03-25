/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

add_task(async function title_field_in_snapshots() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(`SELECT title FROM moz_places_metadata_snapshots`);
});

add_task(async function hidden_field_in_snapshots_groups() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(`SELECT hidden FROM moz_places_metadata_snapshots_groups`);
});

add_task(async function hidden_field_in_snapshots_groups_to_snapshots() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(
    `SELECT hidden FROM moz_places_metadata_groups_to_snapshots`
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
