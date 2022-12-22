/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  const path = await setupPlacesDatabase("places_v69.sqlite");

  const db = await Sqlite.openConnection({ path });
  await db.execute(`
    INSERT INTO moz_places (url, guid, url_hash, origin_id, frecency)
    VALUES
      ('https://test1.com', '___________1', '123456', 100, 0),
      ('https://test2.com', '___________2', '123456', 101, -1),
      ('https://test3.com', '___________3', '123456', 102, -1234)
    `);
  await db.execute(`
    INSERT INTO moz_origins (id, prefix, host, frecency)
    VALUES
      (100, 'https://', 'test1.com', 0),
      (101, 'https://', 'test2.com', 0),
      (102, 'https://', 'test3.com', 0)
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
});

add_task(async function moz_historyvisits() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    function expectedFrecency(guid) {
      switch (guid) {
        case "___________1":
          return 0;
        case "___________2":
          return -1;
        case "___________3":
          return 1234;
        default:
          throw new Error("Unknown guid");
      }
    }
    const rows = await db.execute(
      "SELECT guid, frecency FROM moz_places WHERE url_hash = '123456'"
    );
    for (let row of rows) {
      Assert.equal(
        row.getResultByName("frecency"),
        expectedFrecency(row.getResultByName("guid")),
        "Check expected frecency"
      );
    }
    const origins = new Map(
      (await db.execute("SELECT host, frecency FROM moz_origins")).map(r => [
        r.getResultByName("host"),
        r.getResultByName("frecency"),
      ])
    );
    Assert.equal(origins.get("test1.com"), 0);
    Assert.equal(origins.get("test2.com"), 0);
    Assert.equal(origins.get("test3.com"), 1234);

    const statSum = (
      await db.execute(
        "SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum'"
      )
    )[0].getResultByName("value");
    const sum = (
      await db.execute(
        "SELECT SUM(frecency) AS sum from moz_origins WHERE frecency > 0"
      )
    )[0].getResultByName("sum");
    Assert.equal(sum, statSum, "Check stats were updated");
  });
});
