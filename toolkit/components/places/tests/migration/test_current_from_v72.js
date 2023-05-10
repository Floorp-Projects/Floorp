/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPlacesDatabase("places_v72.sqlite");
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);

  await db.execute(
    "SELECT recalc_frecency, alt_frecency, recalc_alt_frecency FROM moz_origins"
  );

  await db.execute("SELECT alt_frecency, recalc_alt_frecency FROM moz_places");
  Assert.ok(
    await db.indexExists("moz_places_altfrecencyindex"),
    "Should have created an index"
  );
});
