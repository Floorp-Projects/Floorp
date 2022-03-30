/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  let path = await setupPlacesDatabase("places_v65.sqlite");
  // Since this migration doesn't affect favicons.sqlite, we can reuse v41.
  await setupPlacesDatabase("favicons_v41.sqlite", "favicons.sqlite");
  let db = await Sqlite.openConnection({ path });
  await db.execute(`
    INSERT INTO moz_places_metadata_snapshots_groups (title, builder, builder_data)
    VALUES
      ('', 'test1', json_object('fluentTitle', 'test1')),
      ('test2', 'test2', NULL),
      ('test3', 'test3', json_object())
  `);
  await db.close();
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

add_task(async function snapshot_groups_nullable_title_field() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    await db.execute(
      `INSERT INTO moz_places_metadata_snapshots_groups (title, builder, builder_data)
        VALUES (NULL, "test", json_object('title', 'test'))`
    );
    let rows = await db.execute(
      `SELECT * FROM moz_places_metadata_snapshots_groups`
    );
    for (let row of rows) {
      let title = row.getResultByName("title");
      Assert.strictEqual(title, null);
      let builder_data = JSON.parse(row.getResultByName("builder_data"));
      Assert.ok(
        builder_data.fluentTitle || builder_data.title,
        "Check there's always a title or fluentTitle in the data"
      );
    }
  });
});
