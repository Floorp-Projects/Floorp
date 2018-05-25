/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTags = [
  { folder: 123456,
    url: "place:folder=123456&type=7&queryType=1",
    title: "tag1",
    hash: "268505532566465",
  },
  { folder: 234567,
    url: "place:folder=234567&type=7&queryType=1&somethingelse",
    title: "tag2",
    hash: "268506675127932",
  },
  { folder: 345678,
    url: "place:type=7&folder=345678&queryType=1",
    title: "tag3",
    hash: "268506471927988",
  },
  // This will point to an invalid folder id.
  { folder: 456789,
    url: "place:type=7&folder=456789&queryType=1",
    expectedUrl: "place:type=7&invalidOldParentId=456789&queryType=1&excludeItems=1",
    title: "invalid",
    hash: "268505972797836",
  },
];
gTags.forEach(t => t.guid = t.title.padEnd(12, "_"));

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  for (let tag of gTags) {
    // We can reuse the same guid, it doesn't matter for this test.
    await db.execute(`INSERT INTO moz_places (url, guid, url_hash)
                      VALUES (:url, :guid, :hash)
                     `, { url: tag.url, guid: tag.guid, hash: tag.hash });
    if (tag.title != "invalid") {
      await db.execute(`INSERT INTO moz_bookmarks (id, fk, guid, title)
                      VALUES (:id, (SELECT id FROM moz_places WHERE guid = :guid), :guid, :title)
                      `, { id: tag.folder, guid: tag.guid, title: tag.title });
    }
  }

  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_queries_converted() {
  for (let tag of gTags) {
    let url = tag.title == "invalid" ? tag.expectedUrl : "place:tag=" + tag.title;
    let page = await PlacesUtils.history.fetch(tag.guid);
    Assert.equal(page.url.href, url);
  }
});

add_task(async function test_sync_fields() {
  let db = await PlacesUtils.promiseDBConnection();
  for (let tag of gTags) {
    if (tag.title != "invalid") {
      let rows = await db.execute(`
        SELECT syncChangeCounter
        FROM moz_bookmarks
        WHERE guid = :guid
      `, { guid: tag.guid });
      Assert.equal(rows[0].getResultByIndex(0), 2);
    }
  }
});
