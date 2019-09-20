/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_queries() {
  let buf = await openMirror("queries");

  info("Set up places");

  // create a tag and grab the local folder ID.
  let tag = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "a-tag",
  });

  await PlacesTestUtils.markBookmarksAsSynced();

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        // this entry has a tag= query param for a tag that exists.
        guid: "queryAAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title: "TAG_QUERY query",
        url: `place:tag=a-tag&&sort=14&maxResults=10`,
      },
      {
        // this entry has a tag= query param for a tag that doesn't exist.
        guid: "queryBBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title: "TAG_QUERY query but invalid folder id",
        url: `place:tag=b-tag&sort=14&maxResults=10`,
      },
      {
        // this entry has no tag= query param.
        guid: "queryCCCCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title: "TAG_QUERY without a folder at all",
        url: "place:sort=14&maxResults=10",
      },
      {
        // this entry has only a tag= query.
        guid: "queryDDDDDDD",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title: "TAG_QUERY without a folder at all",
        url: "place:tag=a-tag",
      },
    ],
  });

  info("Make remote changes");
  await storeRecords(
    buf,
    shuffle([
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: [
          "queryEEEEEEE",
          "queryFFFFFFF",
          "queryGGGGGGG",
          "queryHHHHHHH",
          "queryIIIIIII",
        ],
      },
      {
        // Legacy tag query.
        id: "queryEEEEEEE",
        parentid: "toolbar",
        type: "query",
        title: "E",
        bmkUri: "place:type=7&folder=999",
        folderName: "taggy",
      },
      {
        // New tag query.
        id: "queryFFFFFFF",
        parentid: "toolbar",
        type: "query",
        title: "F",
        bmkUri: "place:tag=a-tag",
        folderName: "a-tag",
      },
      {
        // Legacy tag query referencing the same tag as the new query.
        id: "queryGGGGGGG",
        parentid: "toolbar",
        type: "query",
        title: "G",
        bmkUri: "place:type=7&folder=111&something=else",
        folderName: "a-tag",
      },
      {
        // Legacy folder lookup query.
        id: "queryHHHHHHH",
        parentid: "toolbar",
        type: "query",
        title: "H",
        bmkUri: "place:folder=1",
      },
      {
        // Legacy tag query with invalid tag folder name.
        id: "queryIIIIIII",
        parentid: "toolbar",
        type: "query",
        title: "I",
        bmkUri: "place:type=7&folder=222",
        folderName: "    ",
      },
    ])
  );

  info("Create records to upload");
  let changes = await buf.apply();
  deepEqual(
    Object.keys(changes),
    [
      "menu",
      "toolbar",
      "queryAAAAAAA",
      "queryBBBBBBB",
      "queryCCCCCCC",
      "queryDDDDDDD",
      "queryEEEEEEE",
      "queryGGGGGGG",
      "queryHHHHHHH",
      "queryIIIIIII",
    ],
    "Should upload roots, new queries, and rewritten queries"
  );
  Assert.strictEqual(changes.queryAAAAAAA.cleartext.folderName, tag.title);
  Assert.strictEqual(changes.queryBBBBBBB.cleartext.folderName, "b-tag");
  Assert.strictEqual(changes.queryCCCCCCC.cleartext.folderName, undefined);
  Assert.strictEqual(changes.queryDDDDDDD.cleartext.folderName, tag.title);
  Assert.strictEqual(changes.queryIIIIIII.tombstone, true);

  await assertLocalTree(
    PlacesUtils.bookmarks.toolbarGuid,
    {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
      children: [
        {
          guid: "queryEEEEEEE",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "E",
          url: "place:tag=taggy",
        },
        {
          guid: "queryFFFFFFF",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "F",
          url: "place:tag=a-tag",
        },
        {
          guid: "queryGGGGGGG",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "G",
          url: "place:tag=a-tag",
        },
        {
          guid: "queryHHHHHHH",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 3,
          title: "H",
          url: "place:folder=1&excludeItems=1",
        },
      ],
    },
    "Should rewrite legacy remote queries"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Bug 632287.
add_task(async function test_mismatched_folder_types() {
  let buf = await openMirror("mismatched_types");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "l1nZZXfB8nC7",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "Innerst i Sneglehode",
      },
      {
        guid: "livemarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
      },
    ],
  });
  // Livemarks have been removed in bug 1477671, but Sync still checks the anno
  // to distinguish them from folders.
  await setItemAnnotation(
    "livemarkBBBB",
    PlacesUtils.LMANNO_FEEDURI,
    "http://example.com/b"
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(buf, [
    {
      id: "toolbar",
      parentid: "places",
      type: "folder",
      children: ["l1nZZXfB8nC7", "livemarkBBBB"],
    },
    {
      id: "l1nZZXfB8nC7",
      type: "livemark",
      siteUri: "http://sneglehode.wordpress.com/",
      feedUri: "http://sneglehode.wordpress.com/feed/",
      parentName: "Bookmarks Toolbar",
      title: "Innerst i Sneglehode",
      description: null,
      children: [
        "HCRq40Rnxhrd",
        "YeyWCV1RVsYw",
        "GCceVZMhvMbP",
        "sYi2hevdArlF",
        "vjbZlPlSyGY8",
        "UtjUhVyrpeG6",
        "rVq8WMG2wfZI",
        "Lx0tcy43ZKhZ",
        "oT74WwV8_j4P",
        "IztsItWVSo3-",
      ],
      parentid: "toolbar",
    },
    {
      id: "livemarkBBBB",
      parentid: "toolbar",
      type: "folder",
      title: "B",
      children: [],
    },
  ]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  // We leave livemarks unmerged because we're about to upload tombstones for
  // them, anyway.
  deepEqual(
    await buf.fetchUnmergedGuids(),
    ["l1nZZXfB8nC7", "livemarkBBBB", PlacesUtils.bookmarks.toolbarGuid],
    "Should leave livemarks and toolbar with new remote structure unmerged"
  );

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: ["toolbar"],
      deleted: ["l1nZZXfB8nC7", "livemarkBBBB"],
    },
    "Should delete livemarks remotely"
  );

  await assertLocalTree(
    PlacesUtils.bookmarks.toolbarGuid,
    {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    },
    "Should delete livemarks locally"
  );

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(
    tombstones.map(({ guid }) => guid),
    ["l1nZZXfB8nC7", "livemarkBBBB"],
    "Should store local tombstones for livemarks"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_different_but_compatible_bookmark_types() {
  let buf = await openMirror("partial_queries");
  try {
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [
        {
          guid: "bookmarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "not yet a query",
          url: "about:blank",
        },
        {
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "a query",
          url: "place:foo",
        },
      ],
    });

    let changes = await buf.apply();
    // We should have an outgoing record for bookmarkA with type=bookmark
    // and bookmarkB with type=query.
    Assert.equal(changes.bookmarkAAAA.cleartext.type, "bookmark");
    Assert.equal(changes.bookmarkBBBB.cleartext.type, "query");

    // Now pretend that same records are already on the server.
    await storeRecords(
      buf,
      [
        {
          id: "menu",
          parentid: "places",
          type: "folder",
          children: ["bookmarkAAAA", "bookmarkBBBB"],
        },
        {
          id: "bookmarkAAAA",
          parentid: "menu",
          type: "bookmark",
          title: "not yet a query",
          bmkUri: "about:blank",
        },
        {
          id: "bookmarkBBBB",
          parentid: "menu",
          type: "query",
          title: "a query",
          bmkUri: "place:foo",
        },
      ],
      { needsMerge: false }
    );
    await PlacesTestUtils.markBookmarksAsSynced();

    // change the url of bookmarkA to be a "real" query and of bookmarkB to
    // no longer be a query.
    await PlacesUtils.bookmarks.update({
      guid: "bookmarkAAAA",
      url: "place:type=6&sort=14&maxResults=10",
    });
    await PlacesUtils.bookmarks.update({
      guid: "bookmarkBBBB",
      url: "about:robots",
    });

    changes = await buf.apply();
    // We should have an outgoing record for bookmarkA with type=query and
    // for bookmarkB with type=bookmark
    Assert.equal(changes.bookmarkAAAA.cleartext.type, "query");
    Assert.equal(changes.bookmarkBBBB.cleartext.type, "bookmark");
  } finally {
    await buf.finalize();
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});

add_task(async function test_incompatible_types() {
  try {
    let buf = await openMirror("incompatible_types");

    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [
        {
          guid: "AAAAAAAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          title: "a bookmark",
          url: "about:blank",
        },
      ],
    });

    await buf.apply();

    // Now pretend that same records are already on the server with incompatible
    // types.
    await storeRecords(
      buf,
      [
        {
          id: "menu",
          parentid: "places",
          type: "folder",
          children: ["AAAAAAAAAAAA"],
        },
        {
          id: "AAAAAAAAAAAA",
          parentid: "menu",
          type: "folder",
          title: "conflicting folder",
        },
      ],
      { needsMerge: true }
    );
    await PlacesTestUtils.markBookmarksAsSynced();

    await Assert.rejects(
      buf.apply(),
      /Can't merge local kind Bookmark and remote kind Folder/
    );
  } finally {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesSyncUtils.bookmarks.reset();
  }
});
