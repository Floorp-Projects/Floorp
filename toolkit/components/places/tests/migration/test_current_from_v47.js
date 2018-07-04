/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");
});


// Accessing the database for the first time should trigger migration, and the
// schema version should be updated.
add_task(async function database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);

  // Now wait for moz_origins.frecency to be populated before continuing with
  // other test tasks.
  await TestUtils.waitForCondition(() => {
    return !Services.prefs.getBoolPref("places.database.migrateV52OriginFrecencies", false);
  }, "Waiting for v52 origin frecencies to be migrated", 100, 3000);
});


// moz_origins should be populated.
add_task(async function test_origins() {
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
  let frecenciesByOriginID = {};

  // Make sure moz_places.origin_id refers to the right origins.
  for (let row of rows) {
    let originID = row.getResultByName("origin_id");
    let origin = origins.find(o => o.id == originID);
    Assert.ok(origin);
    Assert.equal(origin.prefix, row.getResultByName("prefix"));
    Assert.equal(origin.host, row.getResultByName("host"));

    seenOriginIDs.push(originID);

    let frecency = row.getResultByName("frecency");
    frecenciesByOriginID[originID] = frecenciesByOriginID[originID] || 0;
    frecenciesByOriginID[originID] += frecency;
  }

  for (let origin of origins) {
    // Make sure each origin corresponds to at least one moz_place.
    Assert.ok(seenOriginIDs.includes(origin.id));

    // moz_origins.frecency should be the sum of frecencies of all moz_places
    // with the origin.
    Assert.equal(origin.frecency, frecenciesByOriginID[origin.id]);
  }

  // Make sure moz_hosts was emptied.
  rows = await db.execute(`
    SELECT *
    FROM moz_hosts;
  `);
  Assert.equal(rows.length, 0);
});


// Frecency stats should have been collected.
add_task(async function test_frecency_stats() {
  let db = await PlacesUtils.promiseDBConnection();

  // Collect positive frecencies from moz_origins.
  let rows = await db.execute(`
    SELECT frecency FROM moz_origins WHERE frecency > 0
  `);
  Assert.notEqual(rows.length, 0);
  let frecencies = rows.map(r => r.getResultByName("frecency"));

  // Collect stats.
  rows = await db.execute(`
    SELECT
      (SELECT value FROM moz_meta WHERE key = "origin_frecency_count"),
      (SELECT value FROM moz_meta WHERE key = "origin_frecency_sum"),
      (SELECT value FROM moz_meta WHERE key = "origin_frecency_sum_of_squares")
  `);
  let count = rows[0].getResultByIndex(0);
  let sum = rows[0].getResultByIndex(1);
  let squares = rows[0].getResultByIndex(2);

  Assert.equal(count, frecencies.length);
  Assert.equal(sum, frecencies.reduce((memo, f) => memo + f, 0));
  Assert.equal(squares, frecencies.reduce((memo, f) => memo + (f * f), 0));
});
