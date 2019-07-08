/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let guid = "null".padEnd(12, "_");

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  // We can reuse the same guid, it doesn't matter for this test.

  await db.execute(
    `INSERT INTO moz_places (url, guid, url_hash)
                    VALUES (NULL, :guid, "123456")`,
    { guid }
  );
  await db.execute(
    `INSERT INTO moz_bookmarks (fk, guid)
                    VALUES ((SELECT id FROM moz_places WHERE guid = :guid), :guid)
                    `,
    { guid }
  );
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

  let page = await PlacesUtils.history.fetch(guid);
  Assert.equal(page.url.href, "place:excludeItems=1");

  let rows = await db.execute(
    `
    SELECT syncChangeCounter
    FROM moz_bookmarks
    WHERE guid = :guid
  `,
    { guid }
  );
  Assert.equal(rows[0].getResultByIndex(0), 2);
});
