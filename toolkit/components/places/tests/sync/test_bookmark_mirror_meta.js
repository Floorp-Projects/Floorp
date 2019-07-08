/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_highWaterMark() {
  let buf = await openMirror("highWaterMark");

  strictEqual(
    await buf.getCollectionHighWaterMark(),
    0,
    "High water mark should be 0 without items"
  );

  await buf.setCollectionLastModified(123.45);
  equal(
    await buf.getCollectionHighWaterMark(),
    123.45,
    "High water mark should be last modified time without items"
  );

  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: [],
      modified: 50,
    },
    {
      id: "toolbar",
      parentid: "places",
      type: "folder",
      children: [],
      modified: 123.95,
    },
  ]);
  equal(
    await buf.getCollectionHighWaterMark(),
    123.45,
    "High water mark should be last modified time if items are older"
  );

  await storeRecords(buf, [
    {
      id: "unfiled",
      parentid: "places",
      type: "folder",
      children: [],
      modified: 125.45,
    },
  ]);
  equal(
    await buf.getCollectionHighWaterMark(),
    124.45,
    "High water mark should be modified time - 1s of newest record if exists"
  );

  await buf.finalize();
});

add_task(async function test_ensureCurrentSyncId() {
  let buf = await openMirror("ensureCurrentSyncId");

  await buf.ensureCurrentSyncId("syncIdAAAAAA");
  equal(
    await buf.getCollectionHighWaterMark(),
    0,
    "High water mark should be 0 after setting sync ID"
  );

  info("Insert items and set collection last modified");
  await storeRecords(
    buf,
    [
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA"],
        modified: 125.45,
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        children: [],
      },
    ],
    { needsMerge: false }
  );
  await buf.setCollectionLastModified(123.45);

  info("Set matching sync ID");
  await buf.ensureCurrentSyncId("syncIdAAAAAA");
  {
    equal(
      await buf.getSyncId(),
      "syncIdAAAAAA",
      "Should return existing sync ID"
    );
    strictEqual(
      await buf.getCollectionHighWaterMark(),
      124.45,
      "Different sync ID should reset high water mark"
    );

    let itemRows = await buf.db.execute(`
      SELECT guid, needsMerge FROM items
      ORDER BY guid`);
    let itemInfos = itemRows.map(row => ({
      guid: row.getResultByName("guid"),
      needsMerge: !!row.getResultByName("needsMerge"),
    }));
    deepEqual(
      itemInfos,
      [
        {
          guid: "folderAAAAAA",
          needsMerge: false,
        },
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          needsMerge: false,
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          needsMerge: true,
        },
        {
          guid: PlacesUtils.bookmarks.rootGuid,
          needsMerge: false,
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          needsMerge: true,
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          needsMerge: true,
        },
      ],
      "Matching sync ID should not reset items"
    );
  }

  info("Set different sync ID");
  await buf.ensureCurrentSyncId("syncIdBBBBBB");
  {
    equal(
      await buf.getSyncId(),
      "syncIdBBBBBB",
      "Should replace existing sync ID"
    );
    strictEqual(
      await buf.getCollectionHighWaterMark(),
      0,
      "Different sync ID should reset high water mark"
    );

    let itemRows = await buf.db.execute(`
      SELECT guid, needsMerge FROM items
      ORDER BY guid`);
    let itemInfos = itemRows.map(row => ({
      guid: row.getResultByName("guid"),
      needsMerge: !!row.getResultByName("needsMerge"),
    }));
    deepEqual(
      itemInfos,
      [
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          needsMerge: true,
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          needsMerge: true,
        },
        {
          guid: PlacesUtils.bookmarks.rootGuid,
          needsMerge: false,
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          needsMerge: true,
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          needsMerge: true,
        },
      ],
      "Different sync ID should reset items"
    );
  }
});
