/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Keep in sync with `SyncedBookmarksMirror.jsm`.
const CURRENT_MIRROR_SCHEMA_VERSION = 9;

// The oldest schema version that we support. Any databases with schemas older
// than this will be dropped and recreated.
const OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION = 5;

async function getIndexNames(db, table, schema = "mirror") {
  let rows = await db.execute(`PRAGMA ${schema}.index_list(${table})`);
  let names = [];
  for (let row of rows) {
    // Column 4 is `c` if the index was created via `CREATE INDEX`, `u` if
    // via `UNIQUE`, and `pk` if via `PRIMARY KEY`.
    let wasCreated = row.getResultByIndex(3) == "c";
    if (wasCreated) {
      // Column 2 is the name of the index.
      names.push(row.getResultByIndex(1));
    }
  }
  return names.sort();
}

add_task(async function test_migrate_after_downgrade() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let dbFile = await setupFixtureFile("mirror_v5.sqlite");
  let oldBuf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordStepTelemetry() {},
    recordValidationTelemetry() {},
  });

  info("Downgrade schema version to oldest supported");
  await oldBuf.db.setSchemaVersion(
    OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION,
    "mirror"
  );
  await oldBuf.finalize();

  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordStepTelemetry() {},
    recordValidationTelemetry() {},
  });

  // All migrations between `OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION` should
  // be idempotent. When we downgrade, we roll back the schema version, but
  // leave the schema changes in place, since we can't anticipate what a
  // future version will change.
  let schemaVersion = await buf.db.getSchemaVersion("mirror");
  equal(
    schemaVersion,
    CURRENT_MIRROR_SCHEMA_VERSION,
    "Should upgrade downgraded mirror schema"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Migrations between 5 and 7 add three indexes.
add_task(async function test_migrate_from_5_to_current() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let dbFile = await setupFixtureFile("mirror_v5.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordStepTelemetry() {},
    recordValidationTelemetry() {},
  });

  let schemaVersion = await buf.db.getSchemaVersion("mirror");
  equal(
    schemaVersion,
    CURRENT_MIRROR_SCHEMA_VERSION,
    "Should upgrade mirror schema to current version"
  );

  let itemsIndexNames = await getIndexNames(buf.db, "items");
  deepEqual(
    itemsIndexNames,
    ["itemKeywords", "itemURLs"],
    "Should add two indexes on items"
  );

  let structureIndexNames = await getIndexNames(buf.db, "structure");
  deepEqual(
    structureIndexNames,
    ["structurePositions"],
    "Should add an index on structure"
  );

  let changesToUpload = await buf.apply();
  deepEqual(changesToUpload, {}, "Shouldn't flag any items for reupload");

  await assertLocalTree(
    PlacesUtils.bookmarks.menuGuid,
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [
        {
          guid: "bookmarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "A",
          url: "http://example.com/a",
        },
        {
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "B",
          url: "http://example.com/b",
          keyword: "hi",
        },
      ],
    },
    "Should apply mirror tree after migrating"
  );

  let keywordEntry = await PlacesUtils.keywords.fetch("hi");
  equal(
    keywordEntry.url.href,
    "http://example.com/b",
    "Should apply keyword from migrated mirror"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Migrations between 1 and 2 discard the entire database.
add_task(async function test_migrate_from_1_to_2() {
  let dbFile = await setupFixtureFile("mirror_v1.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
  });
  ok(
    buf.wasCorrupt,
    "Migrating from unsupported version should mark database as corrupt"
  );
  await buf.finalize();
});

add_task(async function test_database_corrupt() {
  let corruptFile = await setupFixtureFile("mirror_corrupt.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: corruptFile.path,
  });
  ok(buf.wasCorrupt, "Opening corrupt database should mark it as such");
  await buf.finalize();
});

add_task(async function test_migrate_v7_v9() {
  let buf = await openMirror("test_migrate_v7_v9");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        url: "http://example.com/a",
        title: "A",
      },
      {
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
      },
    ],
  });

  await buf.db.execute(
    `UPDATE moz_bookmarks
     SET syncChangeCounter = 0,
         syncStatus = ${PlacesUtils.bookmarks.SYNC_STATUS.NEW}`
  );

  // setup the mirror.
  await storeRecords(buf, [
    {
      id: "bookmarkAAAA",
      parentid: "menu",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b",
    },
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: [],
    },
  ]);

  await buf.db.setSchemaVersion(7, "mirror");
  await buf.finalize();

  // reopen it.
  buf = await openMirror("test_migrate_v7_v9");
  Assert.equal(await buf.db.getSchemaVersion("mirror"), 9, "did upgrade");

  let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
    "bookmarkAAAA",
    "bookmarkBBBB",
    PlacesUtils.bookmarks.menuGuid
  );
  let [fieldsA, fieldsB, fieldsMenu] = fields;

  // 'A' was in the mirror - should now be _NORMAL
  Assert.equal(fieldsA.guid, "bookmarkAAAA");
  Assert.equal(fieldsA.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);
  // 'B' was not in the mirror so should be untouched.
  Assert.equal(fieldsB.guid, "bookmarkBBBB");
  Assert.equal(fieldsB.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NEW);
  // 'menu' was in the mirror - should now be _NORMAL
  Assert.equal(fieldsMenu.guid, PlacesUtils.bookmarks.menuGuid);
  Assert.equal(fieldsMenu.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL);
  await buf.finalize();
});

add_task(async function test_migrate_v8_v9() {
  let dbFile = await setupFixtureFile("mirror_v8.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordStepTelemetry() {},
    recordValidationTelemetry() {},
  });

  Assert.equal(await buf.db.getSchemaVersion("mirror"), 9, "did upgrade");

  // Verify the new column is there
  Assert.ok(await buf.db.execute("SELECT unknownFields FROM items"));

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
