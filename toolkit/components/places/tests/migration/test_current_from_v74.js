/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  let path = await setupPlacesDatabase("places_v74.sqlite");
  let db = await Sqlite.openConnection({ path });
  await db.execute(`
  INSERT INTO moz_origins (id, prefix, host, frecency, recalc_frecency)
  VALUES
    (100, 'https://', 'test1.com', 0, 0),
    (101, 'https://', 'test2.com', 0, 0),
    (102, 'https://', 'test3.com', 0, 0)
  `);
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

  await db.execute("SELECT * FROM moz_places_extra");
  await db.execute("SELECT * from moz_historyvisits_extra");
});

add_task(async function recalc_origins_frecency() {
  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);

  Assert.equal(
    (
      await db.execute(
        "SELECT count(*) FROM moz_origins WHERE recalc_frecency = 0"
      )
    )[0].getResultByIndex(0),
    0,
    "All entries should be set for recalculation"
  );
});
