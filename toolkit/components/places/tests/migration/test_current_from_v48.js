/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gCreatedParentGuid = "m47___FOLDER";

const gTestItems = [
  {
    // Folder shortcuts to built-in folders.
    guid: "m47_____ROOT",
    url: "place:folder=PLACES_ROOT",
    targetParentGuid: "rootGuid",
  },
  {
    guid: "m47_____MENU",
    url: "place:folder=BOOKMARKS_MENU",
    targetParentGuid: "menuGuid",
  },
  {
    guid: "m47_____TAGS",
    url: "place:folder=TAGS",
    targetParentGuid: "tagsGuid",
  },
  {
    guid: "m47____OTHER",
    url: "place:folder=UNFILED_BOOKMARKS",
    targetParentGuid: "unfiledGuid",
  },
  {
    guid: "m47__TOOLBAR",
    url: "place:folder=TOOLBAR",
    targetParentGuid: "toolbarGuid",
  },
  {
    guid: "m47___MOBILE",
    url: "place:folder=MOBILE_BOOKMARKS",
    targetParentGuid: "mobileGuid",
  },
  {
    // Folder shortcut to using id.
    guid: "m47_______ID",
    url: "place:folder=%id%",
    expectedUrl: "place:parent=%guid%",
  },
  {
    // Folder shortcut to multiple folders.
    guid: "m47____MULTI",
    url: "place:folder=TOOLBAR&folder=%id%&sort=1",
    expectedUrl: "place:parent=%toolbarGuid%&parent=%guid%&sort=1",
  },
  {
    // Folder shortcut to non-existent folder.
    guid: "m47______NON",
    url: "place:folder=454554545",
    expectedUrl: "place:invalidOldParentId=454554545&excludeItems=1",
  },
];

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  let rows = await db.execute(
    `SELECT id FROM moz_bookmarks
                               WHERE guid = :guid`,
    { guid: PlacesUtils.bookmarks.unfiledGuid }
  );

  let unfiledId = rows[0].getResultByName("id");

  // Insert a test folder.
  await db.execute(
    `INSERT INTO moz_bookmarks (guid, title, parent)
                    VALUES (:guid, "Folder", :parent)`,
    { guid: gCreatedParentGuid, parent: unfiledId }
  );

  rows = await db.execute(
    `SELECT id FROM moz_bookmarks
                           WHERE guid = :guid`,
    { guid: gCreatedParentGuid }
  );

  let createdFolderId = rows[0].getResultByName("id");

  for (let item of gTestItems) {
    item.url = item.url.replace("%id%", createdFolderId);

    // We can reuse the same guid, it doesn't matter for this test.
    await db.execute(
      `INSERT INTO moz_places (url, guid, url_hash)
                      VALUES (:url, :guid, :hash)
                     `,
      {
        url: item.url,
        guid: item.guid,
        hash: PlacesUtils.history.hashURL(item.url),
      }
    );
    await db.execute(
      `INSERT INTO moz_bookmarks (id, fk, guid, title, parent)
                      VALUES (:id, (SELECT id FROM moz_places WHERE guid = :guid),
                              :guid, :title,
                              (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid))
                    `,
      {
        id: item.folder,
        guid: item.guid,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: item.guid,
      }
    );
  }

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

add_task(async function test_correct_folder_queries() {
  for (let item of gTestItems) {
    let bm = await PlacesUtils.bookmarks.fetch(item.guid);

    if (item.targetParentGuid) {
      Assert.equal(
        bm.url,
        `place:parent=${PlacesUtils.bookmarks[item.targetParentGuid]}`,
        `Should have updated the URL for ${item.guid}`
      );
    } else {
      let expected = item.expectedUrl
        .replace("%guid%", gCreatedParentGuid)
        .replace("%toolbarGuid%", PlacesUtils.bookmarks.toolbarGuid);

      Assert.equal(
        bm.url,
        expected,
        `Should have updated the URL for ${item.guid}`
      );
    }
  }
});

add_task(async function test_hashes_valid() {
  let db = await PlacesUtils.promiseDBConnection();
  // Ensure all the hashes in moz_places are valid.
  let rows = await db.execute(`SELECT url, url_hash FROM moz_places`);

  for (let row of rows) {
    let url = row.getResultByName("url");
    let url_hash = row.getResultByName("url_hash");
    Assert.equal(
      url_hash,
      PlacesUtils.history.hashURL(url),
      `url hash should be correct for ${url}`
    );
  }
});

add_task(async function test_sync_counters_updated() {
  let db = await PlacesUtils.promiseDBConnection();

  for (let test of gTestItems) {
    let rows = await db.execute(
      `SELECT syncChangeCounter FROM moz_bookmarks
                                 WHERE guid = :guid`,
      { guid: test.guid }
    );

    Assert.equal(rows.length, 1, `Should only be one record for ${test.guid}`);
    Assert.equal(
      rows[0].getResultByName("syncChangeCounter"),
      2,
      `Should have bumped the syncChangeCounter for ${test.guid}`
    );
  }
});
