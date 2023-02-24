/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  let path = await setupPlacesDatabase("places_v70.sqlite");

  let db = await Sqlite.openConnection({ path });
  await db.execute(`
    INSERT INTO moz_places (url, guid, url_hash, origin_id, frecency, foreign_count)
    VALUES
      ('https://test1.com', '___________1', '123456', 100, 0, 2),
      ('https://test2.com', '___________2', '123456', 101, -1, 2),
      ('https://test3.com', '___________3', '123456', 102, -1234, 1)
    `);
  await db.execute(`
    INSERT INTO moz_origins (id, prefix, host, frecency)
    VALUES
      (100, 'https://', 'test1.com', 0),
      (101, 'https://', 'test2.com', 0),
      (102, 'https://', 'test3.com', 0)
    `);
  await db.execute(
    `INSERT INTO moz_session_metadata
      (id, guid)
      VALUES (0, "0")
    `
  );

  await db.execute(
    `INSERT INTO moz_places_metadata_snapshots
      (place_id, created_at, first_interaction_at, last_interaction_at)
      VALUES ((SELECT id FROM moz_places WHERE guid = :guid), 0, 0, 0)
    `,
    { guid: "___________1" }
  );
  await db.execute(
    `INSERT INTO moz_bookmarks
      (fk, guid)
      VALUES ((SELECT id FROM moz_places WHERE guid = :guid), :guid)
    `,
    { guid: "___________1" }
  );

  await db.execute(
    `INSERT INTO moz_places_metadata_snapshots
      (place_id, created_at, first_interaction_at, last_interaction_at)
      VALUES ((SELECT id FROM moz_places WHERE guid = :guid), 0, 0, 0)
    `,
    { guid: "___________2" }
  );
  await db.execute(
    `INSERT INTO moz_session_to_places
      (session_id, place_id)
      VALUES (0, (SELECT id FROM moz_places WHERE guid = :guid))
    `,
    { guid: "___________2" }
  );

  await db.execute(
    `INSERT INTO moz_session_to_places
      (session_id, place_id)
      VALUES (0, (SELECT id FROM moz_places WHERE guid = :guid))
    `,
    { guid: "___________3" }
  );

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

  let rows = await db.execute("SELECT guid, foreign_count FROM moz_places");
  for (let row of rows) {
    let guid = row.getResultByName("guid");
    let count = row.getResultByName("foreign_count");
    if (guid == "___________1") {
      Assert.equal(count, 1, "test1 should have the correct foreign_count");
    }
    if (guid == "___________2") {
      Assert.equal(count, 0, "test2 should have the correct foreign_count");
    }
    if (guid == "___________3") {
      Assert.equal(count, 0, "test3 should have the correct foreign_count");
    }
  }
});
