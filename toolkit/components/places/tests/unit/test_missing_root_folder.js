/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file tests that a missing root folder is correctly fixed when the
 * database is loaded.
 */

const ALL_ROOT_GUIDS = [
  PlacesUtils.bookmarks.menuGuid,
  PlacesUtils.bookmarks.unfiledGuid,
  PlacesUtils.bookmarks.toolbarGuid,
  PlacesUtils.bookmarks.tagsGuid,
  PlacesUtils.bookmarks.mobileGuid,
];

add_task(async function setup() {
  // This file has no root folder.
  await setupPlacesDatabase("noRoot.sqlite");

  // Check database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  let rows = await db.execute(`
    SELECT guid FROM moz_bookmarks
    WHERE guid = :guid
  `, {
    guid: PlacesUtils.bookmarks.rootGuid
  });

  Assert.equal(rows.length, 0, "Root folder should not exist");

  await db.close();
});

add_task(async function test_database_recreates_roots() {
  Assert.ok(PlacesUtils.history.databaseStatus == PlacesUtils.history.DATABASE_STATUS_OK ||
    PlacesUtils.history.databaseStatus == PlacesUtils.history.DATABASE_STATUS_UPGRADED,
    "Should successfully access the database for the first time");

  let db = await PlacesUtils.promiseDBConnection();

  let rows = await db.execute(`
    SELECT id, parent, type FROM moz_bookmarks
    WHERE guid = :guid
  `, {guid: PlacesUtils.bookmarks.rootGuid});

  Assert.equal(rows.length, 1, "Should have added exactly one root");
  Assert.greaterOrEqual(rows[0].getResultByName("id"), 1,
    "Should have a valid root Id");
  Assert.equal(rows[0].getResultByName("parent"), 0,
    "Should have a parent of id 0");
  Assert.equal(rows[0].getResultByName("type"), PlacesUtils.bookmarks.TYPE_FOLDER,
    "Should have a type of folder");

  let id = rows[0].getResultByName("id");
  Assert.equal(await PlacesUtils.promiseItemId(PlacesUtils.bookmarks.rootGuid), id,
    "Should return the correct id from promiseItemId");
  Assert.equal(await PlacesUtils.promiseItemGuid(id), PlacesUtils.bookmarks.rootGuid,
    "Should return the correct guid from promiseItemGuid");

  // Note: Currently we do not fix the parent of the folders on initial startup.
  // There is a maintenance task that will do it, hence we don't check the parents
  // here, just that the built-in folders correctly exist and haven't been
  // duplicated.
  for (let guid of ALL_ROOT_GUIDS) {
    rows = await db.execute(`
      SELECT id FROM moz_bookmarks
      WHERE guid = :guid
    `, {guid});

    Assert.equal(rows.length, 1, "Should have exactly one row for the root");

    let root = await PlacesUtils.bookmarks.fetch(guid);

    Assert.equal(root.guid, guid, "GUIDs should match");
  }
});
