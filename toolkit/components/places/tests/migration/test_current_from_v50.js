/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_GUID = "null".padEnd(11, "_");
const LAST_USED_ANNO = "bookmarkPropertiesDialog/folderLastUsed";
const LAST_USED_META_DATA = "places/bookmarks/edit/lastusedfolder";

let expectedGuids = [];

async function adjustIndices(db, itemGuid) {
  await db.execute(
    `
    UPDATE moz_bookmarks SET
      position = position - 1
    WHERE parent = (SELECT parent FROM moz_bookmarks
                    WHERE guid = :itemGuid) AND
          position >= (SELECT position FROM moz_bookmarks
                       WHERE guid = :itemGuid)`,
    { itemGuid }
  );
}

async function fetchChildInfos(db, parentGuid) {
  let rows = await db.execute(
    `
    SELECT b.guid, b.position, b.syncChangeCounter
    FROM moz_bookmarks b
    JOIN moz_bookmarks p ON p.id = b.parent
    WHERE p.guid = :parentGuid
    ORDER BY b.position`,
    { parentGuid }
  );
  return rows.map(row => ({
    guid: row.getResultByName("guid"),
    position: row.getResultByName("position"),
    syncChangeCounter: row.getResultByName("syncChangeCounter"),
  }));
}

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  // We can reuse the same guid, it doesn't matter for this test.
  await db.execute(
    `INSERT INTO moz_anno_attributes (name)
                    VALUES (:last_used_anno)`,
    { last_used_anno: LAST_USED_ANNO }
  );

  for (let i = 0; i < 3; i++) {
    let guid = `${BASE_GUID}${i}`;
    await db.execute(
      `INSERT INTO moz_bookmarks (guid, type)
                      VALUES (:guid, :type)
                      `,
      { guid, type: PlacesUtils.bookmarks.TYPE_FOLDER }
    );
    await db.execute(
      `INSERT INTO moz_items_annos (item_id, anno_attribute_id, content)
                      VALUES ((SELECT id FROM moz_bookmarks WHERE guid = :guid),
                              (SELECT id FROM moz_anno_attributes WHERE name = :last_used_anno),
                              :content)`,
      {
        guid,
        content: new Date(1517318477569) - (3 - i) * 60 * 60 * 1000,
        last_used_anno: LAST_USED_ANNO,
      }
    );
    expectedGuids.unshift(guid);
  }

  info("Move menu into unfiled");
  await adjustIndices(db, "menu________");
  await db.execute(
    `
    UPDATE moz_bookmarks SET
      parent = (SELECT id FROM moz_bookmarks WHERE guid = :newParentGuid),
      position = IFNULL((SELECT MAX(position) + 1 FROM moz_bookmarks
                         WHERE guid = :newParentGuid), 0)
    WHERE guid = :itemGuid`,
    { newParentGuid: "unfiled_____", itemGuid: "menu________" }
  );

  info("Move toolbar into mobile");
  let mobileChildren = [
    "bookmarkAAAA",
    "bookmarkBBBB",
    "toolbar_____",
    "bookmarkCCCC",
    "bookmarkDDDD",
  ];
  await adjustIndices(db, "toolbar_____");
  for (let position = 0; position < mobileChildren.length; position++) {
    await db.execute(
      `
      INSERT INTO moz_bookmarks(guid, parent, position)
      VALUES(:guid, (SELECT id FROM moz_bookmarks WHERE guid = 'mobile______'),
             :position)
      ON CONFLICT(guid) DO UPDATE SET
        parent = excluded.parent,
        position = excluded.position`,
      { guid: mobileChildren[position], position }
    );
  }

  info("Reset Sync change counters");
  await db.execute(`UPDATE moz_bookmarks SET syncChangeCounter = 0`);

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

add_task(async function test_roots_fixed() {
  let db = await PlacesUtils.promiseDBConnection();

  let expectedRootInfos = [
    {
      guid: PlacesUtils.bookmarks.tagsGuid,
      position: 0,
      syncChangeCounter: 0,
    },
    {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      position: 1,
      syncChangeCounter: 1,
    },
    {
      guid: PlacesUtils.bookmarks.mobileGuid,
      position: 2,
      syncChangeCounter: 1,
    },
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      position: 3,
      syncChangeCounter: 1,
    },
    {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      position: 4,
      syncChangeCounter: 1,
    },
  ];
  Assert.deepEqual(
    expectedRootInfos,
    await fetchChildInfos(db, PlacesUtils.bookmarks.rootGuid),
    "All roots should be reparented to the Places root"
  );

  let expectedMobileInfos = [
    {
      guid: "bookmarkAAAA",
      position: 0,
      syncChangeCounter: 0,
    },
    {
      guid: "bookmarkBBBB",
      position: 1,
      syncChangeCounter: 0,
    },
    {
      guid: "bookmarkCCCC",
      position: 2,
      syncChangeCounter: 0,
    },
    {
      guid: "bookmarkDDDD",
      position: 3,
      syncChangeCounter: 0,
    },
  ];
  Assert.deepEqual(
    expectedMobileInfos,
    await fetchChildInfos(db, PlacesUtils.bookmarks.mobileGuid),
    "Should fix misparented root sibling positions"
  );
});
