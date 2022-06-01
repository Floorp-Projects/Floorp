/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  const path = await setupPlacesDatabase("places_v66.sqlite");

  const db = await Sqlite.openConnection({ path });
  await db.execute(`
    INSERT INTO moz_inputhistory (input, use_count, place_id)
    VALUES
      ('abc', 1, 1),
      ('aBc', 0.9, 1),
      ('ABC', 5, 1),
      ('ABC', 1, 2),
      ('DEF', 1, 3)
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

add_task(async function moz_inputhistory() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    const rows = await db.execute(
      "SELECT * FROM moz_inputhistory ORDER BY place_id"
    );

    Assert.equal(rows.length, 3);

    Assert.equal(rows[0].getResultByName("place_id"), 1);
    Assert.equal(rows[0].getResultByName("input"), "abc");
    Assert.equal(rows[0].getResultByName("use_count"), 5);

    Assert.equal(rows[1].getResultByName("place_id"), 2);
    Assert.equal(rows[1].getResultByName("input"), "abc");
    Assert.equal(rows[1].getResultByName("use_count"), 1);

    Assert.equal(rows[2].getResultByName("place_id"), 3);
    Assert.equal(rows[2].getResultByName("input"), "def");
    Assert.equal(rows[2].getResultByName("use_count"), 1);
  });
});
