// Get bookmarks which aren't marked as normally syncing and with no pending
// changes.
async function getBookmarksNotMarkedAsSynced() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(
    `
    SELECT guid, syncStatus, syncChangeCounter FROM moz_bookmarks
    WHERE syncChangeCounter > 1 OR syncStatus != :syncStatus
    ORDER BY guid
    `,
    { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL }
  );
  return rows.map(row => {
    return {
      guid: row.getResultByName("guid"),
      syncStatus: row.getResultByName("syncStatus"),
      syncChangeCounter: row.getResultByName("syncChangeCounter"),
    };
  });
}

add_task(async function test_reconcile_metadata() {
  let buf = await openMirror("test_reconcile_metadata");

  let olderDate = new Date(Date.now() - 100000);
  info("Set up local tree");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        // this folder is going to reconcile exactly
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
        // this folder's existing child isn't on the server (so will be
        // outgoing) and also will take a new child from the server.
        guid: "folderCCCCCC",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "C",
        children: [
          {
            guid: "bookmarkEEEE",
            url: "http://example.com/e",
            title: "E",
          },
        ],
      },
      {
        // This bookmark is going to take the remote title.
        guid: "bookmarkFFFF",
        url: "http://example.com/f",
        title: "f",
        dateAdded: olderDate,
        lastModified: olderDate,
      },
    ],
  });
  // And a single, local-only bookmark in the toolbar.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid: "bookmarkTTTT",
        url: "http://example.com/t",
        title: "in the toolbar",
        dateAdded: olderDate,
        lastModified: olderDate,
      },
    ],
  });
  // Reset to prepare for our reconciled sync.
  await PlacesSyncUtils.bookmarks.reset();
  // setup the mirror.
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["folderAAAAAA", "folderCCCCCC", "bookmarkFFFF"],
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "folderAAAAAA",
        parentid: "menu",
        type: "folder",
        title: "A",
        children: ["bookmarkBBBB"],
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
        id: "folderCCCCCC",
        parentid: "menu",
        type: "folder",
        title: "C",
        children: ["bookmarkDDDD"],
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "bookmarkDDDD",
        parentid: "folderCCCCCC",
        type: "bookmark",
        title: "D",
        bmkUri: "http://example.com/d",
        modified: Date.now() / 1000 - 60,
      },
      {
        id: "bookmarkFFFF",
        parentid: "menu",
        type: "bookmark",
        title: "F",
        bmkUri: "http://example.com/f",
        dateAdded: olderDate,
        modified: Date.now() / 1000 + 60,
      },
      {
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: [],
        index: 1,
      },
      {
        id: "unfiled",
        parentid: "places",
        type: "folder",
        children: [],
        index: 3,
      },
    ])
  );
  info("Applying");
  let changesToUpload = await buf.apply();
  // We need to upload a bookmark and the parent as they didn't exist on the
  // server. Since we always use the local state for roots (bug 1472241), we'll
  // reupload them too.
  let idsToUpload = inspectChangeRecords(changesToUpload);
  deepEqual(
    idsToUpload,
    {
      updated: [
        "bookmarkEEEE",
        "bookmarkTTTT",
        "folderCCCCCC",
        "menu",
        "mobile",
        "toolbar",
        "unfiled",
      ],
      deleted: [],
    },
    "Should upload the 2 local-only bookmarks and their parents"
  );
  // Check it took the remote thing we were expecting.
  Assert.equal((await PlacesUtils.bookmarks.fetch("bookmarkFFFF")).title, "F");
  // Most things should be synced and have no change counter.
  let badGuids = await getBookmarksNotMarkedAsSynced();
  Assert.deepEqual(badGuids, [
    {
      // The bookmark that was only on the server. Still have SYNC_STATUS_NEW
      // as it's yet to be uploaded.
      guid: "bookmarkEEEE",
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      syncChangeCounter: 1,
    },
    {
      // This bookmark is local only so is yet to be uploaded.
      guid: "bookmarkTTTT",
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      syncChangeCounter: 1,
    },
  ]);
});
