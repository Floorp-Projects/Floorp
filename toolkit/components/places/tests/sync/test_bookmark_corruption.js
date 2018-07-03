/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function getCountOfBookmarkRows(db) {
  let queryRows = await db.execute("SELECT COUNT(*) FROM moz_bookmarks");
  Assert.equal(queryRows.length, 1);
  return queryRows[0].getResultByIndex(0);
}

add_task(async function test_corrupt_roots() {
  let buf = await openMirror("corrupt_roots");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes: Menu > Unfiled");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["unfiled", "bookmarkAAAA"],
  }, {
    id: "unfiled",
    type: "folder",
    children: ["bookmarkBBBB"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }, {
    id: "toolbar",
    deleted: true,
  }]);

  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  deepEqual(changesToUpload, {}, "Should not reupload invalid roots");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "bookmarkAAAA",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "A",
        url: "http://example.com/a",
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
      children: [{
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "B",
        url: "http://example.com/b",
      }],
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should not corrupt local roots");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_missing_children() {
  let buf = await openMirror("missing_childen");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes: A > ([B] C [D E])");
  {
    await storeRecords(buf, shuffle([{
      id: "menu",
      type: "folder",
      children: ["bookmarkBBBB", "bookmarkCCCC", "bookmarkDDDD",
                 "bookmarkEEEE"],
    }, {
      id: "bookmarkCCCC",
      type: "bookmark",
      bmkUri: "http://example.com/c",
      title: "C",
    }]));
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload menu with missing children (B D E)");
    await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "C",
        url: "http://example.com/c",
      }],
    }, "Menu children should be (C)");
    deepEqual(await buf.fetchRemoteOrphans(), {
      missingChildren: ["bookmarkBBBB", "bookmarkDDDD", "bookmarkEEEE"],
      missingParents: [],
      parentsWithGaps: [],
    }, "Should report (B D E) as missing");
  }

  info("Add (B E) to remote");
  {
    await storeRecords(buf, shuffle([{
      id: "bookmarkBBBB",
      type: "bookmark",
      title: "B",
      bmkUri: "http://example.com/b",
    }, {
      id: "bookmarkEEEE",
      type: "bookmark",
      title: "E",
      bmkUri: "http://example.com/e",
    }]));
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload menu with missing child D");
    await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "B",
        url: "http://example.com/b",
      }, {
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "C",
        url: "http://example.com/c",
      }, {
        guid: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 2,
        title: "E",
        url: "http://example.com/e",
      }],
    }, "Menu children should be (B C E)");
    deepEqual(await buf.fetchRemoteOrphans(), {
      missingChildren: ["bookmarkDDDD"],
      missingParents: [],
      parentsWithGaps: [],
    }, "Should report (D) as missing");
  }

  info("Add D to remote");
  {
    await storeRecords(buf, [{
      id: "bookmarkDDDD",
      type: "bookmark",
      title: "D",
      bmkUri: "http://example.com/d",
    }]);
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload complete menu");
    await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "B",
        url: "http://example.com/b",
      }, {
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "C",
        url: "http://example.com/c",
      }, {
        guid: "bookmarkDDDD",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 2,
        title: "D",
        url: "http://example.com/d",
      }, {
        guid: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 3,
        title: "E",
        url: "http://example.com/e",
      }],
    }, "Menu children should be (B C D E)");
    deepEqual(await buf.fetchRemoteOrphans(), {
      missingChildren: [],
      missingParents: [],
      parentsWithGaps: [],
    }, "Should not report any missing children");
  }

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_new_orphan_without_local_parent() {
  let buf = await openMirror("new_orphan_without_local_parent");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  // A doesn't exist locally, so we move the bookmarks into "unfiled" without
  // reuploading. When the partial uploader returns and uploads A, we'll
  // move the bookmarks to the correct folder.
  info("Make remote changes: [A] > (B C D)");
  await storeRecords(buf, shuffle([{
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B (remote)",
    bmkUri: "http://example.com/b-remote",
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C (remote)",
    bmkUri: "http://example.com/c-remote",
  }, {
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D (remote)",
    bmkUri: "http://example.com/d-remote",
  }]));

  info("Apply remote with (B C D)");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload orphans (B C D)");
  }

  await assertLocalTree(PlacesUtils.bookmarks.unfiledGuid, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 3,
    title: UnfiledBookmarksTitle,
    children: [{
      guid: "bookmarkBBBB",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 0,
      title: "B (remote)",
      url: "http://example.com/b-remote",
    }, {
      guid: "bookmarkCCCC",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 1,
      title: "C (remote)",
      url: "http://example.com/c-remote",
    }, {
      guid: "bookmarkDDDD",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 2,
      title: "D (remote)",
      url: "http://example.com/d-remote",
    }],
  }, "Should move (B C D) to unfiled");

  // A is an orphan because we don't have E locally, but we should move
  // (B C D) into A.
  info("Add [E] > A to remote");
  await storeRecords(buf, [{
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkDDDD", "bookmarkCCCC", "bookmarkBBBB"],
  }]);

  info("Apply remote with A");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload orphan A");
  }

  await assertLocalTree(PlacesUtils.bookmarks.unfiledGuid, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 3,
    title: UnfiledBookmarksTitle,
    children: [{
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "A",
      children: [{
        guid: "bookmarkDDDD",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "D (remote)",
        url: "http://example.com/d-remote",
      }, {
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "C (remote)",
        url: "http://example.com/c-remote",
      }, {
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 2,
        title: "B (remote)",
        url: "http://example.com/b-remote",
      }],
    }],
  }, "Should move (D C B) into A");

  info("Add E to remote");
  await storeRecords(buf, [{
    id: "folderEEEEEE",
    type: "folder",
    title: "E",
    children: ["folderAAAAAA"],
  }]);

  info("Apply remote with E");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload orphan E");
  }

  // E is still in unfiled because we don't have a record for the menu.
  await assertLocalTree(PlacesUtils.bookmarks.unfiledGuid, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 3,
    title: UnfiledBookmarksTitle,
    children: [{
      guid: "folderEEEEEE",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "E",
      children: [{
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "A",
        children: [{
          guid: "bookmarkDDDD",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "D (remote)",
          url: "http://example.com/d-remote",
        }, {
          guid: "bookmarkCCCC",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "C (remote)",
          url: "http://example.com/c-remote",
        }, {
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "B (remote)",
          url: "http://example.com/b-remote",
        }],
      }],
    }],
  }, "Should move A into E");

  info("Add Menu > E to remote");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["folderEEEEEE"],
  }]);

  info("Apply remote with menu");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload after forming complete tree");
  }

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "folderEEEEEE",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "E",
        children: [{
          guid: "folderAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "A",
          children: [{
            guid: "bookmarkDDDD",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 0,
            title: "D (remote)",
            url: "http://example.com/d-remote",
          }, {
            guid: "bookmarkCCCC",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 1,
            title: "C (remote)",
            url: "http://example.com/c-remote",
          }, {
            guid: "bookmarkBBBB",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 2,
            title: "B (remote)",
            url: "http://example.com/b-remote",
          }],
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should form complete tree after applying E");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move_into_orphaned() {
  let buf = await openMirror("move_into_orphaned");

  info("Set up mirror: Menu > (A B (C > (D (E > F))))");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "bookmarkAAAA",
      url: "http://example.com/a",
      title: "A",
    }, {
      guid: "bookmarkBBBB",
      url: "http://example.com/b",
      title: "B",
    }, {
      guid: "folderCCCCCC",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "C",
      children: [{
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
      }, {
        guid: "folderEEEEEE",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "E",
        children: [{
          guid: "bookmarkFFFF",
          title: "F",
          url: "http://example.com/f",
        }],
      }],
    }],
  });
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA", "bookmarkBBBB", "folderCCCCCC"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }, {
    id: "folderCCCCCC",
    type: "folder",
    title: "C",
    children: ["bookmarkDDDD", "folderEEEEEE"],
  }, {
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D",
    bmkUri: "http://example.com/d",
  }, {
    id: "folderEEEEEE",
    type: "folder",
    title: "E",
    children: ["bookmarkFFFF"],
  }, {
    id: "bookmarkFFFF",
    type: "bookmark",
    title: "F",
    bmkUri: "http://example.com/f",
  }], { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes: delete D, add E > I");
  await PlacesUtils.bookmarks.remove("bookmarkDDDD");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkIIII",
    parentGuid: "folderEEEEEE",
    title: "I (local)",
    url: "http://example.com/i",
  });

  // G doesn't exist on the server.
  info("Make remote changes: ([G] > A (C > (D H E))), (C > H)");
  await storeRecords(buf, shuffle([{
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }, {
    id: "folderCCCCCC",
    type: "folder",
    title: "C",
    children: ["bookmarkDDDD", "bookmarkHHHH", "folderEEEEEE"],
  }, {
    id: "bookmarkHHHH",
    type: "bookmark",
    title: "H (remote)",
    bmkUri: "http://example.com/h-remote",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["bookmarkIIII", "folderCCCCCC", "folderEEEEEE"],
    deleted: ["bookmarkDDDD"],
  }, "Should upload records for (I C E); tombstone for D");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        // A remains in its original place, since we don't use the `parentid`,
        // and we don't have a record for G.
        guid: "bookmarkAAAA",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "A",
        url: "http://example.com/a",
      }, {
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "B",
        url: "http://example.com/b",
      }, {
        // C exists on the server, so we take its children and order. D was
        // deleted locally, and doesn't exist remotely. C is also a child of
        // G, but we don't have a record for it on the server.
        guid: "folderCCCCCC",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 2,
        title: "C",
        children: [{
          guid: "bookmarkHHHH",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "H (remote)",
          url: "http://example.com/h-remote",
        }, {
          guid: "folderEEEEEE",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 1,
          title: "E",
          children: [{
            guid: "bookmarkFFFF",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 0,
            title: "F",
            url: "http://example.com/f",
          }, {
            guid: "bookmarkIIII",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 1,
            title: "I (local)",
            url: "http://example.com/i",
          }],
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should treat local tree as canonical if server is missing new parent");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_new_orphan_with_local_parent() {
  let buf = await openMirror("new_orphan_with_local_parent");

  info("Set up mirror: A > (B D)");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      children: [{
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
      }, {
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
      }],
    }],
  });
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkBBBB", "bookmarkEEEE"],
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }, {
    id: "bookmarkEEEE",
    type: "bookmark",
    title: "E",
    bmkUri: "http://example.com/e",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  // Simulate a partial write by another device that uploaded only B and C. A
  // exists locally, so we can move B and C into the correct folder, but not
  // the correct positions.
  info("Set up remote with orphans: [A] > (C D)");
  await storeRecords(buf, [{
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D (remote)",
    bmkUri: "http://example.com/d-remote",
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C (remote)",
    bmkUri: "http://example.com/c-remote",
  }]);

  info("Apply remote with (C D)");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload orphans (C D)");
  }

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "A",
        children: [{
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "B",
          url: "http://example.com/b",
        }, {
          guid: "bookmarkEEEE",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "E",
          url: "http://example.com/e",
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
      children: [{
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "C (remote)",
        url: "http://example.com/c-remote",
      }, {
        guid: "bookmarkDDDD",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "D (remote)",
        url: "http://example.com/d-remote",
      }],
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should move (C D) to unfiled");

  // The partial uploader returns and uploads A.
  info("Add A to remote");
  await storeRecords(buf, [{
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkCCCC", "bookmarkDDDD", "bookmarkEEEE", "bookmarkBBBB"],
  }]);

  info("Apply remote with A");
  {
    let changesToUpload = await buf.apply();
    deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
    let idsToUpload = inspectChangeRecords(changesToUpload);
    deepEqual(idsToUpload, {
      updated: [],
      deleted: [],
    }, "Should not reupload orphan A");
  }

  await assertLocalTree("folderAAAAAA", {
    guid: "folderAAAAAA",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "A",
    children: [{
      guid: "bookmarkCCCC",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 0,
      title: "C (remote)",
      url: "http://example.com/c-remote",
    }, {
      guid: "bookmarkDDDD",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 1,
      title: "D (remote)",
      url: "http://example.com/d-remote",
    }, {
      guid: "bookmarkEEEE",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 2,
      title: "E",
      url: "http://example.com/e",
    }, {
      guid: "bookmarkBBBB",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 3,
      title: "B",
      url: "http://example.com/b",
    }],
  }, "Should update child positions once A exists in mirror");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_tombstone_as_child() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let buf = await openMirror("tombstone_as_child");
  // Setup the mirror such that an incoming folder references a tombstone
  // as a child.
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkAAAA", "bookmarkTTTT", "bookmarkBBBB"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "Bookmark A",
    bmkUri: "http://example.com/a",
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "Bookmark B",
    bmkUri: "http://example.com/b",
  }, {
    id: "bookmarkTTTT",
    deleted: true,
  }]), { needsMerge: true });

  let changesToUpload = await buf.apply();
  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload.deleted, [], "no new tombstones were created.");
  // Note that we do not attempt to re-upload the folder with the correct
  // list of children - but we might take some action in the future around
  // this.
  deepEqual(idsToUpload.updated, [], "parent is not re-uploaded");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "folderAAAAAA",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "A",
        children: [{
          guid: "bookmarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          url: "http://example.com/a",
          index: 0,
          title: "Bookmark A",
        }, {
          // Note that this was the 3rd child specified on the server record,
          // but we we've correctly moved it back to being the second after
          // ignoring the tombstone.
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          url: "http://example.com/b",
          index: 1,
          title: "Bookmark B",
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should have ignored tombstone record");
  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_non_syncable_items() {
  let buf = await openMirror("non_syncable_items");

  info("Insert local orphaned left pane queries");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      guid: "folderLEFTPQ",
      url: "place:folder=SOMETHING",
      title: "Some query",
    }, {
      guid: "folderLEFTPC",
      url: "place:folder=SOMETHING_ELSE",
      title: "A query under 'All Bookmarks'",
    }],
  });

  info("Insert syncable local items (A > B) that exist in non-syncable remote root H");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      // A is non-syncable remotely, but B doesn't exist remotely, so we'll
      // remove A from the merged structure, and move B to the menu.
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      children: [{
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
      }],
    }],
  });

  info("Insert non-syncable local root C and items (C > (D > E) F)");
  await insertLocalRoot({
    guid: "rootCCCCCCCC",
    title: "C",
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: "rootCCCCCCCC",
    children: [{
      guid: "folderDDDDDD",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "D",
      children: [{
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
      }],
    }, {
      guid: "bookmarkFFFF",
      url: "http://example.com/f",
      title: "F",
    }],
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(buf, [{
    // H is a non-syncable root that only exists remotely.
    id: "rootHHHHHHHH",
    type: "folder",
    parentid: "places",
    title: "H",
    children: ["folderAAAAAA"],
  }, {
    // A is a folder with children that's non-syncable remotely, and syncable
    // locally. We should remove A and its descendants locally, since its parent
    // H is known to be non-syncable remotely.
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkFFFF", "bookmarkIIII"],
  }, {
    // F exists in two different non-syncable folders: C locally, and A
    // remotely.
    id: "bookmarkFFFF",
    type: "bookmark",
    title: "F",
    bmkUri: "http://example.com/f",
  }, {
    id: "bookmarkIIII",
    type: "query",
    title: "I",
    bmkUri: "http://example.com/i",
  }, {
    // The complete left pane root. We should remove all left pane queries
    // locally, even though they're syncable, since the left pane root is
    // known to be non-syncable.
    id: "folderLEFTPR",
    type: "folder",
    parentid: "places",
    title: "",
    children: ["folderLEFTPQ", "folderLEFTPF"],
  }, {
    id: "folderLEFTPQ",
    type: "query",
    title: "Some query",
    bmkUri: "place:folder=SOMETHING",
  }, {
    id: "folderLEFTPF",
    type: "folder",
    title: "All Bookmarks",
    children: ["folderLEFTPC"],
  }, {
    id: "folderLEFTPC",
    type: "query",
    title: "A query under 'All Bookmarks'",
    bmkUri: "place:folder=SOMETHING_ELSE",
  }, {
    // D, J, and G are syncable remotely, but D is non-syncable locally. Since
    // J and G don't exist locally, and are syncable remotely, we'll remove D
    // from the merged structure, and move J and G to unfiled.
    id: "unfiled",
    type: "folder",
    children: ["folderDDDDDD", "bookmarkGGGG"],
  }, {
    id: "folderDDDDDD",
    type: "folder",
    title: "D",
    children: ["bookmarkJJJJ"],
  }, {
    id: "bookmarkJJJJ",
    type: "bookmark",
    title: "J",
    bmkUri: "http://example.com/j",
  }, {
    id: "bookmarkGGGG",
    type: "bookmark",
    title: "G",
    bmkUri: "http://example.com/g",
  }]);

  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let datesAdded = await promiseManyDatesAdded([PlacesUtils.bookmarks.menuGuid,
    PlacesUtils.bookmarks.unfiledGuid, "bookmarkBBBB", "bookmarkJJJJ"]);
  deepEqual(changesToUpload, {
    folderAAAAAA: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderAAAAAA",
        deleted: true,
      },
    },
    folderDDDDDD: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderDDDDDD",
        deleted: true,
      },
    },
    folderLEFTPQ: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderLEFTPQ",
        deleted: true,
      },
    },
    folderLEFTPC: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderLEFTPC",
        deleted: true,
      },
    },
    folderLEFTPR: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderLEFTPR",
        deleted: true,
      },
    },
    folderLEFTPF: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderLEFTPF",
        deleted: true,
      },
    },
    rootHHHHHHHH: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "rootHHHHHHHH",
        deleted: true,
      },
    },
    bookmarkFFFF: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkFFFF",
        deleted: true,
      },
    },
    bookmarkIIII: {
      tombstone: true,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkIIII",
        deleted: true,
      },
    },
    bookmarkBBBB: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkBBBB",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: BookmarksMenuTitle,
        dateAdded: datesAdded.get("bookmarkBBBB"),
        bmkUri: "http://example.com/b",
        title: "B",
      },
    },
    bookmarkJJJJ: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkJJJJ",
        type: "bookmark",
        parentid: "unfiled",
        hasDupe: true,
        parentName: UnfiledBookmarksTitle,
        dateAdded: undefined,
        bmkUri: "http://example.com/j",
        title: "J",
      },
    },
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
        dateAdded: datesAdded.get(PlacesUtils.bookmarks.menuGuid),
        title: BookmarksMenuTitle,
        children: ["bookmarkBBBB"],
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
        title: UnfiledBookmarksTitle,
        children: ["bookmarkJJJJ", "bookmarkGGGG"],
      },
    },
  }, "Should upload new structure and tombstones for non-syncable items");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [{
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "B",
        url: "http://example.com/b",
      }]
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
      children: [{
        guid: "bookmarkJJJJ",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "J",
        url: "http://example.com/j",
      }, {
        guid: "bookmarkGGGG",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "G",
        url: "http://example.com/g",
      }],
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should exclude non-syncable items from new local structure");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// See what happens when a left-pane root and a left-pane query are on the server
add_task(async function test_left_pane_root() {
  let buf = await openMirror("lpr");

  let initialTree = await fetchLocalTree(PlacesUtils.bookmarks.rootGuid);

  // This test is expected to not touch bookmarks at all, and if it did
  // happen to create a new item that's not under our syncable roots, then
  // just checking the result of fetchLocalTree wouldn't pick that up - so
  // as an additional safety check, count how many bookmark rows exist.
  let numRows = await getCountOfBookmarkRows(buf.db);

  // Add a left pane root, a left-pane query and a left-pane folder to the
  // mirror, all correctly parented.
  // Because we can determine this is a complete tree that's outside our
  // syncable trees, we expect none of them to be applied.
  await storeRecords(buf, shuffle([{
    id: "folderLEFTPR",
    type: "folder",
    parentid: "places",
    title: "",
    children: ["folderLEFTPQ", "folderLEFTPF"],
  }, {
    id: "folderLEFTPQ",
    type: "query",
    parentid: "folderLEFTPR",
    title: "Some query",
    bmkUri: "place:folder=SOMETHING",
  }, {
    id: "folderLEFTPF",
    type: "folder",
    parentid: "folderLEFTPR",
    title: "All Bookmarks",
    children: ["folderLEFTPC"],
  }, {
    id: "folderLEFTPC",
    type: "query",
    parentid: "folderLEFTPF",
    title: "A query under 'All Bookmarks'",
    bmkUri: "place:folder=SOMETHING_ELSE",
  }], { needsMerge: true }));

  await buf.apply();

  // should have ignored everything.
  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, initialTree);

  // and a check we didn't write *any* items to the places database, even
  // outside of our user roots.
  Assert.equal(await getCountOfBookmarkRows(buf.db), numRows);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// See what happens when a left-pane query (without the left-pane root) is on
// the server
add_task(async function test_left_pane_query() {
  let buf = await openMirror("lpq");

  let initialTree = await fetchLocalTree(PlacesUtils.bookmarks.rootGuid);

  // This test is expected to not touch bookmarks at all, and if it did
  // happen to create a new item that's not under our syncable roots, then
  // just checking the result of fetchLocalTree wouldn't pick that up - so
  // as an additional safety check, count how many bookmark rows exist.
  let numRows = await getCountOfBookmarkRows(buf.db);

  // Add the left pane root and left-pane folders to the mirror, correctly parented.
  // We should not apply it because we made a policy decision to not apply
  // orphaned queries (bug 1433182)
  await storeRecords(buf, [{
    id: "folderLEFTPQ",
    type: "query",
    parentid: "folderLEFTPR",
    title: "Some query",
    bmkUri: "place:folder=SOMETHING",
  }], { needsMerge: true });

  await buf.apply();

  // should have ignored everything.
  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, initialTree);

  // and further check we didn't apply it as mis-rooted.
  Assert.equal(await getCountOfBookmarkRows(buf.db), numRows);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_partial_cycle() {
  let buf = await openMirror("partial_cycle");

  info("Set up mirror: Menu > A > B > C");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      children: [{
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "B",
        children: [{
          guid: "bookmarkCCCC",
          url: "http://example.com/c",
          title: "C",
        }],
      }],
    }],
  });
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["folderBBBBBB"],
  }, {
    id: "folderBBBBBB",
    type: "folder",
    title: "B",
    children: ["bookmarkCCCC"],
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C",
    bmkUri: "http://example.com/c",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  // Try to create a cycle: move A into B, and B into the menu, but don't upload
  // a record for the menu. B is still a child of A locally. Since we ignore the
  // `parentid`, we'll move (B A) into unfiled.
  info("Make remote changes: A > C");
  await storeRecords(buf, [{
    id: "folderAAAAAA",
    type: "folder",
    title: "A (remote)",
    children: ["bookmarkCCCC"],
  }, {
    id: "folderBBBBBB",
    type: "folder",
    title: "B (remote)",
    children: ["folderAAAAAA"],
  }]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, { updated: [], deleted: [] },
    "Should not mark any local items for upload");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
      children: [{
        guid: "folderBBBBBB",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "B (remote)",
        children: [{
          guid: "folderAAAAAA",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          index: 0,
          title: "A (remote)",
          children: [{
            guid: "bookmarkCCCC",
            type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
            index: 0,
            title: "C",
            url: "http://example.com/c",
          }],
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should move A and B to unfiled");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_complete_cycle() {
  let buf = await openMirror("complete_cycle");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  // This test is order-dependent. We shouldn't recurse infinitely, but,
  // depending on the order of the records, we might ignore the circular
  // subtree because there's nothing linking it back to the rest of the
  // tree.
  info("Make remote changes: Menu > A > B > C > A");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["folderBBBBBB"],
  }, {
    id: "folderBBBBBB",
    type: "folder",
    title: "B",
    children: ["folderCCCCCC"],
  }, {
    id: "folderCCCCCC",
    type: "folder",
    title: "C",
    children: ["folderDDDDDD"],
  }, {
    id: "folderDDDDDD",
    type: "folder",
    title: "D",
    children: ["folderAAAAAA"],
  }]);

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual((await buf.fetchUnmergedGuids()).sort(), ["folderAAAAAA",
    "folderBBBBBB", "folderCCCCCC", "folderDDDDDD"],
    "Should leave items in circular subtree unmerged");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, { updated: [], deleted: [] },
    "Should not mark any local items for upload");

  await assertLocalTree(PlacesUtils.bookmarks.rootGuid, {
    guid: PlacesUtils.bookmarks.rootGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "",
    children: [{
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should not be confused into creating a cycle");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_invalid_guid() {
  let buf = await openMirror("invalid_guid");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA", "bad!guid~", "bookmarkBBBB"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }, {
    // Should be ignored.
    id: "bad!guid~",
    type: "bookmark",
    title: "Bad GUID",
    bmkUri: "http://example.com/bad-guid",
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }]);

  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  deepEqual(changesToUpload, {}, "Should not reupload menu with gaps");

  deepEqual(await buf.fetchRemoteOrphans(), {
    missingChildren: [],
    missingParents: [],
    parentsWithGaps: [PlacesUtils.bookmarks.menuGuid],
  }, "Should report gaps in menu");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
