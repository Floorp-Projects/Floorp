/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Timer.jsm");

// This is tricky to test, because we need to induce a race between a write
// on the main connection, and the merge transaction on the mirror connection.
// We do this by starting a transaction on the main connection, then calling
// `apply`. Both the main and mirror connections use WAL mode, so reading
// from the mirror will succeed, and the mirror transaction will wait on
// the main transaction to finish. We can then change bookmarks, commit the main
// transaction, and check that we abort the merge.
add_task(async function test_bookmark_change_during_sync() {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  let buf = await openMirror("bookmark_change_during_sync");

  info("Set up empty mirror");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote changes");
  await storeRecords(buf, [{
    id: "menu",
    type: "folder",
    children: ["bookmarkAAAA"],
  }, {
    id: "bookmarkAAAA",
    type: "bookmark",
    title: "A",
    bmkUri: "http://example.com/a",
  }]);

  await PlacesUtils.withConnectionWrapper("test_bookmark_change_during_sync", async function(db) {
    info("Open main transaction");
    await db.execute(`BEGIN EXCLUSIVE`);

    // TODO(lina): We should have the mirror emit an event instead of using
    // a timeout.
    info("Wait for mirror to merge");
    let applyPromise = buf.apply();
    await new Promise(resolve => setTimeout(resolve, 5000));

    info("Change local bookmark");
    await db.execute(`
      UPDATE moz_bookmarks SET
        syncChangeCounter = syncChangeCounter + 1
      WHERE guid = :toolbarGuid`,
      { toolbarGuid: PlacesUtils.bookmarks.toolbarGuid });

    await db.execute(`COMMIT`);

    await Assert.rejects(applyPromise, /Local tree changed during merge/,
      "Should fail merge if local tree changes before applying");
  });

  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  let infoForA = await PlacesUtils.bookmarks.fetch("bookmarkAAAA");
  let datesAdded = await promiseManyDatesAdded([
    PlacesUtils.bookmarks.toolbarGuid, "bookmarkAAAA"]);
  deepEqual(changesToUpload, {
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
        title: BookmarksToolbarTitle,
        children: [],
      },
    },
  }, "Should upload flagged toolbar");
  deepEqual(infoForA, {
    guid: "bookmarkAAAA",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "A",
    lastModified: infoForA.lastModified,
    url: infoForA.url,
  }, "Should apply A");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
