/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a corrupt places schema.

add_task(async function() {
  let path = await setupPlacesDatabase("../migration/favicons_v41.sqlite");

  // Ensure the database will go through a migration that depends on moz_places
  // and break the schema by dropping that table.
  let db = await Sqlite.openConnection({ path });
  await db.setSchemaVersion(38);
  await db.execute("DROP TABLE moz_icons");
  await db.close();

  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CORRUPT
  );
  db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute("SELECT 1 FROM moz_icons");
  Assert.equal(rows.length, 0, "Found no icons");
});
