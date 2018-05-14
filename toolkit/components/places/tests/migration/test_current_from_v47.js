/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_origins() {
  await TestUtils.waitForCondition(() => {
    return !Services.prefs.getBoolPref("places.database.migrateV48Frecencies", false);
  }, "Waiting for v48 origin frecencies to be migrated", 100, 3000);

  let db = await PlacesUtils.promiseDBConnection();

  // Collect origins.
  let rows = await db.execute(`
    SELECT id, prefix, host, frecency
    FROM moz_origins
    ORDER BY id ASC;
  `);
  Assert.notEqual(rows.length, 0);
  let origins = rows.map(r => ({
    id: r.getResultByName("id"),
    prefix: r.getResultByName("prefix"),
    host: r.getResultByName("host"),
    frecency: r.getResultByName("frecency"),
  }));

  // Get moz_places.
  rows = await db.execute(`
    SELECT get_prefix(url) AS prefix, get_host_and_port(url) AS host,
           origin_id, frecency
    FROM moz_places;
  `);
  Assert.notEqual(rows.length, 0);

  let seenOriginIDs = [];
  let maxFrecencyByOriginID = {};

  // Make sure moz_places.origin_id refers to the right origins.
  for (let row of rows) {
    let originID = row.getResultByName("origin_id");
    let origin = origins.find(o => o.id == originID);
    Assert.ok(origin);
    Assert.equal(origin.prefix, row.getResultByName("prefix"));
    Assert.equal(origin.host, row.getResultByName("host"));

    seenOriginIDs.push(originID);

    let frecency = row.getResultByName("frecency");
    if (!(originID in maxFrecencyByOriginID)) {
      maxFrecencyByOriginID[originID] = frecency;
    } else {
      maxFrecencyByOriginID[originID] =
        Math.max(frecency, maxFrecencyByOriginID[originID]);
    }
  }

  for (let origin of origins) {
    // Make sure each origin corresponds to at least one moz_place.
    Assert.ok(seenOriginIDs.includes(origin.id));

    // moz_origins.frecency should be the max frecency of all moz_places with
    // the origin.
    Assert.equal(origin.frecency, maxFrecencyByOriginID[origin.id]);
  }

  // Make sure moz_hosts was emptied.
  rows = await db.execute(`
    SELECT *
    FROM moz_hosts;
  `);
  Assert.equal(rows.length, 0);
});
