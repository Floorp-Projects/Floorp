/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_value_structure_conflict() {
  let buf = await openMirror("value_structure_conflict");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "bookmarkBBBB",
            url: "http://example.com/b",
            title: "B",
          },
          {
            guid: "bookmarkCCCC",
            url: "http://example.com/c",
            title: "C",
          },
        ],
      },
      {
        guid: "folderDDDDDD",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "D",
        children: [
          {
            guid: "bookmarkEEEE",
            url: "http://example.com/e",
            title: "E",
          },
        ],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA", "folderDDDDDD"],
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        children: ["bookmarkBBBB", "bookmarkCCCC"],
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "bookmarkBBBB",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        children: ["bookmarkEEEE"],
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
        modified: Date.now() / 1000 - 60,
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local value change");
  await PlacesUtils.bookmarks.update({
    guid: "folderAAAAAA",
    title: "A (local)",
  });

  info("Make local structure change");
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkBBBB",
    parentGuid: "folderDDDDDD",
    index: 0,
  });

  info("Make remote value change");
  await storeRecords(buf, [
    {
      id: "folderDDDDDD",
      parentid: "menu",
      type: "folder",
      title: "D (remote)",
      children: ["bookmarkEEEE"],
      modified: Date.now() / 1000 + 60,
    },
  ]);

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: Date.now() / 1000,
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["folderDDDDDD"],
    "Should leave D with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkBBBB", "folderAAAAAA", "folderDDDDDD"],
      deleted: [],
    },
    "Should upload records for merged and new local items"
  );

  let localItemIds = await PlacesUtils.promiseManyItemIds([
    "folderAAAAAA",
    "bookmarkEEEE",
    "bookmarkBBBB",
    "folderDDDDDD",
  ]);
  observer.check([
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkEEEE"),
        oldParentId: localItemIds.get("folderDDDDDD"),
        oldIndex: 1,
        newParentId: localItemIds.get("folderDDDDDD"),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkEEEE",
        oldParentGuid: "folderDDDDDD",
        newParentGuid: "folderDDDDDD",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/e",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        oldParentId: localItemIds.get("folderDDDDDD"),
        oldIndex: 0,
        newParentId: localItemIds.get("folderDDDDDD"),
        newIndex: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkBBBB",
        oldParentGuid: "folderDDDDDD",
        newParentGuid: "folderDDDDDD",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/b",
      },
    },
    {
      name: "onItemChanged",
      params: {
        itemId: localItemIds.get("folderDDDDDD"),
        property: "title",
        isAnnoProperty: false,
        newValue: "D (remote)",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentId: PlacesUtils.bookmarksMenuFolderId,
        guid: "folderDDDDDD",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        oldValue: "D",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
  ]);

  await assertLocalTree(
    PlacesUtils.bookmarks.menuGuid,
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [
        {
          guid: "folderAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "A (local)",
          children: [
            {
              guid: "bookmarkCCCC",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "C",
              url: "http://example.com/c",
            },
          ],
        },
        {
          guid: "folderDDDDDD",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: "D (remote)",
          children: [
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "E",
              url: "http://example.com/e",
            },
            {
              guid: "bookmarkBBBB",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              title: "B",
              url: "http://example.com/b",
            },
          ],
        },
      ],
    },
    "Should reconcile structure and value changes"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move() {
  let buf = await openMirror("move");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "devFolder___",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "Dev",
        children: [
          {
            guid: "mdnBmk______",
            title: "MDN",
            url: "https://developer.mozilla.org",
          },
          {
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            guid: "mozFolder___",
            title: "Mozilla",
            children: [
              {
                guid: "fxBmk_______",
                title: "Get Firefox!",
                url: "http://getfirefox.com/",
              },
              {
                guid: "nightlyBmk__",
                title: "Nightly",
                url: "https://nightly.mozilla.org",
              },
            ],
          },
          {
            guid: "wmBmk_______",
            title: "Webmaker",
            url: "https://webmaker.org",
          },
        ],
      },
      {
        guid: "bzBmk_______",
        title: "Bugzilla",
        url: "https://bugzilla.mozilla.org",
      },
    ],
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  await storeRecords(
    buf,
    shuffle([
      {
        id: "unfiled",
        parentid: "places",
        type: "folder",
        children: ["mozFolder___"],
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["devFolder___"],
      },
      {
        // Moving to toolbar.
        id: "devFolder___",
        parentid: "toolbar",
        type: "folder",
        title: "Dev",
        children: ["bzBmk_______", "wmBmk_______"],
      },
      {
        // Moving to "Mozilla".
        id: "mdnBmk______",
        parentid: "mozFolder___",
        type: "bookmark",
        title: "MDN",
        bmkUri: "https://developer.mozilla.org",
      },
      {
        // Rearranging children and moving to unfiled.
        id: "mozFolder___",
        parentid: "unfiled",
        type: "folder",
        title: "Mozilla",
        children: ["nightlyBmk__", "mdnBmk______", "fxBmk_______"],
      },
      {
        id: "fxBmk_______",
        parentid: "mozFolder___",
        type: "bookmark",
        title: "Get Firefox!",
        bmkUri: "http://getfirefox.com/",
      },
      {
        id: "nightlyBmk__",
        parentid: "mozFolder___",
        type: "bookmark",
        title: "Nightly",
        bmkUri: "https://nightly.mozilla.org",
      },
      {
        id: "wmBmk_______",
        parentid: "devFolder___",
        type: "bookmark",
        title: "Webmaker",
        bmkUri: "https://webmaker.org",
      },
      {
        id: "bzBmk_______",
        parentid: "devFolder___",
        type: "bookmark",
        title: "Bugzilla",
        bmkUri: "https://bugzilla.mozilla.org",
      },
    ])
  );

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: [],
      deleted: [],
    },
    "Should not upload records for remotely moved items"
  );

  let localItemIds = await PlacesUtils.promiseManyItemIds([
    "devFolder___",
    "mozFolder___",
    "bzBmk_______",
    "wmBmk_______",
    "nightlyBmk__",
    "mdnBmk______",
    "fxBmk_______",
  ]);
  observer.check([
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("devFolder___"),
        oldParentId: PlacesUtils.bookmarksMenuFolderId,
        oldIndex: 0,
        newParentId: PlacesUtils.toolbarFolderId,
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        guid: "devFolder___",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: PlacesUtils.bookmarks.toolbarGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: null,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("mozFolder___"),
        oldParentId: localItemIds.get("devFolder___"),
        oldIndex: 1,
        newParentId: await PlacesUtils.promiseItemId(
          PlacesUtils.bookmarks.unfiledGuid
        ),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        guid: "mozFolder___",
        oldParentGuid: "devFolder___",
        newParentGuid: PlacesUtils.bookmarks.unfiledGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: null,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bzBmk_______"),
        oldParentId: PlacesUtils.bookmarksMenuFolderId,
        oldIndex: 1,
        newParentId: localItemIds.get("devFolder___"),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bzBmk_______",
        oldParentGuid: PlacesUtils.bookmarks.menuGuid,
        newParentGuid: "devFolder___",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "https://bugzilla.mozilla.org/",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("wmBmk_______"),
        oldParentId: localItemIds.get("devFolder___"),
        oldIndex: 2,
        newParentId: localItemIds.get("devFolder___"),
        newIndex: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "wmBmk_______",
        oldParentGuid: "devFolder___",
        newParentGuid: "devFolder___",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "https://webmaker.org/",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("nightlyBmk__"),
        oldParentId: localItemIds.get("mozFolder___"),
        oldIndex: 1,
        newParentId: localItemIds.get("mozFolder___"),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "nightlyBmk__",
        oldParentGuid: "mozFolder___",
        newParentGuid: "mozFolder___",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "https://nightly.mozilla.org/",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("mdnBmk______"),
        oldParentId: localItemIds.get("devFolder___"),
        oldIndex: 0,
        newParentId: localItemIds.get("mozFolder___"),
        newIndex: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "mdnBmk______",
        oldParentGuid: "devFolder___",
        newParentGuid: "mozFolder___",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "https://developer.mozilla.org/",
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("fxBmk_______"),
        oldParentId: localItemIds.get("mozFolder___"),
        oldIndex: 0,
        newParentId: localItemIds.get("mozFolder___"),
        newIndex: 2,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "fxBmk_______",
        oldParentGuid: "mozFolder___",
        newParentGuid: "mozFolder___",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://getfirefox.com/",
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
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: BookmarksToolbarTitle,
          children: [
            {
              guid: "devFolder___",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "Dev",
              children: [
                {
                  guid: "bzBmk_______",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "Bugzilla",
                  url: "https://bugzilla.mozilla.org/",
                },
                {
                  guid: "wmBmk_______",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 1,
                  title: "Webmaker",
                  url: "https://webmaker.org/",
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
          children: [
            {
              guid: "mozFolder___",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "Mozilla",
              children: [
                {
                  guid: "nightlyBmk__",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "Nightly",
                  url: "https://nightly.mozilla.org/",
                },
                {
                  guid: "mdnBmk______",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 1,
                  title: "MDN",
                  url: "https://developer.mozilla.org/",
                },
                {
                  guid: "fxBmk_______",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 2,
                  title: "Get Firefox!",
                  url: "http://getfirefox.com/",
                },
              ],
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
        },
      ],
    },
    "Should move and reorder bookmarks to match remote"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move_into_parent_sibling() {
  // This test moves a bookmark that exists locally into a new folder that only
  // exists remotely, and is a later sibling of the local parent. This ensures
  // we set up the local structure before applying structure changes.
  let buf = await openMirror("move_into_parent_sibling");

  info("Set up mirror: Menu > A > B");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "bookmarkBBBB",
            url: "http://example.com/b",
            title: "B",
          },
        ],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA"],
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        children: ["bookmarkBBBB"],
      },
      {
        id: "bookmarkBBBB",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes: Menu > (A (B > C))");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["folderAAAAAA", "folderCCCCCC"],
    },
    {
      id: "folderAAAAAA",
      parentid: "menu",
      type: "folder",
      title: "A",
    },
    {
      id: "folderCCCCCC",
      parentid: "menu",
      type: "folder",
      title: "C",
      children: ["bookmarkBBBB"],
    },
    {
      id: "bookmarkBBBB",
      parentid: "folderCCCCCC",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b",
    },
  ]);

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: [],
      deleted: [],
    },
    "Should not upload records for remote-only structure changes"
  );

  let localItemIds = await PlacesUtils.promiseManyItemIds([
    "folderCCCCCC",
    "bookmarkBBBB",
    "folderAAAAAA",
  ]);
  observer.check([
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("folderCCCCCC"),
        parentId: PlacesUtils.bookmarksMenuFolderId,
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        urlHref: "",
        title: "C",
        guid: "folderCCCCCC",
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkBBBB"),
        oldParentId: localItemIds.get("folderAAAAAA"),
        oldIndex: 0,
        newParentId: localItemIds.get("folderCCCCCC"),
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkBBBB",
        oldParentGuid: "folderAAAAAA",
        newParentGuid: "folderCCCCCC",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/b",
      },
    },
  ]);

  await assertLocalTree(
    PlacesUtils.bookmarks.menuGuid,
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [
        {
          guid: "folderAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "A",
        },
        {
          guid: "folderCCCCCC",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: "C",
          children: [
            {
              guid: "bookmarkBBBB",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "B",
              url: "http://example.com/b",
            },
          ],
        },
      ],
    },
    "Should set up local structure correctly"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_complex_move_with_additions() {
  let mergeTelemetryCounts;
  let buf = await openMirror("complex_move_with_additions", {
    recordStepTelemetry(name, took, counts) {
      if (name == "merge") {
        mergeTelemetryCounts = counts;
      }
    },
  });

  info("Set up mirror: Menu > A > (B C)");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "bookmarkBBBB",
            url: "http://example.com/b",
            title: "B",
          },
          {
            guid: "bookmarkCCCC",
            url: "http://example.com/c",
            title: "C",
          },
        ],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA"],
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        children: ["bookmarkBBBB", "bookmarkCCCC"],
      },
      {
        id: "bookmarkBBBB",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local change: Menu > A > (B C D)");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkDDDD",
    parentGuid: "folderAAAAAA",
    title: "D (local)",
    url: "http://example.com/d-local",
  });

  info("Make remote change: ((Menu > C) (Toolbar > A > (B E)))");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkCCCC"],
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA"],
      },
      {
        id: "folderAAAAAA",
        parentid: "toolbar",
        type: "folder",
        title: "A",
        children: ["bookmarkBBBB", "bookmarkEEEE"],
      },
      {
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderAAAAAA",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
    ])
  );

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    notifyInStableOrder: true,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["folderAAAAAA"],
    "Should leave A with new remote structure unmerged"
  );
  deepEqual(
    mergeTelemetryCounts,
    [{ name: "items", count: 10 }],
    "Should record telemetry with structure change counts"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkDDDD", "folderAAAAAA"],
      deleted: [],
    },
    "Should upload new records for (A D)"
  );

  let localItemIds = await PlacesUtils.promiseManyItemIds([
    "bookmarkEEEE",
    "folderAAAAAA",
    "bookmarkCCCC",
  ]);
  observer.check([
    {
      name: "bookmark-added",
      params: {
        itemId: localItemIds.get("bookmarkEEEE"),
        parentId: localItemIds.get("folderAAAAAA"),
        index: 1,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        urlHref: "http://example.com/e",
        title: "E",
        guid: "bookmarkEEEE",
        parentGuid: "folderAAAAAA",
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
      },
    },
    {
      name: "onItemMoved",
      params: {
        itemId: localItemIds.get("bookmarkCCCC"),
        oldParentId: localItemIds.get("folderAAAAAA"),
        oldIndex: 1,
        newParentId: PlacesUtils.bookmarksMenuFolderId,
        newIndex: 0,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        guid: "bookmarkCCCC",
        oldParentGuid: "folderAAAAAA",
        newParentGuid: PlacesUtils.bookmarks.menuGuid,
        source: PlacesUtils.bookmarks.SOURCES.SYNC,
        urlHref: "http://example.com/c",
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
              guid: "bookmarkCCCC",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "C",
              url: "http://example.com/c",
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
              // We can guarantee child order (B E D), since we always walk remote
              // children first, and the remote folder A record is newer than the
              // local folder. If the local folder were newer, the order would be
              // (D B E).
              guid: "folderAAAAAA",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "A",
              children: [
                {
                  guid: "bookmarkBBBB",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "B",
                  url: "http://example.com/b",
                },
                {
                  guid: "bookmarkEEEE",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 1,
                  title: "E",
                  url: "http://example.com/e",
                },
                {
                  guid: "bookmarkDDDD",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 2,
                  title: "D (local)",
                  url: "http://example.com/d-local",
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
    "Should take remote order and preserve local children"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_reorder_and_insert() {
  let buf = await openMirror("reorder_and_insert");

  info("Set up mirror");
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
      {
        guid: "bookmarkCCCC",
        url: "http://example.com/c",
        title: "C",
      },
    ],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bookmarkDDDD",
        url: "http://example.com/d",
        title: "D",
      },
      {
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
      },
      {
        guid: "bookmarkFFFF",
        url: "http://example.com/f",
        title: "F",
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "bookmarkBBBB", "bookmarkCCCC"],
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
        id: "bookmarkCCCC",
        parentid: "menu",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["bookmarkDDDD", "bookmarkEEEE", "bookmarkFFFF"],
      },
      {
        id: "bookmarkDDDD",
        parentid: "toolbar",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
      },
      {
        id: "bookmarkEEEE",
        parentid: "toolbar",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
      {
        id: "bookmarkFFFF",
        parentid: "toolbar",
        type: "bookmark",
        title: "F",
        bmkUri: "http://example.com/f",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  let now = Date.now();

  info("Make local changes: Reorder Menu, Toolbar > (G H)");
  await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.menuGuid, [
    "bookmarkCCCC",
    "bookmarkAAAA",
    "bookmarkBBBB",
  ]);
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bookmarkGGGG",
        url: "http://example.com/g",
        title: "G",
        dateAdded: new Date(now),
        lastModified: new Date(now),
      },
      {
        guid: "bookmarkHHHH",
        url: "http://example.com/h",
        title: "H",
        dateAdded: new Date(now),
        lastModified: new Date(now),
      },
    ],
  });

  info("Make remote changes: Reorder Toolbar, Menu > (I J)");
  await storeRecords(
    buf,
    shuffle([
      {
        // The server has a newer toolbar, so we should use the remote order (F D E)
        // as the base, then append (G H).
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["bookmarkFFFF", "bookmarkDDDD", "bookmarkEEEE"],
        modified: now / 1000 + 5,
      },
      {
        // The server has an older menu, so we should use the local order (C A B)
        // as the base, then append (I J).
        id: "menu",
        parentid: "places",
        type: "folder",
        children: [
          "bookmarkAAAA",
          "bookmarkBBBB",
          "bookmarkCCCC",
          "bookmarkIIII",
          "bookmarkJJJJ",
        ],
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkIIII",
        parentid: "menu",
        type: "bookmark",
        title: "I",
        bmkUri: "http://example.com/i",
      },
      {
        id: "bookmarkJJJJ",
        parentid: "menu",
        type: "bookmark",
        title: "J",
        bmkUri: "http://example.com/j",
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: now / 1000,
    localTimeSeconds: now / 1000,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.menuGuid, PlacesUtils.bookmarks.toolbarGuid],
    "Should leave roots with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkGGGG", "bookmarkHHHH", "menu", "toolbar"],
      deleted: [],
    },
    "Should upload records for merged and new local items"
  );

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
              guid: "bookmarkCCCC",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              url: "http://example.com/c",
              title: "C",
            },
            {
              guid: "bookmarkAAAA",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              url: "http://example.com/a",
              title: "A",
            },
            {
              guid: "bookmarkBBBB",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 2,
              url: "http://example.com/b",
              title: "B",
            },
            {
              guid: "bookmarkIIII",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 3,
              url: "http://example.com/i",
              title: "I",
            },
            {
              guid: "bookmarkJJJJ",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 4,
              url: "http://example.com/j",
              title: "J",
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
              guid: "bookmarkFFFF",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              url: "http://example.com/f",
              title: "F",
            },
            {
              guid: "bookmarkDDDD",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              url: "http://example.com/d",
              title: "D",
            },
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 2,
              url: "http://example.com/e",
              title: "E",
            },
            {
              guid: "bookmarkGGGG",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 3,
              url: "http://example.com/g",
              title: "G",
            },
            {
              guid: "bookmarkHHHH",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 4,
              url: "http://example.com/h",
              title: "H",
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
    "Should use timestamps to decide base folder order"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_newer_remote_moves() {
  let now = Date.now();
  let buf = await openMirror("newer_remote_moves");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        url: "http://example.com/a",
        title: "A",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
      {
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
        children: [
          {
            guid: "bookmarkCCCC",
            url: "http://example.com/c",
            title: "C",
            dateAdded: new Date(now - 5000),
            lastModified: new Date(now - 5000),
          },
        ],
      },
      {
        guid: "folderDDDDDD",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "D",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
    ],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
      {
        guid: "folderFFFFFF",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "F",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
        children: [
          {
            guid: "bookmarkGGGG",
            url: "http://example.com/g",
            title: "G",
            dateAdded: new Date(now - 5000),
            lastModified: new Date(now - 5000),
          },
        ],
      },
      {
        guid: "folderHHHHHH",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "H",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "folderBBBBBB", "folderDDDDDD"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderBBBBBB",
        parentid: "menu",
        type: "folder",
        title: "B",
        children: ["bookmarkCCCC"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["bookmarkEEEE", "folderFFFFFF", "folderHHHHHH"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkEEEE",
        parentid: "toolbar",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderFFFFFF",
        parentid: "toolbar",
        type: "folder",
        title: "F",
        children: ["bookmarkGGGG"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkGGGG",
        parentid: "folderFFFFFF",
        type: "bookmark",
        title: "G",
        bmkUri: "http://example.com/g",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderHHHHHH",
        parentid: "toolbar",
        type: "folder",
        title: "H",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info(
    "Make local changes: Unfiled > A, Mobile > B; Toolbar > (H F E); D > C; H > G"
  );
  let localMoves = [
    {
      guid: "bookmarkAAAA",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      guid: "folderBBBBBB",
      parentGuid: PlacesUtils.bookmarks.mobileGuid,
    },
    {
      guid: "bookmarkCCCC",
      parentGuid: "folderDDDDDD",
    },
    {
      guid: "bookmarkGGGG",
      parentGuid: "folderHHHHHH",
    },
  ];
  for (let { guid, parentGuid } of localMoves) {
    await PlacesUtils.bookmarks.update({
      guid,
      parentGuid,
      index: 0,
      lastModified: new Date(now - 2500),
    });
  }
  await PlacesUtils.bookmarks.reorder(
    PlacesUtils.bookmarks.toolbarGuid,
    ["folderHHHHHH", "folderFFFFFF", "bookmarkEEEE"],
    { lastModified: new Date(now - 2500) }
  );

  info(
    "Make remote changes: Mobile > A, Unfiled > B; Toolbar > (F E H); D > G; H > C"
  );
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderDDDDDD"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "mobile",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        // This is similar to H > C, explained below, except we'll always reupload
        // the mobile root, because we always prefer the local state for roots.
        id: "bookmarkAAAA",
        parentid: "mobile",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "unfiled",
        parentid: "places",
        type: "folder",
        children: ["folderBBBBBB"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "folderBBBBBB",
        parentid: "unfiled",
        type: "folder",
        title: "B",
        children: [],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["folderFFFFFF", "bookmarkEEEE", "folderHHHHHH"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "folderHHHHHH",
        parentid: "toolbar",
        type: "folder",
        title: "H",
        children: ["bookmarkCCCC"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        // Reparenting an item uploads records for the item and its parent.
        // The merger would still work if we only marked H as unmerged; we'd
        // then use the remote state for H, and local state for C. Since C was
        // changed locally, we'll reupload it, even though it didn't actually
        // change.
        id: "bookmarkCCCC",
        parentid: "folderHHHHHH",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        children: ["bookmarkGGGG"],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        dateAdded: now - 5000,
        modified: now / 1000,
        children: ["bookmarkGGGG"],
      },
      {
        id: "folderFFFFFF",
        parentid: "toolbar",
        type: "folder",
        title: "F",
        children: [],
        dateAdded: now - 5000,
        modified: now / 1000,
      },
      {
        // Same as C above.
        id: "bookmarkGGGG",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "G",
        bmkUri: "http://example.com/g",
        dateAdded: now - 5000,
        modified: now / 1000,
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply({
    localTimeSeconds: now / 1000,
    remoteTimeSeconds: now / 1000,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [
      PlacesUtils.bookmarks.menuGuid,
      PlacesUtils.bookmarks.mobileGuid,
      PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.unfiledGuid,
    ],
    "Should leave roots with new remote structure unmerged"
  );
  let datesAdded = await promiseManyDatesAdded([
    PlacesUtils.bookmarks.menuGuid,
    PlacesUtils.bookmarks.mobileGuid,
    PlacesUtils.bookmarks.unfiledGuid,
    PlacesUtils.bookmarks.toolbarGuid,
  ]);
  deepEqual(
    changesToUpload,
    {
      // We took the remote structure for the roots, but they're still flagged as
      // changed locally. Since we always use the local state for roots
      // (bug 1472241), and can't distinguish between value and structure changes
      // in Places (see the comment for F below), we'll reupload them.
      menu: {
        tombstone: false,
        counter: 2,
        synced: false,
        cleartext: {
          id: "menu",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.menuGuid),
          children: ["folderDDDDDD"],
          title: BookmarksMenuTitle,
        },
      },
      mobile: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "mobile",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.mobileGuid),
          children: ["bookmarkAAAA"],
          title: MobileBookmarksTitle,
        },
      },
      unfiled: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "unfiled",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.unfiledGuid),
          children: ["folderBBBBBB"],
          title: UnfiledBookmarksTitle,
        },
      },
      toolbar: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "toolbar",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.toolbarGuid),
          children: ["folderFFFFFF", "bookmarkEEEE", "folderHHHHHH"],
          title: BookmarksToolbarTitle,
        },
      },
    },
    "Should only reupload local roots"
  );

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
              guid: "folderDDDDDD",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "D",
              children: [
                {
                  guid: "bookmarkGGGG",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "G",
                  url: "http://example.com/g",
                },
              ],
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
              guid: "folderFFFFFF",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "F",
            },
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              title: "E",
              url: "http://example.com/e",
            },
            {
              guid: "folderHHHHHH",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 2,
              title: "H",
              children: [
                {
                  guid: "bookmarkCCCC",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "C",
                  url: "http://example.com/c",
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
          children: [
            {
              guid: "folderBBBBBB",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "B",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
          children: [
            {
              guid: "bookmarkAAAA",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "A",
              url: "http://example.com/a",
            },
          ],
        },
      ],
    },
    "Should use newer remote parents and order"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_newer_local_moves() {
  let now = Date.now();
  let buf = await openMirror("newer_local_moves");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        url: "http://example.com/a",
        title: "A",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
      {
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
        children: [
          {
            guid: "bookmarkCCCC",
            url: "http://example.com/c",
            title: "C",
            dateAdded: new Date(now - 5000),
            lastModified: new Date(now - 5000),
          },
        ],
      },
      {
        guid: "folderDDDDDD",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "D",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
    ],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
      {
        guid: "folderFFFFFF",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "F",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
        children: [
          {
            guid: "bookmarkGGGG",
            url: "http://example.com/g",
            title: "G",
            dateAdded: new Date(now - 5000),
            lastModified: new Date(now - 5000),
          },
        ],
      },
      {
        guid: "folderHHHHHH",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "H",
        dateAdded: new Date(now - 5000),
        lastModified: new Date(now - 5000),
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA", "folderBBBBBB", "folderDDDDDD"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderBBBBBB",
        parentid: "menu",
        type: "folder",
        title: "B",
        children: ["bookmarkCCCC"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["bookmarkEEEE", "folderFFFFFF", "folderHHHHHH"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkEEEE",
        parentid: "toolbar",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderFFFFFF",
        parentid: "toolbar",
        type: "folder",
        title: "F",
        children: ["bookmarkGGGG"],
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "bookmarkGGGG",
        parentid: "folderFFFFFF",
        type: "bookmark",
        title: "G",
        bmkUri: "http://example.com/g",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
      {
        id: "folderHHHHHH",
        parentid: "toolbar",
        type: "folder",
        title: "H",
        dateAdded: now - 5000,
        modified: now / 1000 - 5,
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info(
    "Make local changes: Unfiled > A, Mobile > B; Toolbar > (H F E); D > C; H > G"
  );
  let localMoves = [
    {
      guid: "bookmarkAAAA",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      guid: "folderBBBBBB",
      parentGuid: PlacesUtils.bookmarks.mobileGuid,
    },
    {
      guid: "bookmarkCCCC",
      parentGuid: "folderDDDDDD",
    },
    {
      guid: "bookmarkGGGG",
      parentGuid: "folderHHHHHH",
    },
  ];
  for (let { guid, parentGuid } of localMoves) {
    await PlacesUtils.bookmarks.update({
      guid,
      parentGuid,
      index: 0,
      lastModified: new Date(now),
    });
  }
  await PlacesUtils.bookmarks.reorder(
    PlacesUtils.bookmarks.toolbarGuid,
    ["folderHHHHHH", "folderFFFFFF", "bookmarkEEEE"],
    { lastModified: new Date(now) }
  );

  info(
    "Make remote changes: Mobile > A, Unfiled > B; Toolbar > (F E H); D > G; H > C"
  );
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderDDDDDD"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "mobile",
        parentid: "places",
        type: "folder",
        children: ["bookmarkAAAA"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "bookmarkAAAA",
        parentid: "mobile",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "unfiled",
        parentid: "places",
        type: "folder",
        children: ["folderBBBBBB"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "folderBBBBBB",
        parentid: "unfiled",
        type: "folder",
        title: "B",
        children: [],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["folderFFFFFF", "bookmarkEEEE", "folderHHHHHH"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "folderHHHHHH",
        parentid: "toolbar",
        type: "folder",
        title: "H",
        children: ["bookmarkCCCC"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderHHHHHH",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        children: ["bookmarkGGGG"],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "folderFFFFFF",
        parentid: "toolbar",
        type: "folder",
        title: "F",
        children: [],
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
      {
        id: "bookmarkGGGG",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "G",
        bmkUri: "http://example.com/g",
        dateAdded: now - 5000,
        modified: now / 1000 - 2.5,
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply({
    localTimeSeconds: now / 1000,
    remoteTimeSeconds: now / 1000,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [
      "bookmarkAAAA",
      "bookmarkCCCC",
      "bookmarkGGGG",
      "folderBBBBBB",
      "folderDDDDDD",
      "folderFFFFFF",
      "folderHHHHHH",
      PlacesUtils.bookmarks.menuGuid,
      PlacesUtils.bookmarks.mobileGuid,
      PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.unfiledGuid,
    ],
    "Should leave items with new remote structure unmerged"
  );
  let datesAdded = await promiseManyDatesAdded([
    PlacesUtils.bookmarks.menuGuid,
    PlacesUtils.bookmarks.mobileGuid,
    PlacesUtils.bookmarks.unfiledGuid,
    PlacesUtils.bookmarks.toolbarGuid,
  ]);
  deepEqual(
    changesToUpload,
    {
      // Reupload roots with new children.
      menu: {
        tombstone: false,
        counter: 2,
        synced: false,
        cleartext: {
          id: "menu",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.menuGuid),
          children: ["folderDDDDDD"],
          title: BookmarksMenuTitle,
        },
      },
      mobile: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "mobile",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.mobileGuid),
          children: ["folderBBBBBB"],
          title: MobileBookmarksTitle,
        },
      },
      unfiled: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "unfiled",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.unfiledGuid),
          children: ["bookmarkAAAA"],
          title: UnfiledBookmarksTitle,
        },
      },
      toolbar: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "toolbar",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: datesAdded.get(PlacesUtils.bookmarks.toolbarGuid),
          children: ["folderHHHHHH", "folderFFFFFF", "bookmarkEEEE"],
          title: BookmarksToolbarTitle,
        },
      },
      // G moved to H from F, so F and H have new children, and we need
      // to upload G for the new `parentid`.
      folderFFFFFF: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "folderFFFFFF",
          type: "folder",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: now - 5000,
          children: [],
          title: "F",
        },
      },
      folderHHHHHH: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "folderHHHHHH",
          type: "folder",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: now - 5000,
          children: ["bookmarkGGGG"],
          title: "H",
        },
      },
      bookmarkGGGG: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkGGGG",
          type: "bookmark",
          parentid: "folderHHHHHH",
          hasDupe: true,
          parentName: "H",
          dateAdded: now - 5000,
          bmkUri: "http://example.com/g",
          title: "G",
        },
      },
      // C moved to D, so we need to reupload D (for `children`) and C
      // (for `parentid`).
      folderDDDDDD: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "folderDDDDDD",
          type: "folder",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: now - 5000,
          children: ["bookmarkCCCC"],
          title: "D",
        },
      },
      bookmarkCCCC: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkCCCC",
          type: "bookmark",
          parentid: "folderDDDDDD",
          hasDupe: true,
          parentName: "D",
          dateAdded: now - 5000,
          bmkUri: "http://example.com/c",
          title: "C",
        },
      },
      // Reupload A with the new `parentid`. B moved to mobile *and* has
      // new children` so we should upload it, anyway.
      bookmarkAAAA: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkAAAA",
          type: "bookmark",
          parentid: "unfiled",
          hasDupe: true,
          parentName: UnfiledBookmarksTitle,
          dateAdded: now - 5000,
          bmkUri: "http://example.com/a",
          title: "A",
        },
      },
      folderBBBBBB: {
        tombstone: false,
        counter: 2,
        synced: false,
        cleartext: {
          id: "folderBBBBBB",
          type: "folder",
          parentid: "mobile",
          hasDupe: true,
          parentName: MobileBookmarksTitle,
          dateAdded: now - 5000,
          children: [],
          title: "B",
        },
      },
    },
    "Should reupload new local structure"
  );

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
              guid: "folderDDDDDD",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "D",
              children: [
                {
                  guid: "bookmarkCCCC",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "C",
                  url: "http://example.com/c",
                },
              ],
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
              guid: "folderHHHHHH",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "H",
              children: [
                {
                  guid: "bookmarkGGGG",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "G",
                  url: "http://example.com/g",
                },
              ],
            },
            {
              guid: "folderFFFFFF",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 1,
              title: "F",
            },
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 2,
              title: "E",
              url: "http://example.com/e",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 3,
          title: UnfiledBookmarksTitle,
          children: [
            {
              guid: "bookmarkAAAA",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "A",
              url: "http://example.com/a",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
          children: [
            {
              guid: "folderBBBBBB",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "B",
            },
          ],
        },
      ],
    },
    "Should use newer local parents and order"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_unchanged_newer_changed_older() {
  let buf = await openMirror("unchanged_newer_changed_older");
  let modified = new Date(Date.now() - 5000);

  info("Set up mirror");
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.menuGuid,
    dateAdded: new Date(modified.getTime() - 5000),
  });
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    dateAdded: new Date(modified.getTime() - 5000),
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        dateAdded: new Date(modified.getTime() - 5000),
        lastModified: modified,
      },
      {
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
        dateAdded: new Date(modified.getTime() - 5000),
        lastModified: modified,
      },
    ],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "folderCCCCCC",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "C",
        dateAdded: new Date(modified.getTime() - 5000),
        lastModified: modified,
      },
      {
        guid: "bookmarkDDDD",
        url: "http://example.com/d",
        title: "D",
        dateAdded: new Date(modified.getTime() - 5000),
        lastModified: modified,
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA", "bookmarkBBBB"],
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
      {
        id: "bookmarkBBBB",
        parentid: "menu",
        type: "bookmark",
        title: "B",
        bmkUri: "http://example.com/b",
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: ["folderCCCCCC", "bookmarkDDDD"],
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
      {
        id: "folderCCCCCC",
        parentid: "toolbar",
        type: "folder",
        title: "C",
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
      {
        id: "bookmarkDDDD",
        parentid: "toolbar",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000,
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  // Even though the local menu is newer (local = 5s, remote = 9s; adding E
  // updated the modified times of A and the menu), it's not *changed* locally,
  // so we should merge remote children first.
  info("Add A > E locally with newer time; delete A remotely with older time");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkEEEE",
    parentGuid: "folderAAAAAA",
    url: "http://example.com/e",
    title: "E",
    index: 0,
    dateAdded: new Date(modified.getTime() + 5000),
    lastModified: new Date(modified.getTime() + 5000),
  });
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["bookmarkBBBB"],
      dateAdded: modified.getTime() - 5000,
      modified: modified.getTime() / 1000 + 1,
    },
    {
      id: "folderAAAAAA",
      deleted: true,
    },
  ]);

  // Even though the remote toolbar is newer (local = 15s, remote = 10s), it's
  // not changed remotely, so we should merge local children first.
  info("Add C > F remotely with newer time; delete C locally with older time");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "folderCCCCCC",
        parentid: "toolbar",
        type: "folder",
        title: "C",
        children: ["bookmarkFFFF"],
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000 + 5,
      },
      {
        id: "bookmarkFFFF",
        parentid: "folderCCCCCC",
        type: "bookmark",
        title: "F",
        bmkUri: "http://example.com/f",
        dateAdded: modified.getTime() - 5000,
        modified: modified.getTime() / 1000 + 5,
      },
    ])
  );
  await PlacesUtils.bookmarks.remove("folderCCCCCC");
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    lastModified: new Date(modified.getTime() - 5000),
    // Use `SOURCES.SYNC` to avoid bumping the change counter and flagging the
    // local toolbar as modified.
    source: PlacesUtils.bookmarks.SOURCES.SYNC,
  });

  info("Apply remote");
  let changesToUpload = await buf.apply({
    localTimeSeconds: modified.getTime() / 1000 + 10,
    remoteTimeSeconds: modified.getTime() / 1000 + 10,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkFFFF", "folderCCCCCC", PlacesUtils.bookmarks.menuGuid],
    "Should leave deleted C; F and menu with new remote structure unmerged"
  );

  deepEqual(
    changesToUpload,
    {
      menu: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "menu",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: modified.getTime() - 5000,
          children: ["bookmarkBBBB", "bookmarkEEEE"],
          title: BookmarksMenuTitle,
        },
      },
      toolbar: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "toolbar",
          type: "folder",
          parentid: "places",
          hasDupe: true,
          parentName: "",
          dateAdded: modified.getTime() - 5000,
          children: ["bookmarkDDDD", "bookmarkFFFF"],
          title: BookmarksToolbarTitle,
        },
      },
      // Upload E and F with new `parentid`.
      bookmarkEEEE: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkEEEE",
          type: "bookmark",
          parentid: "menu",
          hasDupe: true,
          parentName: BookmarksMenuTitle,
          dateAdded: modified.getTime() + 5000,
          bmkUri: "http://example.com/e",
          title: "E",
        },
      },
      bookmarkFFFF: {
        tombstone: false,
        counter: 1,
        synced: false,
        cleartext: {
          id: "bookmarkFFFF",
          type: "bookmark",
          parentid: "toolbar",
          hasDupe: true,
          parentName: BookmarksToolbarTitle,
          dateAdded: modified.getTime() - 5000,
          bmkUri: "http://example.com/f",
          title: "F",
        },
      },
      folderCCCCCC: {
        tombstone: true,
        counter: 1,
        synced: false,
        cleartext: {
          id: "folderCCCCCC",
          deleted: true,
        },
      },
    },
    "Should reupload menu, toolbar, E, F with new structure; tombstone for C"
  );

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
              guid: "bookmarkBBBB",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "B",
              url: "http://example.com/b",
            },
            {
              guid: "bookmarkEEEE",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 1,
              title: "E",
              url: "http://example.com/e",
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
              guid: "bookmarkDDDD",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "D",
              url: "http://example.com/d",
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
    "Should merge children of changed side first, even if they're older"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
