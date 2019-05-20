/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_duping_local_newer() {
  let mergeTelemetryEvents = [];
  let buf = await openMirror("duping_local_newer", {
    recordTelemetryEvent(object, method, value, extra) {
      equal(object, "mirror", "Wrong object for telemetry event");
      if (method == "merge") {
        mergeTelemetryEvents.push({ value, extra });
      }
    },
  });
  let localModified = new Date();

  info("Start with empty local and mirror with merged items");
  await storeRecords(buf, [{
    id: "menu",
    parentid: "places",
    type: "folder",
    children: ["bookmarkAAA5"],
  }, {
    id: "bookmarkAAA5",
    parentid: "menu",
    type: "bookmark",
    bmkUri: "http://example.com/a",
    title: "A",
  }], { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Add newer local dupes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "bookmarkAAA1",
      title: "A",
      url: "http://example.com/a",
      dateAdded: localModified,
      lastModified: localModified,
    }, {
      guid: "bookmarkAAA2",
      title: "A",
      url: "http://example.com/a",
      dateAdded: localModified,
      lastModified: localModified,
    }, {
      guid: "bookmarkAAA3",
      title: "A",
      url: "http://example.com/a",
      dateAdded: localModified,
      lastModified: localModified,
    }],
  });

  info("Add older remote dupes");
  await storeRecords(buf, [{
    id: "menu",
    parentid: "places",
    type: "folder",
    children: ["bookmarkAAAA", "bookmarkAAA4", "bookmarkAAA5"],
    modified: localModified / 1000 - 5,
  }, {
    id: "bookmarkAAAA",
    parentid: "menu",
    type: "bookmark",
    bmkUri: "http://example.com/a",
    title: "A",
    keyword: "kw",
    tags: ["remote", "tags"],
    modified: localModified / 1000 - 5,
  }, {
    id: "bookmarkAAA4",
    parentid: "menu",
    type: "bookmark",
    bmkUri: "http://example.com/a",
    title: "A",
    modified: localModified / 1000 - 5,
  }]);

  info("Apply remote");
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: localModified / 1000,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
  deepEqual(mergeTelemetryEvents, [{
    value: "structure",
    extra: { remoteRevives: 0, localDeletes: 0, localRevives: 0,
             remoteDeletes: 0 },
  }], "Should record telemetry with dupe counts");

  let menuInfo = await PlacesUtils.bookmarks.fetch(
    PlacesUtils.bookmarks.menuGuid);
  deepEqual(changesToUpload, {
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
        dateAdded: menuInfo.dateAdded.getTime(),
        title: menuInfo.title,
        children: ["bookmarkAAAA", "bookmarkAAA4", "bookmarkAAA3",
                   "bookmarkAAA5"],
      },
    },
    // Note that we always reupload the deduped local item, because content
    // matching doesn't account for attributes like keywords, synced annos, or
    // tags.
    bookmarkAAAA: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkAAAA",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: menuInfo.title,
        dateAdded: localModified.getTime(),
        title: "A",
        bmkUri: "http://example.com/a",
      },
    },
    // Unchanged from local.
    bookmarkAAA4: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkAAA4",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: menuInfo.title,
        dateAdded: localModified.getTime(),
        title: "A",
        bmkUri: "http://example.com/a",
      },
    },
    bookmarkAAA3: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkAAA3",
        type: "bookmark",
        parentid: "menu",
        hasDupe: true,
        parentName: menuInfo.title,
        dateAdded: localModified.getTime(),
        title: "A",
        bmkUri: "http://example.com/a",
      },
    },
  }, "Should uploaded newer deduped local items");

  await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
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
    }, {
      guid: "bookmarkAAA4",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 1,
      title: "A",
      url: "http://example.com/a",
    }, {
      guid: "bookmarkAAA3",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 2,
      title: "A",
      url: "http://example.com/a",
    }, {
      guid: "bookmarkAAA5",
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      index: 3,
      title: "A",
      url: "http://example.com/a",
    }],
  }, "Should dedupe local multiple bookmarks with similar contents");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_duping_remote_newer() {
  let buf = await openMirror("duping_remote_new");
  let localModified = new Date();

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
  await storeRecords(buf, shuffle([{
    id: "menu",
    parentid: "places",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    parentid: "menu",
    type: "folder",
    title: "A",
    children: ["bookmarkGGGG"],
  }, {
    id: "bookmarkGGGG",
    parentid: "folderAAAAAA",
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
      dateAdded: localModified,
      lastModified: localModified,
      children: [{
        // Should dupe to `bookmarkC222`.
        guid: "bookmarkC111",
        url: "http://example.com/c",
        title: "C",
        dateAdded: localModified,
        lastModified: localModified,
      }, {
        // Should dupe to `separatorF11` because the positions are the same.
        guid: "separatorFFF",
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        dateAdded: localModified,
        lastModified: localModified,
      }],
    }, {
      // Shouldn't dupe to `separatorE11`, because the positions are different.
      guid: "separatorEEE",
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      dateAdded: localModified,
      lastModified: localModified,
    }, {
      // Shouldn't dupe to `bookmarkC222` because the parents are different.
      guid: "bookmarkCCCC",
      url: "http://example.com/c",
      title: "C",
      dateAdded: localModified,
      lastModified: localModified,
    }, {
      // Should dupe to `queryD111111`.
      guid: "queryDDDDDDD",
      url: "place:maxResults=10&sort=8",
      title: "Most Visited",
      dateAdded: localModified,
      lastModified: localModified,
    }],
  });

  // Make sure we still dedupe this even though it doesn't have SYNC_STATUS.NEW
  PlacesTestUtils.setBookmarkSyncFields({
    guid: "folderBBBBBB",
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
  });

  // Not a candidate for `bookmarkH111` because we didn't dupe `folderAAAAAA`.
  await PlacesUtils.bookmarks.insert({
    parentGuid: "folderAAAAAA",
    guid: "bookmarkHHHH",
    url: "http://example.com/h",
    title: "H",
    dateAdded: localModified,
    lastModified: localModified,
  });

  info("Make remote changes");
  await storeRecords(buf, shuffle([{
    id: "menu",
    parentid: "places",
    type: "folder",
    children: ["folderAAAAAA", "folderB11111", "folderA11111",
               "separatorE11", "queryD111111"],
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "folderB11111",
    parentid: "menu",
    type: "folder",
    title: "B",
    children: ["bookmarkC222", "separatorF11"],
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "bookmarkC222",
    parentid: "folderB11111",
    type: "bookmark",
    bmkUri: "http://example.com/c",
    title: "C",
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "separatorF11",
    parentid: "folderB11111",
    type: "separator",
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "folderA11111",
    parentid: "menu",
    type: "folder",
    title: "A",
    children: ["bookmarkG111"],
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "bookmarkG111",
    parentid: "folderA11111",
    type: "bookmark",
    bmkUri: "http://example.com/g",
    title: "G",
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "separatorE11",
    parentid: "menu",
    type: "separator",
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }, {
    id: "queryD111111",
    parentid: "menu",
    type: "query",
    bmkUri: "place:maxResults=10&sort=8",
    title: "Most Visited",
    dateAdded: localModified.getTime(),
    modified: localModified / 1000 + 5,
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: localModified / 1000,
  });
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

add_task(async function test_duping_both() {
  let buf = await openMirror("duping_both");
  let now = Date.now();

  info("Start with empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Add local dupes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      // `folderAAAAA1` is older than `folderAAAAAA`, but we should still flag
      // it for upload because it has a new structure (`bookmarkCCCC`).
      guid: "folderAAAAA1",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      dateAdded: new Date(now - 10000),
      lastModified: new Date(now - 5000),
      children: [{
        // Shouldn't upload, since `bookmarkBBBB` is newer.
        guid: "bookmarkBBB1",
        title: "B",
        url: "http://example.com/b",
        dateAdded: new Date(now - 10000),
        lastModified: new Date(now - 5000),
      }, {
        // Should upload, since `bookmarkCCCC` doesn't exist on the server and
        // has no content matches.
        guid: "bookmarkCCCC",
        title: "C",
        url: "http://example.com/c",
        dateAdded: new Date(now - 10000),
        lastModified: new Date(now - 5000),
      }],
    }, {
      // `folderDDDDD1` should keep complete local structure, but we'll still
      // flag it for reupload because it's newer than `folderDDDDDD`.
      guid: "folderDDDDD1",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "D",
      dateAdded: new Date(now - 10000),
      lastModified: new Date(now + 5000),
      children: [{
        guid: "bookmarkEEE1",
        title: "E",
        url: "http://example.com/e",
        dateAdded: new Date(now - 10000),
        lastModified: new Date(now - 5000),
      }],
    }, {
      // `folderFFFFF1` should keep complete remote value and structure, so
      // we shouldn't upload it or its children.
      guid: "folderFFFFF1",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "F",
      dateAdded: new Date(now - 10000),
      lastModified: new Date(now - 5000),
      children: [{
        guid: "bookmarkGGG1",
        title: "G",
        url: "http://example.com/g",
        dateAdded: new Date(now - 10000),
        lastModified: new Date(now - 5000),
      }],
    }],
  });

  info("Add remote dupes");
  await storeRecords(buf, [{
    id: "menu",
    parentid: "places",
    type: "folder",
    children: ["folderAAAAAA", "folderDDDDDD", "folderFFFFFF"],
  }, {
    id: "folderAAAAAA",
    parentid: "menu",
    type: "folder",
    title: "A",
    dateAdded: now - 10000,
    modified: now / 1000 + 5,
    children: ["bookmarkBBBB"],
  }, {
    id: "bookmarkBBBB",
    parentid: "folderAAAAAA",
    type: "bookmark",
    bmkUri: "http://example.com/b",
    title: "B",
    dateAdded: now - 10000,
    modified: now / 1000 + 5,
  }, {
    id: "folderDDDDDD",
    parentid: "menu",
    type: "folder",
    title: "D",
    dateAdded: now - 10000,
    modified: now / 1000 - 5,
    children: ["bookmarkEEEE"],
  }, {
    id: "bookmarkEEEE",
    parentid: "folderDDDDDD",
    type: "bookmark",
    bmkUri: "http://example.com/e",
    title: "E",
    dateAdded: now - 10000,
    modified: now / 1000 + 5,
  }, {
    id: "folderFFFFFF",
    parentid: "menu",
    type: "folder",
    title: "F",
    dateAdded: now - 10000,
    modified: now / 1000 + 5,
    children: ["bookmarkGGGG", "bookmarkHHHH"],
  }, {
    id: "bookmarkGGGG",
    parentid: "folderFFFFFF",
    type: "bookmark",
    bmkUri: "http://example.com/g",
    title: "G",
    dateAdded: now - 10000,
    modified: now / 1000 - 5,
  }, {
    id: "bookmarkHHHH",
    parentid: "folderFFFFFF",
    type: "bookmark",
    bmkUri: "http://example.com/h",
    title: "H",
    dateAdded: now - 10000,
    modified: now / 1000 + 5,
  }]);

  info("Apply remote");
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: now / 1000,
  });

  let menuInfo = await PlacesUtils.bookmarks.fetch(
    PlacesUtils.bookmarks.menuGuid);
  deepEqual(changesToUpload, {
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
        dateAdded: menuInfo.dateAdded.getTime(),
        title: menuInfo.title,
        children: ["folderAAAAAA", "folderDDDDDD", "folderFFFFFF"],
      },
    },
    folderAAAAAA: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderAAAAAA",
        type: "folder",
        parentid: "menu",
        hasDupe: true,
        parentName: menuInfo.title,
        dateAdded: now - 10000,
        title: "A",
        children: ["bookmarkBBBB", "bookmarkCCCC"],
      },
    },
    bookmarkCCCC: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "bookmarkCCCC",
        type: "bookmark",
        parentid: "folderAAAAAA",
        hasDupe: true,
        parentName: "A",
        dateAdded: now - 10000,
        title: "C",
        bmkUri: "http://example.com/c",
      },
    },
    folderDDDDDD: {
      tombstone: false,
      counter: 1,
      synced: false,
      cleartext: {
        id: "folderDDDDDD",
        type: "folder",
        parentid: "menu",
        hasDupe: true,
        parentName: menuInfo.title,
        dateAdded: now - 10000,
        title: "D",
        children: ["bookmarkEEEE"],
      },
    },
  }, "Should upload new and newer locally deduped items");

  await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
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
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "C",
        url: "http://example.com/c",
      }],
    }, {
      guid: "folderDDDDDD",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: "D",
      children: [{
        guid: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "E",
        url: "http://example.com/e",
      }],
    }, {
      guid: "folderFFFFFF",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 2,
      title: "F",
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
    }],
  }, "Should change local GUIDs for mixed older and newer items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_applying_two_empty_folders_doesnt_smush() {
  let buf = await openMirror("applying_two_empty_folders_doesnt_smush");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(buf, shuffle([{
    id: "mobile",
    parentid: "places",
    type: "folder",
    children: ["emptyempty01", "emptyempty02"],
  }, {
    id: "emptyempty01",
    parentid: "mobile",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty02",
    parentid: "mobile",
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
  await storeRecords(buf, shuffle([{
    id: "mobile",
    parentid: "places",
    type: "folder",
    children: ["emptyempty01", "emptyempty02", "emptyempty03"],
  }, {
    id: "emptyempty01",
    parentid: "mobile",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty02",
    parentid: "mobile",
    type: "folder",
    title: "Empty",
  }, {
    id: "emptyempty03",
    parentid: "mobile",
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
  await storeRecords(buf, shuffle([{
    id: "mobile",
    parentid: "places",
    type: "folder",
    children: ["bookmarkAAAA"],
  }, {
    id: "bookmarkAAAA",
    parentid: "mobile",
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
