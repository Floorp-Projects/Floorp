/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  const path = await setupPlacesDatabase("places_v68.sqlite");

  const db = await Sqlite.openConnection({ path });
  await db.execute("INSERT INTO moz_historyvisits (from_visit) VALUES (-1)");
  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);
});

add_task(async function moz_historyvisits() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    const rows = await db.execute(
      "SELECT * FROM moz_historyvisits WHERE from_visit=-1"
    );

    Assert.equal(rows.length, 1);
    Assert.equal(rows[0].getResultByName("source"), 0);
    Assert.equal(rows[0].getResultByName("triggeringPlaceId"), null);
  });
});
