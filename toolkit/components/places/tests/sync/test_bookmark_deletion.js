/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_complex_orphaning() {
  let now = Date.now();

  let mergeTelemetryCounts;
  let buf = await openMirror("complex_orphaning", {
    recordStepTelemetry(name, took, counts) {
      if (name == "merge") {
        mergeTelemetryCounts = counts.filter(({ count }) => count > 0);
      }
    },
  });

  // On iOS, the mirror exists as a separate table. On Desktop, we have a
  // shadow mirror of synced local bookmarks without new changes.
  info("Set up mirror: ((Toolbar > A > B) (Menu > G > C > D))");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "folderBBBBBB",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
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
        children: ["folderBBBBBB"],
      },
      {
        id: "folderBBBBBB",
        parentid: "folderAAAAAA",
        type: "folder",
        title: "B",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderGGGGGG",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "G",
        children: [
          {
            guid: "folderCCCCCC",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "C",
            children: [
              {
                guid: "folderDDDDDD",
                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                title: "D",
              },
            ],
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
        children: ["folderGGGGGG"],
      },
      {
        id: "folderGGGGGG",
        parentid: "menu",
        type: "folder",
        title: "G",
        children: ["folderCCCCCC"],
      },
      {
        id: "folderCCCCCC",
        parentid: "folderGGGGGG",
        type: "folder",
        title: "C",
        children: ["folderDDDDDD"],
      },
      {
        id: "folderDDDDDD",
        parentid: "folderCCCCCC",
        type: "folder",
        title: "D",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: delete D, add B > E");
  await PlacesUtils.bookmarks.remove("folderDDDDDD");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkEEEE",
    parentGuid: "folderBBBBBB",
    title: "E",
    url: "http://example.com/e",
  });

  info("Make remote changes: delete B, add D > F");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "folderBBBBBB",
        deleted: true,
      },
      {
        id: "folderAAAAAA",
        parentid: "toolbar",
        type: "folder",
        title: "A",
      },
      {
        id: "folderDDDDDD",
        parentid: "folderCCCCCC",
        type: "folder",
        children: ["bookmarkFFFF"],
      },
      {
        id: "bookmarkFFFF",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "F",
        bmkUri: "http://example.com/f",
      },
    ])
  );

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkFFFF", "folderAAAAAA", "folderDDDDDD"],
    "Should leave deleted D; A and F with new remote structure unmerged"
  );
  deepEqual(
    mergeTelemetryCounts,
    [
      { name: "items", count: 10 },
      { name: "localDeletes", count: 1 },
      { name: "remoteDeletes", count: 1 },
    ],
    "Should record telemetry with structure change counts"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkEEEE", "bookmarkFFFF", "folderAAAAAA", "folderCCCCCC"],
      deleted: ["folderDDDDDD"],
    },
    "Should upload new records for (A > E), (C > F); tombstone for D"
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
              guid: "folderGGGGGG",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "G",
              children: [
                {
                  guid: "folderCCCCCC",
                  type: PlacesUtils.bookmarks.TYPE_FOLDER,
                  index: 0,
                  title: "C",
                  children: [
                    {
                      // D was deleted, so F moved to C, the closest surviving parent.
                      guid: "bookmarkFFFF",
                      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                      index: 0,
                      title: "F",
                      url: "http://example.com/f",
                    },
                  ],
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
              guid: "folderAAAAAA",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              index: 0,
              title: "A",
              children: [
                {
                  // B was deleted, so E moved to A.
                  guid: "bookmarkEEEE",
                  type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                  index: 0,
                  title: "E",
                  url: "http://example.com/e",
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
    "Should move orphans to closest surviving parent"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["folderDDDDDD"],
    "Should store local tombstone for D"
  );
  Assert.ok(
    is_time_ordered(now, tombstones[0].dateRemoved.getTime()),
    "Tombstone timestamp should be recent"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_locally_modified_remotely_deleted() {
  let mergeTelemetryCounts;
  let buf = await openMirror("locally_modified_remotely_deleted", {
    recordStepTelemetry(name, took, counts) {
      if (name == "merge") {
        mergeTelemetryCounts = counts.filter(({ count }) => count > 0);
      }
    },
  });

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
      },
      {
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
        children: [
          {
            guid: "bookmarkCCCC",
            title: "C",
            url: "http://example.com/c",
          },
          {
            guid: "folderDDDDDD",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "D",
            children: [
              {
                guid: "bookmarkEEEE",
                title: "E",
                url: "http://example.com/e",
              },
            ],
          },
        ],
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
        children: ["bookmarkAAAA", "folderBBBBBB"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
      },
      {
        id: "folderBBBBBB",
        parentid: "menu",
        type: "folder",
        title: "B",
        children: ["bookmarkCCCC", "folderDDDDDD"],
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "folderDDDDDD",
        parentid: "folderBBBBBB",
        type: "folder",
        title: "D",
        children: ["bookmarkEEEE"],
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: change A; B > ((D > F) G)");
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkAAAA",
    title: "A (local)",
    url: "http://example.com/a-local",
  });
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkFFFF",
    parentGuid: "folderDDDDDD",
    title: "F (local)",
    url: "http://example.com/f-local",
  });
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkGGGG",
    parentGuid: "folderBBBBBB",
    title: "G (local)",
    url: "http://example.com/g-local",
  });

  info("Make remote changes: delete A, B");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: [],
    },
    {
      id: "bookmarkAAAA",
      deleted: true,
    },
    {
      id: "folderBBBBBB",
      deleted: true,
    },
    {
      id: "bookmarkCCCC",
      deleted: true,
    },
    {
      id: "folderDDDDDD",
      deleted: true,
    },
    {
      id: "bookmarkEEEE",
      deleted: true,
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkAAAA", PlacesUtils.bookmarks.menuGuid],
    "Should leave revived A and menu with new remote structure unmerged"
  );
  deepEqual(
    mergeTelemetryCounts,
    [
      { name: "items", count: 8 },
      { name: "localRevives", count: 1 },
      { name: "remoteDeletes", count: 2 },
    ],
    "Should record telemetry for local item and remote folder deletions"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkAAAA", "bookmarkFFFF", "bookmarkGGGG", "menu"],
      deleted: [],
    },
    "Should upload A, relocated local orphans, and menu"
  );

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
          title: "A (local)",
          url: "http://example.com/a-local",
        },
        {
          guid: "bookmarkFFFF",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "F (local)",
          url: "http://example.com/f-local",
        },
        {
          guid: "bookmarkGGGG",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "G (local)",
          url: "http://example.com/g-local",
        },
      ],
    },
    "Should restore A and relocate (F G) to menu"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(tombstones, [], "Should not store local tombstones");

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_locally_deleted_remotely_modified() {
  let now = Date.now();

  let mergeTelemetryCounts;
  let buf = await openMirror("locally_deleted_remotely_modified", {
    recordStepTelemetry(name, took, counts) {
      if (name == "merge") {
        mergeTelemetryCounts = counts.filter(({ count }) => count > 0);
      }
    },
  });

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
      },
      {
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
        children: [
          {
            guid: "bookmarkCCCC",
            title: "C",
            url: "http://example.com/c",
          },
          {
            guid: "folderDDDDDD",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "D",
            children: [
              {
                guid: "bookmarkEEEE",
                title: "E",
                url: "http://example.com/e",
              },
            ],
          },
        ],
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
        children: ["bookmarkAAAA", "folderBBBBBB"],
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "A",
        bmkUri: "http://example.com/a",
      },
      {
        id: "folderBBBBBB",
        parentid: "menu",
        type: "folder",
        title: "B",
        children: ["bookmarkCCCC", "folderDDDDDD"],
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "folderDDDDDD",
        parentid: "folderBBBBBB",
        type: "folder",
        title: "D",
        children: ["bookmarkEEEE"],
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: delete A, B");
  await PlacesUtils.bookmarks.remove("bookmarkAAAA");
  await PlacesUtils.bookmarks.remove("folderBBBBBB");

  info("Make remote changes: change A; B > ((D > F) G)");
  await storeRecords(buf, [
    {
      id: "bookmarkAAAA",
      parentid: "menu",
      type: "bookmark",
      title: "A (remote)",
      bmkUri: "http://example.com/a-remote",
    },
    {
      id: "folderBBBBBB",
      parentid: "menu",
      type: "folder",
      title: "B (remote)",
      children: ["bookmarkCCCC", "folderDDDDDD", "bookmarkGGGG"],
    },
    {
      id: "folderDDDDDD",
      parentid: "folderBBBBBB",
      type: "folder",
      title: "D",
      children: ["bookmarkEEEE", "bookmarkFFFF"],
    },
    {
      id: "bookmarkFFFF",
      parentid: "folderDDDDDD",
      type: "bookmark",
      title: "F (remote)",
      bmkUri: "http://example.com/f-remote",
    },
    {
      id: "bookmarkGGGG",
      parentid: "folderBBBBBB",
      type: "bookmark",
      title: "G (remote)",
      bmkUri: "http://example.com/g-remote",
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkFFFF", "bookmarkGGGG", "folderBBBBBB", "folderDDDDDD"],
    "Should leave deleted B and D; relocated F and G unmerged"
  );
  deepEqual(
    mergeTelemetryCounts,
    [
      { name: "items", count: 8 },
      { name: "remoteRevives", count: 1 },
      { name: "localDeletes", count: 2 },
    ],
    "Should record telemetry for remote item and local folder deletions"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkFFFF", "bookmarkGGGG", "menu"],
      deleted: ["bookmarkCCCC", "bookmarkEEEE", "folderBBBBBB", "folderDDDDDD"],
    },
    "Should upload relocated remote orphans and menu"
  );

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
          title: "A (remote)",
          url: "http://example.com/a-remote",
        },
        {
          guid: "bookmarkFFFF",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "F (remote)",
          url: "http://example.com/f-remote",
        },
        {
          guid: "bookmarkGGGG",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "G (remote)",
          url: "http://example.com/g-remote",
        },
      ],
    },
    "Should restore A and relocate (F G) to menu"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["bookmarkCCCC", "bookmarkEEEE", "folderBBBBBB", "folderDDDDDD"],
    "Should store local tombstones for deleted items; remove for undeleted"
  );
  Assert.ok(
    tombstones.every(({ dateRemoved }) =>
      is_time_ordered(now, dateRemoved.getTime())
    ),
    "Local tombstone timestamps should be recent"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move_to_new_then_delete() {
  let buf = await openMirror("move_to_new_then_delete");

  info("Set up mirror: A > B > (C D)");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "A",
        children: [
          {
            guid: "folderBBBBBB",
            type: PlacesUtils.bookmarks.TYPE_FOLDER,
            title: "B",
            children: [
              {
                guid: "bookmarkCCCC",
                url: "http://example.com/c",
                title: "C",
              },
              {
                guid: "bookmarkDDDD",
                url: "http://example.com/d",
                title: "D",
              },
            ],
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
        children: ["folderBBBBBB"],
      },
      {
        id: "folderBBBBBB",
        parentid: "folderAAAAAA",
        type: "folder",
        title: "B",
        children: ["bookmarkCCCC", "bookmarkDDDD"],
      },
      {
        id: "bookmarkCCCC",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "C",
        bmkUri: "http://example.com/c",
      },
      {
        id: "bookmarkDDDD",
        parentid: "folderBBBBBB",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: E > A, delete E");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    guid: "folderEEEEEE",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "E",
  });
  await PlacesUtils.bookmarks.update({
    guid: "folderAAAAAA",
    parentGuid: "folderEEEEEE",
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  // E isn't synced, so we shouldn't upload a tombstone.
  await PlacesUtils.bookmarks.remove("folderEEEEEE");

  info("Make remote changes");
  await storeRecords(buf, [
    {
      id: "bookmarkCCCC",
      parentid: "folderBBBBBB",
      type: "bookmark",
      title: "C (remote)",
      bmkUri: "http://example.com/c-remote",
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["bookmarkCCCC", PlacesUtils.bookmarks.toolbarGuid],
    "Should leave revived C and toolbar with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkCCCC", "menu", "toolbar"],
      deleted: ["bookmarkDDDD", "folderAAAAAA", "folderBBBBBB"],
    },
    "Should upload records for Menu > C, Toolbar"
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
              title: "C (remote)",
              url: "http://example.com/c-remote",
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: BookmarksToolbarTitle,
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
    "Should move C to closest surviving parent"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["bookmarkDDDD", "folderAAAAAA", "folderBBBBBB"],
    "Should store local tombstones for (D A B)"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_nonexistent_on_one_side() {
  let buf = await openMirror("nonexistent_on_one_side");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  // A doesn't exist in the mirror.
  info("Create local tombstone for nonexistent remote item A");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkAAAA",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "A",
    url: "http://example.com/a",
    // Pretend a bookmark restore added A, so that we'll write a tombstone when
    // we remove it.
    source: PlacesUtils.bookmarks.SOURCES.RESTORE,
  });
  await PlacesUtils.bookmarks.remove("bookmarkAAAA");

  // B doesn't exist in Places, and we don't currently persist tombstones (bug
  // 1343103), so we should ignore it.
  info("Create remote tombstone for nonexistent local item B");
  await storeRecords(buf, [
    {
      id: "bookmarkBBBB",
      deleted: true,
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.menuGuid],
    "Should leave menu with new remote structure unmerged"
  );

  let menuInfo = await PlacesUtils.bookmarks.fetch(
    PlacesUtils.bookmarks.menuGuid
  );

  // We should still upload a record for the menu, since we changed its
  // children when we added then removed A.
  deepEqual(changesToUpload, {
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
        dateAdded: menuInfo.dateAdded.getTime(),
        title: BookmarksMenuTitle,
        children: [],
      },
    },
  });

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_clear_folder_then_delete() {
  let buf = await openMirror("clear_folder_then_delete");

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
          {
            guid: "bookmarkFFFF",
            url: "http://example.com/f",
            title: "F",
          },
        ],
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
        children: ["folderAAAAAA", "folderDDDDDD"],
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
      {
        id: "folderDDDDDD",
        parentid: "menu",
        type: "folder",
        title: "D",
        children: ["bookmarkEEEE", "bookmarkFFFF"],
      },
      {
        id: "bookmarkEEEE",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "E",
        bmkUri: "http://example.com/e",
      },
      {
        id: "bookmarkFFFF",
        parentid: "folderDDDDDD",
        type: "bookmark",
        title: "F",
        bmkUri: "http://example.com/f",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: Menu > E, Mobile > F, delete D");
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkEEEE",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  });
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkFFFF",
    parentGuid: PlacesUtils.bookmarks.mobileGuid,
    index: 0,
  });
  await PlacesUtils.bookmarks.remove("folderDDDDDD");

  info("Make remote changes: Menu > B, Unfiled > C, delete A");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["bookmarkBBBB", "folderDDDDDD"],
    },
    {
      id: "bookmarkBBBB",
      parentid: "menu",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b",
    },
    {
      id: "unfiled",
      parentid: "places",
      type: "folder",
      children: ["bookmarkCCCC"],
    },
    {
      id: "bookmarkCCCC",
      parentid: "unfiled",
      type: "bookmark",
      title: "C",
      bmkUri: "http://example.com/c",
    },
    {
      id: "folderAAAAAA",
      deleted: true,
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.menuGuid, PlacesUtils.bookmarks.mobileGuid],
    "Should leave menu and mobile with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["bookmarkEEEE", "bookmarkFFFF", "menu", "mobile"],
      deleted: ["folderDDDDDD"],
    },
    "Should upload locally moved and deleted items"
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
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 3,
          title: UnfiledBookmarksTitle,
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
          guid: PlacesUtils.bookmarks.mobileGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 4,
          title: MobileBookmarksTitle,
          children: [
            {
              guid: "bookmarkFFFF",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "F",
              url: "http://example.com/f",
            },
          ],
        },
      ],
    },
    "Should not orphan moved children of a deleted folder"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["folderDDDDDD"],
    "Should store local tombstone for D"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_newer_move_to_deleted() {
  let buf = await openMirror("test_newer_move_to_deleted");

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
        ],
      },
      {
        guid: "folderCCCCCC",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "C",
        children: [
          {
            guid: "bookmarkDDDD",
            url: "http://example.com/d",
            title: "D",
          },
        ],
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
        children: ["folderAAAAAA", "folderCCCCCC"],
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
      {
        id: "folderCCCCCC",
        parentid: "menu",
        type: "folder",
        title: "C",
        children: ["bookmarkDDDD"],
      },
      {
        id: "bookmarkDDDD",
        parentid: "folderCCCCCC",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
      },
    ],
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  let now = Date.now();

  // A will have a newer local timestamp. However, we should *not* revert
  // remotely moving B to the toolbar. (Locally, B exists in A, but we
  // deleted the now-empty A remotely).
  info("Make local changes: A > E, Toolbar > D, delete C");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkEEEE",
    parentGuid: "folderAAAAAA",
    title: "E",
    url: "http://example.com/e",
    dateAdded: new Date(now),
    lastModified: new Date(now),
  });
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkDDDD",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
    lastModified: new Date(now),
  });
  await PlacesUtils.bookmarks.remove("folderCCCCCC");

  // C will have a newer remote timestamp. However, we should *not* revert
  // locally moving D to the toolbar. (Locally, D exists in C, but we
  // deleted the now-empty C locally).
  info("Make remote changes: C > F, Toolbar > B, delete A");
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["folderCCCCCC"],
    },
    {
      id: "folderCCCCCC",
      parentid: "menu",
      type: "folder",
      title: "C",
      children: ["bookmarkDDDD", "bookmarkFFFF"],
      modified: now / 1000 + 5,
    },
    {
      id: "bookmarkFFFF",
      parentid: "folderCCCCCC",
      type: "bookmark",
      title: "F",
      bmkUri: "http://example.com/f",
    },
    {
      id: "toolbar",
      parentid: "places",
      type: "folder",
      children: ["bookmarkBBBB"],
      modified: now / 1000 - 5,
    },
    {
      id: "bookmarkBBBB",
      parentid: "toolbar",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b",
      modified: now / 1000 - 5,
    },
    {
      id: "folderAAAAAA",
      deleted: true,
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply({
    localTimeSeconds: now / 1000,
    remoteTimeSeconds: now / 1000,
  });
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [
      "bookmarkFFFF",
      "folderCCCCCC",
      PlacesUtils.bookmarks.menuGuid,
      PlacesUtils.bookmarks.toolbarGuid,
    ],
    "Should leave deleted C; revived F and roots with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: [
        "bookmarkDDDD",
        "bookmarkEEEE",
        "bookmarkFFFF",
        "menu",
        "toolbar",
      ],
      deleted: ["folderCCCCCC"],
    },
    "Should upload new and moved items"
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
              guid: "bookmarkDDDD",
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              index: 0,
              title: "D",
              url: "http://example.com/d",
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
    "Should not decide to keep newly moved items in deleted parents"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["folderCCCCCC"],
    "Should store local tombstone for C"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
