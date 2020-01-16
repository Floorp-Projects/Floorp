/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function promiseAllURLFrecencies() {
  let frecencies = new Map();
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT url, frecency
    FROM moz_places
    WHERE url_hash BETWEEN hash('http', 'prefix_lo') AND
                           hash('http', 'prefix_hi')`);
  for (let row of rows) {
    let url = row.getResultByName("url");
    let frecency = row.getResultByName("frecency");
    frecencies.set(url, frecency);
  }
  return frecencies;
}

function mapFilterIterator(iter, fn) {
  let results = [];
  for (let value of iter) {
    let newValue = fn(value);
    if (newValue) {
      results.push(newValue);
    }
  }
  return results;
}

add_task(async function test_update_frecencies() {
  let buf = await openMirror("update_frecencies");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        // Not modified in mirror; shouldn't recalculate frecency.
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
      },
      {
        // URL changed to B1 in mirror; should recalculate frecency for B
        // and B1, using existing frecency to determine order.
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
      },
      {
        // URL changed to new URL in mirror, should recalculate frecency
        // for new URL first, before B1.
        guid: "bookmarkBBB1",
        title: "B1",
        url: "http://example.com/b1",
      },
    ],
  });
  await storeRecords(
    buf,
    [
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "bookmarkBBBB", "bookmarkBBB1"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
      },
      {
        id: "bookmarkBBB1",
        parentid: "menu",
        type: "bookmark",
        title: "B1",
        bmkUri: "http://example.com/b1",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        // Query; shouldn't recalculate frecency.
        guid: "queryCCCCCCC",
        title: "C",
        url: "place:type=6&sort=14&maxResults=10",
      },
    ],
  });

  info("Calculate frecencies for all local URLs");
  await PlacesUtils.withConnectionWrapper(
    "Update all frecencies",
    async function(db) {
      await db.execute(`UPDATE moz_places SET
        frecency = CALCULATE_FRECENCY(id)`);
    }
  );

  info("Make remote changes");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["bookmarkAAAA", "bookmarkBBBB", "bookmarkBBB1"],
    },
    {
      id: "unfiled",
      parentid: "places",
      type: "folder",
      children: [
        "bookmarkBBB2",
        "bookmarkDDDD",
        "bookmarkEEEE",
        "queryFFFFFFF",
      ],
    },
    {
      // Existing bookmark changed to existing URL.
      id: "bookmarkBBBB",
      parentid: "menu",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b1",
    },
    {
      // Existing bookmark with new URL; should recalculate frecency first.
      id: "bookmarkBBB1",
      parentid: "menu",
      type: "bookmark",
      title: "B1",
      bmkUri: "http://example.com/b11",
    },
    {
      id: "bookmarkBBB2",
      parentid: "unfiled",
      type: "bookmark",
      title: "B2",
      bmkUri: "http://example.com/b",
    },
    {
      // New bookmark with new URL; should recalculate frecency first.
      id: "bookmarkDDDD",
      parentid: "unfiled",
      type: "bookmark",
      title: "D",
      bmkUri: "http://example.com/d",
    },
    {
      // New bookmark with new URL.
      id: "bookmarkEEEE",
      parentid: "unfiled",
      type: "bookmark",
      title: "E",
      bmkUri: "http://example.com/e",
    },
    {
      // New query; shouldn't count against limit.
      id: "queryFFFFFFF",
      parentid: "unfiled",
      type: "query",
      title: "F",
      bmkUri: `place:parent=${PlacesUtils.bookmarks.menuGuid}`,
    },
  ]);

  info("Apply new items and recalculate 3 frecencies");
  await buf.apply({
    maxFrecenciesToRecalculate: 3,
  });

  {
    let frecencies = await promiseAllURLFrecencies();
    let urlsWithFrecency = mapFilterIterator(
      frecencies.entries(),
      ([href, frecency]) => (frecency > 0 ? href : null)
    );

    // A is unchanged, and we should recalculate frecency for three more
    // random URLs.
    equal(
      urlsWithFrecency.length,
      4,
      "Should keep unchanged frecency and recalculate 3"
    );
    let unexpectedURLs = CommonUtils.difference(
      urlsWithFrecency,
      new Set([
        // A is unchanged.
        "http://example.com/a",

        // B11, D, and E are new URLs.
        "http://example.com/b11",
        "http://example.com/d",
        "http://example.com/e",

        // B and B1 are existing, changed URLs.
        "http://example.com/b",
        "http://example.com/b1",
      ])
    );
    ok(
      !unexpectedURLs.size,
      "Should recalculate frecency for new and changed URLs only"
    );
  }

  info("Change non-URL property of D");
  await storeRecords(buf, [
    {
      id: "bookmarkDDDD",
      parentid: "unfiled",
      type: "bookmark",
      title: "D (remote)",
      bmkUri: "http://example.com/d",
    },
  ]);

  info("Apply new item and recalculate remaining frecencies");
  await buf.apply();

  {
    let frecencies = await promiseAllURLFrecencies();
    let urlsWithoutFrecency = mapFilterIterator(
      frecencies.entries(),
      ([href, frecency]) => (frecency <= 0 ? href : null)
    );
    deepEqual(
      urlsWithoutFrecency,
      [],
      "Should finish calculating remaining frecencies"
    );
  }

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

async function setupLocalTree(localTimeSeconds) {
  let dateAdded = new Date(localTimeSeconds * 1000);
  let lastModified = new Date(localTimeSeconds * 1000);
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        dateAdded,
        lastModified,
        children: [
          {
            guid: "bookmarkBBBB",
            title: "B",
            url: "http://example.com/b",
            dateAdded,
            lastModified,
          },
          {
            guid: "bookmarkCCCC",
            title: "C",
            url: "http://example.com/c",
            dateAdded,
            lastModified,
          },
        ],
      },
      {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
        dateAdded,
        lastModified,
      },
    ],
  });
}

// This test ensures we clean up the temp tables between merges, and don't throw
// constraint errors recording observer notifications.
add_task(async function test_apply_then_revert() {
  let buf = await openMirror("apply_then_revert");

  let now = Date.now() / 1000;
  let localTimeSeconds = now - 180;

  info("Set up initial local tree and mirror");
  await setupLocalTree(localTimeSeconds);
  let recordsToUpload = await buf.apply({
    localTimeSeconds,
    remoteTimeSeconds: now,
  });
  await storeChangesInMirror(buf, recordsToUpload);
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkEEE1",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "E",
    url: "http://example.com/e",
    dateAdded: new Date(localTimeSeconds * 1000),
    lastModified: new Date(localTimeSeconds * 1000),
  });

  info("Make remote changes");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["bookmarkEEEE", "bookmarkFFFF"],
      modified: now,
    },
    {
      id: "toolbar",
      parentid: "places",
      type: "folder",
      children: ["folderAAAAAA"],
      modified: now,
    },
    {
      id: "folderAAAAAA",
      parentid: "toolbar",
      type: "folder",
      title: "A (remote)",
      children: ["bookmarkCCCC", "bookmarkBBBB"],
      modified: now,
    },
    {
      id: "bookmarkBBBB",
      parentid: "folderAAAAAA",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b-remote",
      modified: now,
    },
    {
      id: "bookmarkDDDD",
      deleted: true,
      modified: now,
    },
    {
      id: "bookmarkEEEE",
      parentid: "menu",
      type: "bookmark",
      title: "E",
      bmkUri: "http://example.com/e",
      modified: now,
    },
    {
      id: "bookmarkFFFF",
      parentid: "menu",
      type: "bookmark",
      title: "F",
      bmkUri: "http://example.com/f",
      modified: now,
    },
  ]);

  info("Apply remote changes, first time");
  let firstTimeRecords = await buf.apply({
    localTimeSeconds,
    remoteTimeSeconds: now,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.menuGuid],
    "Should leave menu with new remote structure unmerged after first time"
  );

  info("Revert local tree");
  await PlacesSyncUtils.bookmarks.wipe();
  await setupLocalTree(localTimeSeconds);
  await PlacesTestUtils.markBookmarksAsSynced();
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkEEE1",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "E",
    url: "http://example.com/e",
    dateAdded: new Date(localTimeSeconds * 1000),
    lastModified: new Date(localTimeSeconds * 1000),
  });
  let localIdForD = await PlacesUtils.promiseItemId("bookmarkDDDD");

  info("Apply remote changes, second time");
  await buf.db.execute(
    `
    UPDATE items SET
      needsMerge = 1
    WHERE guid <> :rootGuid`,
    { rootGuid: PlacesUtils.bookmarks.rootGuid }
  );
  let observer = expectBookmarkChangeNotifications();
  let secondTimeRecords = await buf.apply({
    localTimeSeconds,
    remoteTimeSeconds: now,
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.menuGuid],
    "Should leave menu with new remote structure unmerged after second time"
  );
  deepEqual(
    secondTimeRecords,
    firstTimeRecords,
    "Should stage identical records to upload, first and second time"
  );

  let localItemIds = await PlacesUtils.promiseManyItemIds([
    "bookmarkFFFF",
    "bookmarkEEEE",
    "folderAAAAAA",
    "bookmarkCCCC",
    "bookmarkBBBB",
  ]);

  observer.check([
    {
      name: "onItemChanged",
      params: {
        itemId: localItemIds.get("bookmarkEEEE"),
        property: "guid",
        isAnnoProperty: false,
        newValue: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        parentId: PlacesUtils.bookmarksMenuFolderId,
        guid: "bookmarkEEEE",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        oldValue: "bookmarkEEE1",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "bookmark-removed",
      params: {
        itemId: localIdForD,
        parentId: PlacesUtils.bookmarksMenuFolderId,
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/d",
        guid: "bookmarkDDDD",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("bookmarkFFFF"),
        parentId: PlacesUtils.bookmarksMenuFolderId,
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/f",
        title: "F",
        guid: "bookmarkFFFF",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkEEEE"),
        oldParentId: PlacesUtils.bookmarksMenuFolderId,
        oldIndex: 2,
        newParentId: PlacesUtils.bookmarksMenuFolderId,
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkEEEE",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/e",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("folderAAAAAA"),
        oldParentId: PlacesUtils.bookmarksMenuFolderId,
        oldIndex: 0,
        newParentId: PlacesUtils.toolbarFolderId,
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        guid: "folderAAAAAA",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.toolbarGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: null,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        oldParentId: localItemIds.get("folderAAAAAA"),
        oldIndex: 1,
        newParentId: localItemIds.get("folderAAAAAA"),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkCCCC",
        oldParentGuid: "folderAAAAAA",
        newParentGuid: "folderAAAAAA",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/c",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        oldParentId: localItemIds.get("folderAAAAAA"),
        oldIndex: 0,
        newParentId: localItemIds.get("folderAAAAAA"),
        newIndex: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkBBBB",
        oldParentGuid: "folderAAAAAA",
        newParentGuid: "folderAAAAAA",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/b-remote",
      },
    },
    {
      name: "onItemChanged",
      params: {
        itemId: localItemIds.get("folderAAAAAA"),
        property: "title",
        isAnnoProperty: false,
        newValue: "A (remote)",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentId: PlacesUtils.toolbarFolderId,
        guid: "folderAAAAAA",
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        oldValue: "A",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "onItemChanged",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        property: "uri",
        isAnnoProperty: false,
        newValue: "http://example.com/b-remote",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        parentId: localItemIds.get("folderAAAAAA"),
        guid: "bookmarkBBBB",
        parentGuid: "folderAAAAAA",
        oldValue: "http://example.com/b",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
  ]);

  await assertLocalTree(
    PlacesUtils.bookmarks.rootGuid,
    {
      guid: PlacesUtils.bookmarks.rootGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "",
      children: [
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: BookmarksMenuTitle,
          children: [
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "E",
              url: "http://example.com/e",
            },
            {
              guid: "bookmarkFFFF",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              title: "F",
              url: "http://example.com/f",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: BookmarksToolbarTitle,
          children: [
            {
              guid: "folderAAAAAA",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "A (remote)",
              children: [
                {
                  guid: "bookmarkCCCC",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "C",
                  url: "http://example.com/c",
                },
                {
                  guid: "bookmarkBBBB",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 1,
                  title: "B",
                  url: "http://example.com/b-remote",
                },
              ],
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 3,
          title: UnfiledBookmarksTitle,
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
        },
      ],
    },
    "Should apply new structure, second time"
  );

  await storeChangesInMirror(buf, secondTimeRecords);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
