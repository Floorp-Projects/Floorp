/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file tests that a missing built-in folders (child of root) are correctly
 * fixed when the database is loaded.
 */

const ALL_ROOT_GUIDS = [
  PlacesUtils.bookmarks.menuGuid,
  PlacesUtils.bookmarks.unfiledGuid,
  PlacesUtils.bookmarks.toolbarGuid,
  PlacesUtils.bookmarks.tagsGuid,
  PlacesUtils.bookmarks.mobileGuid,
];

add_task(async function setup() {
  await setupPlacesDatabase([
    "migration",
    `places_v${Ci.nsINavHistoryService.DATABASE_SCHEMA_VERSION}.sqlite`,
  ]);

  // Prepare database contents by removing the tolbar and mobile folders.
  let path = PathUtils.join(PathUtils.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  await db.execute(
    `
    DELETE FROM moz_bookmarks WHERE guid IN(:toolbar, :mobile)
    `,
    {
      toolbar: PlacesUtils.bookmarks.toolbarGuid,
      mobile: PlacesUtils.bookmarks.mobileGuid,
    }
  );
  await db.close();
});

add_task(async function test_database_recreates_roots() {
  Assert.ok(
    PlacesUtils.history.databaseStatus ==
      PlacesUtils.history.DATABASE_STATUS_OK ||
      PlacesUtils.history.databaseStatus ==
        PlacesUtils.history.DATABASE_STATUS_UPGRADED,
    "Should successfully access the database for the first time"
  );

  let db = await PlacesUtils.promiseDBConnection();
  let rootId = await PlacesTestUtils.promiseItemId(
    PlacesUtils.bookmarks.rootGuid
  );
  for (let guid of ALL_ROOT_GUIDS) {
    let rows = await db.execute(
      `
      SELECT id, parent FROM moz_bookmarks
      WHERE guid = :guid
    `,
      { guid }
    );

    Assert.equal(rows.length, 1, "Should have exactly one row for the root");

    Assert.equal(
      rows[0].getResultByName("parent"),
      rootId,
      "Should have been created with the correct parent"
    );

    let root = await PlacesUtils.bookmarks.fetch(guid);

    Assert.equal(root.guid, guid, "GUIDs should match");
    Assert.equal(
      root.parentGuid,
      PlacesUtils.bookmarks.rootGuid,
      "Should have the correct parent GUID"
    );
    Assert.equal(
      root.type,
      PlacesUtils.bookmarks.TYPE_FOLDER,
      "Should have the correct type"
    );

    let id = rows[0].getResultByName("id");
    Assert.equal(
      await PlacesTestUtils.promiseItemId(guid),
      id,
      "Should return the correct id from promiseItemId"
    );
    Assert.equal(
      await PlacesTestUtils.promiseItemGuid(id),
      guid,
      "Should return the correct guid from promiseItemGuid"
    );
  }

  let rows = await db.execute(
    `
    SELECT 1 FROM moz_bookmarks
    WHERE parent = (SELECT id from moz_bookmarks WHERE guid = :guid)
  `,
    {
      guid: PlacesUtils.bookmarks.rootGuid,
    }
  );

  Assert.equal(
    rows.length,
    ALL_ROOT_GUIDS.length,
    "Root folder should have the expected number of children"
  );
});
