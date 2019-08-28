/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Keep in sync with `SyncedBookmarksMirror.jsm`.
const CURRENT_MIRROR_SCHEMA_VERSION = 7;

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

// Migrations between 5 and 7 add three indexes.
add_task(async function test_migrate_from_5_to_current() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let dbFile = await setupFixtureFile("mirror_v5.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordTelemetryEvent(object, method, value, extra) {},
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
  let telemetryEvents = [];
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordTelemetryEvent(object, method, value, extra) {
      telemetryEvents.push({ object, method, value, extra });
    },
  });
  let dbFileSize = Math.floor((await OS.File.stat(dbFile.path)).size / 1024);
  deepEqual(telemetryEvents, [
    {
      object: "mirror",
      method: "open",
      value: "retry",
      extra: { why: "corrupt" },
    },
    {
      object: "mirror",
      method: "open",
      value: "success",
      extra: { size: dbFileSize.toString(10) },
    },
  ]);
  await buf.finalize();
});

add_task(async function test_database_corrupt() {
  let corruptFile = await setupFixtureFile("mirror_corrupt.sqlite");
  let telemetryEvents = [];
  let buf = await SyncedBookmarksMirror.open({
    path: corruptFile.path,
    recordTelemetryEvent(object, method, value, extra) {
      telemetryEvents.push({ object, method, value, extra });
    },
  });
  let dbFileSize = Math.floor(
    (await OS.File.stat(corruptFile.path)).size / 1024
  );
  deepEqual(telemetryEvents, [
    {
      object: "mirror",
      method: "open",
      value: "retry",
      extra: { why: "corrupt" },
    },
    {
      object: "mirror",
      method: "open",
      value: "success",
      extra: { size: dbFileSize.toString(10) },
    },
  ]);
  await buf.finalize();
});
