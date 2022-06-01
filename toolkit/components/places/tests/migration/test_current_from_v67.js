/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  const path = await setupPlacesDatabase("places_v67.sqlite");
  const db = await Sqlite.openConnection({ path });
  await db.execute(
    `INSERT INTO moz_places (url, guid, url_hash)
     VALUES
      ('https://test1.com', '___________1', "123456"),
      ('https://test2.com', '___________2', "123456")
    `
  );
  await db.execute(`
    INSERT INTO moz_places_metadata_snapshots (place_id, created_at, removed_at, first_interaction_at, last_interaction_at)
    VALUES
      ((SELECT id FROM moz_places WHERE guid = '___________1'), 1, 1, 1, 1),
      ((SELECT id FROM moz_places WHERE guid = '___________2'), 1, NULL, 1, 1)
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

add_task(async function test() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    let rows = await db.execute(
      `SELECT * FROM moz_places_metadata_snapshots WHERE
       (removed_at ISNULL) <> (removed_reason ISNULL)`
    );
    Assert.equal(rows.length, 0);

    rows = await db.execute(
      `SELECT removed_reason FROM moz_places_metadata_snapshots WHERE
      place_id = (SELECT id FROM moz_places WHERE guid = '___________1')`
    );
    Assert.equal(rows[0].getResultByName("removed_reason"), 0);
  });
});
