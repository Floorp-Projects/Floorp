/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_value_structure_conflict() {
  let buf = await openMirror("value_structure_conflict");

  info("Set up mirror");
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
        guid: "bookmarkCCCC",
        url: "http://example.com/c",
        title: "C",
      }],
    }, {
      guid: "folderDDDDDD",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "D",
      children: [{
        guid: "bookmarkEEEE",
        url: "http://example.com/e",
        title: "E",
      }],
    }],
  });
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA", "folderDDDDDD"],
    modified: Date.now() / 1000 - 60,
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkBBBB", "bookmarkCCCC"],
    modified: Date.now() / 1000 - 60,
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
    modified: Date.now() / 1000 - 60,
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C",
    bmkUri: "http://example.com/c",
    modified: Date.now() / 1000 - 60,
  }, {
    id: "folderDDDDDD",
    type: "folder",
    title: "D",
    children: ["bookmarkEEEE"],
    modified: Date.now() / 1000 - 60,
  }, {
    id: "bookmarkEEEE",
    type: "bookmark",
    title: "E",
    bmkUri: "http://example.com/e",
    modified: Date.now() / 1000 - 60,
  }]), { needsMerge: false });
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
  await storeRecords(buf, [{
    id: "folderDDDDDD",
    type: "folder",
    title: "D (remote)",
    children: ["bookmarkEEEE"],
    modified: Date.now() / 1000 + 60,
  }]);

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: Date.now() / 1000,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["bookmarkBBBB", "folderAAAAAA", "folderDDDDDD"],
    deleted: [],
  }, "Should upload records for merged and new local items");

  let localItemIds = await PlacesUtils.promiseManyItemIds(["folderAAAAAA",
    "bookmarkEEEE", "bookmarkBBBB", "folderDDDDDD"]);
  observer.check([{
    name: "onItemMoved",
    params: { itemId: localItemIds.get("bookmarkEEEE"),
              oldParentId: localItemIds.get("folderDDDDDD"),
              oldIndex: 1, newParentId: localItemIds.get("folderDDDDDD"),
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "bookmarkEEEE",
              oldParentGuid: "folderDDDDDD",
              newParentGuid: "folderDDDDDD",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "http://example.com/e" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("bookmarkBBBB"),
              oldParentId: localItemIds.get("folderDDDDDD"),
              oldIndex: 0, newParentId: localItemIds.get("folderDDDDDD"),
              newIndex: 1, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "bookmarkBBBB",
              oldParentGuid: "folderDDDDDD",
              newParentGuid: "folderDDDDDD",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "http://example.com/b" },
  }, {
    name: "onItemChanged",
    params: { itemId: localItemIds.get("folderDDDDDD"), property: "title",
              isAnnoProperty: false, newValue: "D (remote)",
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              parentId: PlacesUtils.bookmarksMenuFolderId, guid: "folderDDDDDD",
              parentGuid: PlacesUtils.bookmarks.menuGuid, oldValue: "D",
              source: PlacesUtils.bookmarks.SOURCES.SYNC },
  }]);

  await assertLocalTree(PlacesUtils.bookmarks.menuGuid, {
    guid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: BookmarksMenuTitle,
    children: [{
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: "A (local)",
      children: [{
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "C",
        url: "http://example.com/c",
      }],
    }, {
      guid: "folderDDDDDD",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: "D (remote)",
      children: [{
        guid: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "E",
        url: "http://example.com/e",
      }, {
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        title: "B",
        url: "http://example.com/b",
      }],
    }],
  }, "Should reconcile structure and value changes");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_move() {
  let buf = await openMirror("move");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "devFolder___",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Dev",
      children: [{
        guid: "mdnBmk______",
        title: "MDN",
        url: "https://developer.mozilla.org",
      }, {
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        guid: "mozFolder___",
        title: "Mozilla",
        children: [{
          guid: "fxBmk_______",
          title: "Get Firefox!",
          url: "http://getfirefox.com/",
        }, {
          guid: "nightlyBmk__",
          title: "Nightly",
          url: "https://nightly.mozilla.org",
        }],
      }, {
        guid: "wmBmk_______",
        title: "Webmaker",
        url: "https://webmaker.org",
      }],
    }, {
      guid: "bzBmk_______",
      title: "Bugzilla",
      url: "https://bugzilla.mozilla.org",
    }]
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  await storeRecords(buf, shuffle([{
    id: "unfiled",
    type: "folder",
    children: ["mozFolder___"],
  }, {
    id: "toolbar",
    type: "folder",
    children: ["devFolder___"],
  }, {
    id: "devFolder___",
    // Moving to toolbar.
    type: "folder",
    title: "Dev",
    children: ["bzBmk_______", "wmBmk_______"],
  }, {
    // Moving to "Mozilla".
    id: "mdnBmk______",
    type: "bookmark",
    title: "MDN",
    bmkUri: "https://developer.mozilla.org",
  }, {
    // Rearranging children and moving to unfiled.
    id: "mozFolder___",
    type: "folder",
    title: "Mozilla",
    children: ["nightlyBmk__", "mdnBmk______", "fxBmk_______"],
  }, {
    id: "fxBmk_______",
    type: "bookmark",
    title: "Get Firefox!",
    bmkUri: "http://getfirefox.com/",
  }, {
    id: "nightlyBmk__",
    type: "bookmark",
    title: "Nightly",
    bmkUri: "https://nightly.mozilla.org",
  }, {
    id: "wmBmk_______",
    type: "bookmark",
    title: "Webmaker",
    bmkUri: "https://webmaker.org",
  }, {
    id: "bzBmk_______",
    type: "bookmark",
    title: "Bugzilla",
    bmkUri: "https://bugzilla.mozilla.org",
  }]));

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: [],
    deleted: [],
  }, "Should not upload records for remotely moved items");

  let localItemIds = await PlacesUtils.promiseManyItemIds(["devFolder___",
    "mozFolder___", "bzBmk_______", "wmBmk_______", "nightlyBmk__",
    "mdnBmk______", "fxBmk_______"]);
  observer.check([{
    name: "onItemMoved",
    params: { itemId: localItemIds.get("devFolder___"),
              oldParentId: PlacesUtils.bookmarksMenuFolderId,
              oldIndex: 0, newParentId: PlacesUtils.toolbarFolderId,
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_FOLDER,
              guid: "devFolder___",
              oldParentGuid: PlacesUtils.bookmarks.menuGuid,
              newParentGuid: PlacesUtils.bookmarks.toolbarGuid,
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: null },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("mozFolder___"),
              oldParentId: localItemIds.get("devFolder___"),
              oldIndex: 1, newParentId: PlacesUtils.unfiledBookmarksFolderId,
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_FOLDER,
              guid: "mozFolder___",
              oldParentGuid: "devFolder___",
              newParentGuid: PlacesUtils.bookmarks.unfiledGuid,
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: null },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("bzBmk_______"),
              oldParentId: PlacesUtils.bookmarksMenuFolderId,
              oldIndex: 1, newParentId: localItemIds.get("devFolder___"),
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "bzBmk_______",
              oldParentGuid: PlacesUtils.bookmarks.menuGuid,
              newParentGuid: "devFolder___",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "https://bugzilla.mozilla.org/" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("wmBmk_______"),
              oldParentId: localItemIds.get("devFolder___"),
              oldIndex: 2, newParentId: localItemIds.get("devFolder___"),
              newIndex: 1, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "wmBmk_______",
              oldParentGuid: "devFolder___",
              newParentGuid: "devFolder___",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "https://webmaker.org/" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("nightlyBmk__"),
              oldParentId: localItemIds.get("mozFolder___"),
              oldIndex: 1, newParentId: localItemIds.get("mozFolder___"),
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "nightlyBmk__",
              oldParentGuid: "mozFolder___",
              newParentGuid: "mozFolder___",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "https://nightly.mozilla.org/" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("mdnBmk______"),
              oldParentId: localItemIds.get("devFolder___"),
              oldIndex: 0, newParentId: localItemIds.get("mozFolder___"),
              newIndex: 1, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "mdnBmk______",
              oldParentGuid: "devFolder___",
              newParentGuid: "mozFolder___",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "https://developer.mozilla.org/" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("fxBmk_______"),
              oldParentId: localItemIds.get("mozFolder___"),
              oldIndex: 0, newParentId: localItemIds.get("mozFolder___"),
              newIndex: 2, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "fxBmk_______",
              oldParentGuid: "mozFolder___",
              newParentGuid: "mozFolder___",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "http://getfirefox.com/" },
  }]);

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
      children: [{
        guid: "devFolder___",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "Dev",
        children: [{
          guid: "bzBmk_______",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "Bugzilla",
          url: "https://bugzilla.mozilla.org/",
        }, {
          guid: "wmBmk_______",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "Webmaker",
          url: "https://webmaker.org/",
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 3,
      title: UnfiledBookmarksTitle,
      children: [{
        guid: "mozFolder___",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        index: 0,
        title: "Mozilla",
        children: [{
          guid: "nightlyBmk__",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "Nightly",
          url: "https://nightly.mozilla.org/",
        }, {
          guid: "mdnBmk______",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "MDN",
          url: "https://developer.mozilla.org/",
        }, {
          guid: "fxBmk_______",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "Get Firefox!",
          url: "http://getfirefox.com/",
        }],
      }],
    }, {
      guid: PlacesUtils.bookmarks.mobileGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 4,
      title: MobileBookmarksTitle,
    }],
  }, "Should move and reorder bookmarks to match remote");

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
    children: [{
      guid: "folderAAAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "A",
      children: [{
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
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
    children: ["bookmarkBBBB"],
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes: Menu > (A (B > C))");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["folderAAAAAA", "folderCCCCCC"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
  }, {
    id: "folderCCCCCC",
    type: "folder",
    title: "C",
    children: ["bookmarkBBBB"],
  }]);

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: [],
    deleted: [],
  }, "Should not upload records for remote-only structure changes");

  let localItemIds = await PlacesUtils.promiseManyItemIds(["folderCCCCCC",
    "bookmarkBBBB", "folderAAAAAA"]);
  observer.check([{
    name: "onItemAdded",
    params: { itemId: localItemIds.get("folderCCCCCC"),
              parentId: PlacesUtils.bookmarksMenuFolderId, index: 1,
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              urlHref: null, title: "C",
              guid: "folderCCCCCC",
              parentGuid: PlacesUtils.bookmarks.menuGuid,
              source: PlacesUtils.bookmarks.SOURCES.SYNC },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("bookmarkBBBB"),
              oldParentId: localItemIds.get("folderAAAAAA"),
              oldIndex: 0, newParentId: localItemIds.get("folderCCCCCC"),
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "bookmarkBBBB",
              oldParentGuid: "folderAAAAAA",
              newParentGuid: "folderCCCCCC",
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "http://example.com/b" },
  }]);

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
    }, {
      guid: "folderCCCCCC",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: "C",
      children: [{
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "B",
        url: "http://example.com/b",
      }],
    }],
  }, "Should set up local structure correctly");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_complex_move_with_additions() {
  let mergeTelemetryEvents = [];
  let buf = await openMirror("complex_move_with_additions", {
    recordTelemetryEvent(object, method, value, extra) {
      equal(object, "mirror", "Wrong object for telemetry event");
      if (method == "merge") {
        mergeTelemetryEvents.push({ value, extra });
      }
    },
  });

  info("Set up mirror: Menu > A > (B C)");
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
        guid: "bookmarkCCCC",
        url: "http://example.com/c",
        title: "C",
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
    children: ["bookmarkBBBB", "bookmarkCCCC"],
  }, {
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b",
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C",
    bmkUri: "http://example.com/c",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make local change: Menu > A > (B C D)");
  await PlacesUtils.bookmarks.insert({
    guid: "bookmarkDDDD",
    parentGuid: "folderAAAAAA",
    title: "D (local)",
    url: "http://example.com/d-local",
  });

  info("Make remote change: ((Menu > C) (Toolbar > A > (B E)))");
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["bookmarkCCCC"],
  }, {
    id: "toolbar",
    type: "folder",
    children: ["folderAAAAAA"],
  }, {
    id: "folderAAAAAA",
    type: "folder",
    title: "A",
    children: ["bookmarkBBBB", "bookmarkEEEE"],
  }, {
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C",
    bmkUri: "http://example.com/c",
  }, {
    id: "bookmarkEEEE",
    type: "bookmark",
    title: "E",
    bmkUri: "http://example.com/e",
  }]));

  info("Apply remote");
  let observer = expectBookmarkChangeNotifications();
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
  deepEqual(mergeTelemetryEvents, [{
    value: "structure",
    extra: { new: 2, remoteRevives: 0, localDeletes: 0, localRevives: 0,
             remoteDeletes: 0 },
  }], "Should record telemetry with structure change counts");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["bookmarkDDDD", "folderAAAAAA"],
    deleted: [],
  }, "Should upload new records for (A D)");

  let localItemIds = await PlacesUtils.promiseManyItemIds(["bookmarkEEEE",
    "folderAAAAAA", "bookmarkCCCC"]);
  observer.check([{
    name: "onItemAdded",
    params: { itemId: localItemIds.get("bookmarkEEEE"),
              parentId: localItemIds.get("folderAAAAAA"), index: 1,
              type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              urlHref: "http://example.com/e", title: "E",
              guid: "bookmarkEEEE",
              parentGuid: "folderAAAAAA",
              source: PlacesUtils.bookmarks.SOURCES.SYNC },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("bookmarkCCCC"),
              oldParentId: localItemIds.get("folderAAAAAA"),
              oldIndex: 1, newParentId: PlacesUtils.bookmarksMenuFolderId,
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
              guid: "bookmarkCCCC",
              oldParentGuid: "folderAAAAAA",
              newParentGuid: PlacesUtils.bookmarks.menuGuid,
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: "http://example.com/c" },
  }, {
    name: "onItemMoved",
    params: { itemId: localItemIds.get("folderAAAAAA"),
              oldParentId: PlacesUtils.bookmarksMenuFolderId,
              oldIndex: 0, newParentId: PlacesUtils.toolbarFolderId,
              newIndex: 0, type: PlacesUtils.bookmarks.TYPE_FOLDER,
              guid: "folderAAAAAA",
              oldParentGuid: PlacesUtils.bookmarks.menuGuid,
              newParentGuid: PlacesUtils.bookmarks.toolbarGuid,
              source: PlacesUtils.bookmarks.SOURCES.SYNC,
              uri: null },
  }]);

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
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        title: "C",
        url: "http://example.com/c",
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
      children: [{
        // We can guarantee child order (B E D), since we always walk remote
        // children first, and the remote folder A record is newer than the
        // local folder. If the local folder were newer, the order would be
        // (D B E).
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
        }, {
          guid: "bookmarkDDDD",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 2,
          title: "D (local)",
          url: "http://example.com/d-local",
        }],
      }],
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
  }, "Should take remote order and preserve local children");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_reorder_and_insert() {
  let buf = await openMirror("reorder_and_insert");

  info("Set up mirror");
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
      guid: "bookmarkCCCC",
      url: "http://example.com/c",
      title: "C",
    }],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      guid: "bookmarkDDDD",
      url: "http://example.com/d",
      title: "D",
    }, {
      guid: "bookmarkEEEE",
      url: "http://example.com/e",
      title: "E",
    }, {
      guid: "bookmarkFFFF",
      url: "http://example.com/f",
      title: "F",
    }],
  });
  await storeRecords(buf, shuffle([{
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA", "bookmarkBBBB", "bookmarkCCCC"],
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
    id: "bookmarkCCCC",
    type: "bookmark",
    title: "C",
    bmkUri: "http://example.com/c",
  }, {
    id: "toolbar",
    type: "folder",
    children: ["bookmarkDDDD", "bookmarkEEEE", "bookmarkFFFF"],
  }, {
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D",
    bmkUri: "http://example.com/d",
  }, {
    id: "bookmarkEEEE",
    type: "bookmark",
    title: "E",
    bmkUri: "http://example.com/e",
  }, {
    id: "bookmarkFFFF",
    type: "bookmark",
    title: "F",
    bmkUri: "http://example.com/f",
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  let now = Date.now();

  info("Make local changes: Reorder Menu, Toolbar > (G H)");
  await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.menuGuid, [
    "bookmarkCCCC", "bookmarkAAAA", "bookmarkBBBB"]);
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      guid: "bookmarkGGGG",
      url: "http://example.com/g",
      title: "G",
      dateAdded: new Date(now),
      lastModified: new Date(now),
    }, {
      guid: "bookmarkHHHH",
      url: "http://example.com/h",
      title: "H",
      dateAdded: new Date(now),
      lastModified: new Date(now),
    }],
  });

  info("Make remote changes: Reorder Toolbar, Menu > (I J)");
  await storeRecords(buf, shuffle([{
    // The server has a newer toolbar, so we should use the remote order (F D E)
    // as the base, then append (G H).
    id: "toolbar",
    type: "folder",
    children: ["bookmarkFFFF", "bookmarkDDDD", "bookmarkEEEE"],
    modified: now / 1000 + 5,
  }, {
    // The server has an older menu, so we should use the local order (C A B)
    // as the base, then append (I J).
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA", "bookmarkBBBB", "bookmarkCCCC", "bookmarkIIII",
               "bookmarkJJJJ"],
    modified: now / 1000 - 5,
  }, {
    id: "bookmarkIIII",
    type: "bookmark",
    title: "I",
    bmkUri: "http://example.com/i",
  }, {
    id: "bookmarkJJJJ",
    type: "bookmark",
    title: "J",
    bmkUri: "http://example.com/j",
  }]));

  info("Apply remote");
  let changesToUpload = await buf.apply({
    remoteTimeSeconds: now / 1000,
    localTimeSeconds: now / 1000,
  });
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(idsToUpload, {
    updated: ["bookmarkGGGG", "bookmarkHHHH", "menu", "toolbar"],
    deleted: [],
  }, "Should upload records for merged and new local items");

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
        guid: "bookmarkCCCC",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        url: "http://example.com/c",
        title: "C",
      }, {
        guid: "bookmarkAAAA",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        url: "http://example.com/a",
        title: "A",
      }, {
        guid: "bookmarkBBBB",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 2,
        url: "http://example.com/b",
        title: "B",
      }, {
        guid: "bookmarkIIII",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 3,
        url: "http://example.com/i",
        title: "I",
      }, {
        guid: "bookmarkJJJJ",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 4,
        url: "http://example.com/j",
        title: "J",
      }],
    }, {
      guid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 1,
      title: BookmarksToolbarTitle,
      children: [{
        guid: "bookmarkFFFF",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 0,
        url: "http://example.com/f",
        title: "F",
      }, {
        guid: "bookmarkDDDD",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 1,
        url: "http://example.com/d",
        title: "D",
      }, {
        guid: "bookmarkEEEE",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 2,
        url: "http://example.com/e",
        title: "E",
      }, {
        guid: "bookmarkGGGG",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 3,
        url: "http://example.com/g",
        title: "G",
      }, {
        guid: "bookmarkHHHH",
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        index: 4,
        url: "http://example.com/h",
        title: "H",
      }],
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
  }, "Should use timestamps to decide base folder order");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
