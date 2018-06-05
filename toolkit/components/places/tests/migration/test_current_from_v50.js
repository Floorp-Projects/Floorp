/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_GUID = "null".padEnd(11, "_");
const LAST_USED_ANNO = "bookmarkPropertiesDialog/folderLastUsed";
const LAST_USED_META_DATA = "places/bookmarks/edit/lastusedfolder";

let expectedGuids = [];

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  // We can reuse the same guid, it doesn't matter for this test.
  await db.execute(`INSERT INTO moz_anno_attributes (name)
                    VALUES (:last_used_anno)`, { last_used_anno: LAST_USED_ANNO });

  for (let i = 0; i < 3; i++) {
    let guid = `${BASE_GUID}${i}`;
    await db.execute(`INSERT INTO moz_bookmarks (guid, type)
                      VALUES (:guid, :type)
                      `, { guid, type: PlacesUtils.bookmarks.TYPE_FOLDER });
    await db.execute(`INSERT INTO moz_items_annos (item_id, anno_attribute_id, content)
                      VALUES ((SELECT id FROM moz_bookmarks WHERE guid = :guid),
                              (SELECT id FROM moz_anno_attributes WHERE name = :last_used_anno),
                              :content)`, {
      guid,
      content: new Date(1517318477569) - (3 - i) * 60 * 60 * 1000,
      last_used_anno: LAST_USED_ANNO,
    });
    expectedGuids.unshift(guid);
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

add_task(async function test_folders_migrated() {
  let metaData = await PlacesUtils.metadata.get(LAST_USED_META_DATA);

  Assert.deepEqual(JSON.parse(metaData), expectedGuids);
});

add_task(async function test_annotations_removed() {
  let db = await PlacesUtils.promiseDBConnection();

  await assertAnnotationsRemoved(db, [LAST_USED_ANNO]);
});

add_task(async function test_no_orphan_annotations() {
  let db = await PlacesUtils.promiseDBConnection();

  await assertNoOrphanAnnotations(db);
});
