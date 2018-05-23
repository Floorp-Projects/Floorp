/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_inconsistencies() {
  let buf = await openMirror("inconsistencies");

  info("Set up local tree");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    source: PlacesUtils.bookmarks.SOURCE_SYNC,
    children: [{
      // NORMAL bookmark that exists in the mirror; not an inconsistency.
      guid: "bookmarkAAAA",
      title: "A",
      url: "http://example.com/a",
    }, {
      // NORMAL bookmark that doesn't exist in the mirror at all.
      guid: "bookmarkBBBB",
      title: "B",
      url: "http://example.com/b",
    }, {
      // NORMAL bookmark with a tombstone that doesn't exist in the mirror.
      guid: "bookmarkCCCC",
      title: "C",
      url: "http://example.com/c",
    }, {
      // NORMAL bookmark with a tombstone that exists in the mirror; not an
      // inconsistency.
      guid: "bookmarkDDDD",
      title: "D",
      url: "http://example.com/d",
    }],
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      // NEW bookmark that doesn't exist in the mirror; not an inconsistency.
      guid: "bookmarkEEEE",
      title: "E",
      url: "http://example.com/e",
    }, {
      // NEW bookmark that exists in the mirror.
      guid: "bookmarkFFFF",
      title: "F",
      url: "http://example.com/f",
    }],
  });
  await PlacesUtils.bookmarks.remove("bookmarkCCCC");
  await PlacesUtils.bookmarks.remove("bookmarkDDDD");

  deepEqual(await buf.fetchSyncStatusMismatches(), {
    missingLocal: [],
    missingRemote: [],
    wrongSyncStatus: [],
  }, "Should not report inconsistencies with empty mirror");

  info("Set up mirror");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA", "bookmarkDDDD"],
  }, {
    id: "toolbar",
    type: "folder",
    children: ["bookmarkFFFF"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }, {
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D",
    bmkUri: "http://example.com/d",
  }, {
    id: "bookmarkFFFF",
    type: "bookmark",
    title: "F",
    bmkUri: "http://example.com/f",
  }, {
    // Merged bookmark that doesn't exist locally.
    id: "bookmarkGGGG",
    type: "bookmark",
    title: "G",
    bmkUri: "http://example.com/g",
  }], { needsMerge: false });
  await storeRecords(buf, [{
    id: "bookmarkHHHH",
    type: "bookmark",
    title: "H",
    bmkUri: "http://example.com/h",
  }, {
    id: "bookmarkIIII",
    deleted: true,
  }]);

  let { missingLocal, missingRemote, wrongSyncStatus } =
    await buf.fetchSyncStatusMismatches();
  deepEqual(missingLocal, ["bookmarkGGGG"],
    "Should report merged remote items that don't exist locally");
  deepEqual(missingRemote.sort(), ["bookmarkBBBB", "bookmarkCCCC"],
    "Should report NORMAL local items that don't exist remotely");
  deepEqual(wrongSyncStatus.sort(), [PlacesUtils.bookmarks.menuGuid,
    PlacesUtils.bookmarks.toolbarGuid, PlacesUtils.bookmarks.unfiledGuid,
    PlacesUtils.bookmarks.mobileGuid, "bookmarkFFFF"].sort(),
    "Should report remote items with wrong local sync status");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
