/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_duping() {
  let buf = await openMirror("duping");

  info("Set up mirror");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      // Shouldn't dupe to `folderA11111` because its sync status is "NORMAL".
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      children: [{
        // Shouldn't dupe to `bookmarkG111`.
        guid: "bookmarkGGGG",
        url: "http://example.com/g",
        title: "G",
      }],
    }],
  });
  await buf.store(shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkGGGG"],
  }, {
    id: "bookmarkGGGG",
    type: "bookmark",
    title: "G",
    bmkUri: "http://example.com/g",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Insert local dupes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      // Should dupe to `folderB11111`.
      guid: "folderBBBBBB",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "B",
      children: [{
        // Should dupe to `bookmarkC222`.
        guid: "bookmarkC111",
        url: "http://example.com/c",
        title: "C",
      }, {
        // Should dupe to `separatorF11` because the positions are the same.
        guid: "separatorFFF",
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      }],
    }, {
      // Shouldn't dupe to `separatorE11`, because the positions are different.
      guid: "separatorEEE",
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    }, {
      // Shouldn't dupe to `bookmarkC222` because the parents are different.
      guid: "bookmarkCCCC",
      url: "http://example.com/c",
      title: "C",
    }, {
      // Should dupe to `queryD111111`.
      guid: "queryDDDDDDD",
      url: "place:sort=8&maxResults=10",
      title: "Most Visited",
      annos: [{
        name: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
        value: "MostVisited",
      }],
    }],
  });

  // Make sure we still dedupe this even though it doesn't have SYNC_STATUS.NEW
  PlacesTestUtils.setBookmarkSyncFields({
    guid: "folderBBBBBB",
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN
  });

  // Not a candidate for `bookmarkH111` because we didn't dupe `folderAAAAAA`.
  await PlacesUtils.bookmarks.insert({
    parentGuid: "folderAAAAAA",
    guid: "bookmarkHHHH",
    url: "http://example.com/h",
    title: "H",
  });

  info("Make remote changes");
  await buf.store(shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA", "folderB11111", "folderA11111",
               "separatorE11", "queryD111111"],
  }, {
    id: "folderB11111",
    type: "folder",
    title: "B",
    children: ["bookmarkC222", "separatorF11"],
  }, {
    id: "bookmarkC222",
    type: "bookmark",
    bmkUri: "http://example.com/c",
    title: "C",
  }, {
    id: "separatorF11",
    type: "separator",
  }, {
    id: "folderA11111",
    type: "folder",
    title: "A",
    children: ["bookmarkG111"],
  }, {
    id: "bookmarkG111",
    type: "bookmark",
    bmkUri: "http://example.com/g",
    title: "G",
  }, {
    id: "separatorE11",
    type: "separator",
  }, {
    id: "queryD111111",
    type: "query",
    bmkUri: "place:maxResults=10&sort=8",
    title: "Most Visited",
    queryId: "MostVisited",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["bookmarkCCCC", "bookmarkHHHH", "folderAAAAAA", "menu",
              "separatorEEE"],
    deleted: [],
  }, "Should not upload deduped local records");

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
          guid: "bookmarkGGGG",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "G",
          url: "http://example.com/g",
        }, {
          guid: "bookmarkHHHH",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "H",
          url: "http://example.com/h",
        }],
      }, {
        guid: "folderB11111",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 1,
        title: "B",
        children: [{
          guid: "bookmarkC222",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "C",
          url: "http://example.com/c",
        }, {
          guid: "separatorF11",
          type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          index: 1,
          title: "",
        }],
      }, {
        guid: "folderA11111",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 2,
        title: "A",
        children: [{
          guid: "bookmarkG111",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "G",
          url: "http://example.com/g",
        }],
      }, {
        guid: "separatorE11",
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        index: 3,
        title: "",
      }, {
        guid: "queryD111111",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 4,
        title: "Most Visited",
        url: "place:maxResults=10&sort=8",
        annos: [{
          name: PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO,
          flags: 0,
          expires: PlacesUtils.annotations.EXPIRE_NEVER,
          value: "MostVisited",
        }],
      }, {
        guid: "separatorEEE",
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        index: 5,
        title: "",
      }, {
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 6,
        title: "C",
        url: "http://example.com/c",
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
  }, "Should dedupe matching NEW bookmarks");

  ok((await PlacesTestUtils.fetchBookmarkSyncFields(
    "menu________", "folderB11111", "bookmarkC222", "separatorF11",
    "folderA11111", "bookmarkG111", "separatorE11", "queryD111111"))
    .every(info => info.syncStatus == PlacesUtils.bookmarks.SYNC_STATUS.NORMAL));


  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_applying_two_empty_folders_doesnt_smush() {
  let buf = await openMirror("applying_two_empty_folders_doesnt_smush");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await buf.store(shuffle([{
    id: "mobile",
    type: "folder",
    children: ["emptyempty01", "emptyempty02"],
  }, {
    id: "emptyempty01",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty02",
    type: "folder",
    title: "Empty",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: [],
    deleted: [],
  }, "Should not upload records for remote-only value changes");

  await assertLocalTree(PlacesUtils.bookmarks.mobileGuid, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 4,
    title: "mobile",
    children: [{
      guid: "emptyempty01",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "Empty",
    }, {
      guid: "emptyempty02",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: "Empty",
    }],
  }, "Should not smush 1 and 2");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_applying_two_empty_folders_matches_only_one() {
  let buf = await openMirror("applying_two_empty_folders_doesnt_smush");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.mobileGuid,
    children: [{
      guid: "emptyempty02",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Empty",
    }, {
      guid: "emptyemptyL0",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Empty",
    }],
  });

  info("Make remote changes");
  await buf.store(shuffle([{
    id: "mobile",
    type: "folder",
    children: ["emptyempty01", "emptyempty02", "emptyempty03"],
  }, {
    id: "emptyempty01",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty02",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty03",
    type: "folder",
    title: "Empty",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["mobile"],
    deleted: [],
  }, "Should not upload records after applying empty folders");

  await assertLocalTree(PlacesUtils.bookmarks.mobileGuid, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 4,
    title: "mobile",
    children: [{
      guid: "emptyempty01",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "Empty",
    }, {
      guid: "emptyempty02",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: "Empty",
    }, {
      guid: "emptyempty03",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 2,
      title: "Empty",
    }],
  }, "Should apply 1 and dedupe L0 to 3");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Bug 747699.
add_task(async function test_duping_mobile_bookmarks() {
  let buf = await openMirror("duping_mobile_bookmarks");

  info("Set up empty mirror with localized mobile root title");
  let mobileInfo = await PlacesUtils.bookmarks.fetch(
    PlacesUtils.bookmarks.mobileGuid);
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.mobileGuid,
    title: "Favoritos do celular",
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local changes");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkAAA1",
    parentGuid: PlacesUtils.bookmarks.mobileGuid,
    title: "A",
    url: "http://example.com/a",
  });

  info("Make remote changes");
  await buf.store(shuffle([{
    id: "mobile",
    type: "folder",
    children: ["bookmarkAAAA"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["mobile"],
    deleted: [],
  }, "Should not upload records after applying deduped mobile bookmark");

  await assertLocalTree(PlacesUtils.bookmarks.mobileGuid, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 4,
    title: "Favoritos do celular",
    children: [{
      guid: "bookmarkAAAA",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 0,
      title: "A",
      url: "http://example.com/a",
    }],
  }, "Should dedupe A1 to A with different parent title");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  // Restore the original mobile root title.
  await PlacesUtils.bookmarks.update({
    guid: PlacesUtils.bookmarks.mobileGuid,
    title: mobileInfo.title,
  });
  await PlacesSyncUtils.bookmarks.reset();
});
