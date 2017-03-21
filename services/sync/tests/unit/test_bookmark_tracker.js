/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesUtils.jsm");
const {
  // `fetchGuidsWithAnno` isn't exported, but we can still access it here via a
  // backstage pass.
  fetchGuidsWithAnno,
} = Cu.import("resource://gre/modules/PlacesSyncUtils.jsm", {});
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/PlacesTestUtils.jsm");
Cu.import("resource:///modules/PlacesUIUtils.jsm");

Service.engineManager.register(BookmarksEngine);
var engine = Service.engineManager.get("bookmarks");
var store  = engine._store;
var tracker = engine._tracker;

store.wipe();
tracker.persistChangedIDs = false;

const DAY_IN_MS = 24 * 60 * 60 * 1000;

// Test helpers.
async function verifyTrackerEmpty() {
  let changes = await tracker.promiseChangedIDs();
  deepEqual(changes, {});
  equal(tracker.score, 0);
}

async function resetTracker() {
  await PlacesTestUtils.markBookmarksAsSynced();
  tracker.resetScore();
}

async function cleanup() {
  engine.lastSync = 0;
  engine._needWeakReupload.clear()
  store.wipe();
  await resetTracker();
  await stopTracking();
}

// startTracking is a signal that the test wants to notice things that happen
// after this is called (ie, things already tracked should be discarded.)
async function startTracking() {
  Svc.Obs.notify("weave:engine:start-tracking");
  await PlacesTestUtils.markBookmarksAsSynced();
}

async function stopTracking() {
  Svc.Obs.notify("weave:engine:stop-tracking");
}

async function verifyTrackedItems(tracked) {
  let changedIDs = await tracker.promiseChangedIDs();
  let trackedIDs = new Set(Object.keys(changedIDs));
  for (let guid of tracked) {
    ok(guid in changedIDs, `${guid} should be tracked`);
    ok(changedIDs[guid].modified > 0, `${guid} should have a modified time`);
    ok(changedIDs[guid].counter >= -1, `${guid} should have a change counter`);
    trackedIDs.delete(guid);
  }
  equal(trackedIDs.size, 0, `Unhandled tracked IDs: ${
    JSON.stringify(Array.from(trackedIDs))}`);
}

async function verifyTrackedCount(expected) {
  let changedIDs = await tracker.promiseChangedIDs();
  do_check_attribute_count(changedIDs, expected);
}

// A debugging helper that dumps the full bookmarks tree.
async function dumpBookmarks() {
  let columns = ["id", "title", "guid", "syncStatus", "syncChangeCounter", "position"];
  return PlacesUtils.promiseDBConnection().then(connection => {
    let all = [];
    return connection.executeCached(`SELECT ${columns.join(", ")} FROM moz_bookmarks;`,
                                    {},
                                    row => {
                                      let repr = {};
                                      for (let column of columns) {
                                        repr[column] = row.getResultByName(column);
                                      }
                                      all.push(repr);
                                    }
    ).then(() => {
      dump("All bookmarks:\n");
      dump(JSON.stringify(all, undefined, 2));
    });
  })
}

async function insertBookmarksToMigrate() {
  await PlacesUtils.bookmarks.insert({
    guid: "0gtWTOgYcoJD",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://mozilla.org",
  });
  let fxBmk = await PlacesUtils.bookmarks.insert({
    guid: "0dbpnMdxKxfg",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://getfirefox.com",
  });
  let tbBmk = await PlacesUtils.bookmarks.insert({
    guid: "r5ouWdPB3l28",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://getthunderbird.com",
  });
  await PlacesUtils.bookmarks.insert({
    guid: "YK5Bdq5MIqL6",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://bugzilla.mozilla.org",
  });
  let exampleBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com",
  });

  await PlacesTestUtils.setBookmarkSyncFields({
    guid: fxBmk.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  }, {
    guid: tbBmk.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.UNKNOWN,
  }, {
    guid: exampleBmk.guid,
    syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
  });

  await PlacesUtils.bookmarks.remove(exampleBmk.guid);
}

// `PlacesUtils.annotations.setItemAnnotation` prevents us from setting
// annotations on nonexistent items, so this test helper writes to the DB
// directly.
function setAnnoUnchecked(itemId, name, value, type) {
  return PlacesUtils.withConnectionWrapper(
    "test_bookmark_tracker: setItemAnnoUnchecked", async function(db) {
      await db.executeCached(`
        INSERT OR IGNORE INTO moz_anno_attributes (name)
        VALUES (:name)`,
        { name });

      let annoIds = await db.executeCached(`
        SELECT a.id, a.dateAdded
        FROM moz_items_annos a WHERE a.item_id = :itemId`,
        { itemId });

      let annoId;
      let dateAdded;
      let lastModified = PlacesUtils.toPRTime(Date.now());

      if (annoIds.length) {
        annoId = annoIds[0].getResultByName("id");
        dateAdded = annoIds[0].getResultByName("dateAdded");
      } else {
        annoId = null;
        dateAdded = lastModified;
      }

      await db.executeCached(`
        INSERT OR REPLACE INTO moz_items_annos (id, item_id, anno_attribute_id,
          content, flags, expiration, type, dateAdded, lastModified)
        VALUES (:annoId, :itemId, (SELECT id FROM moz_anno_attributes
                                   WHERE name = :name),
                :value, 0, :expiration, :type, :dateAdded, :lastModified)`,
        { annoId, itemId, name, value, type,
          expiration: PlacesUtils.annotations.EXPIRE_NEVER,
          dateAdded, lastModified });
    }
  );
}

add_task(async function test_leftPaneFolder() {
  _("Ensure we never track left pane roots");

  try {
    await startTracking();

    // Creates the organizer queries as a side effect.
    let leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
    _(`Left pane root ID: ${leftPaneId}`);

    {
      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {}, "New left pane queries should not be tracked");
      do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    }

    _("Reset synced bookmarks to simulate a disconnect");
    await PlacesSyncUtils.bookmarks.reset();

    {
      let changes = await tracker.promiseChangedIDs();
      deepEqual(Object.keys(changes).sort(), ["menu", "mobile", "toolbar", "unfiled"],
        "Left pane queries should not be tracked after reset");
      do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
      await PlacesTestUtils.markBookmarksAsSynced();
    }

    // The following tests corrupt the left pane queries in different ways.
    // `PlacesUIUtils.maybeRebuildLeftPane` will rebuild the entire root, but
    // none of those changes should be tracked by Sync.

    _("Annotate unrelated folder as left pane root");
    {
      let folder = await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: "Fake left pane root",
      });
      let folderId = await PlacesUtils.promiseItemId(folder.guid);
      await setAnnoUnchecked(folderId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO, 0,
                             PlacesUtils.annotations.TYPE_INT32);

      leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
      _(`Left pane root ID after deleting unrelated folder: ${leftPaneId}`);

      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {},
        "Should not track left pane items after deleting unrelated folder");
    }

    _("Corrupt organizer left pane version");
    {
      await setAnnoUnchecked(leftPaneId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO,
                             -1, PlacesUtils.annotations.TYPE_INT32);

      leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
      _(`Left pane root ID after restoring version: ${leftPaneId}`);

      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {},
        "Should not track left pane items after restoring version");
    }

    _("Set left pane anno on nonexistent item");
    {
      await setAnnoUnchecked(999, PlacesUIUtils.ORGANIZER_QUERY_ANNO,
                             "Tags", PlacesUtils.annotations.TYPE_STRING);

      leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
      _(`Left pane root ID after detecting nonexistent item: ${leftPaneId}`);

      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {},
        "Should not track left pane items after detecting nonexistent item");
    }

    _("Move query out of left pane root");
    {
      let queryId = await PlacesUIUtils.leftPaneQueries.Downloads;
      let queryGuid = await PlacesUtils.promiseItemGuid(queryId);
      await PlacesUtils.bookmarks.update({
        guid: queryGuid,
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });

      leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
      _(`Left pane root ID after restoring moved query: ${leftPaneId}`);

      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {},
        "Should not track left pane items after restoring moved query");
    }

    _("Add duplicate query");
    {
      let leftPaneGuid = await PlacesUtils.promiseItemGuid(leftPaneId);
      let query = await PlacesUtils.bookmarks.insert({
        parentGuid: leftPaneGuid,
        url: `place:folder=TAGS`,
      });
      let queryId = await PlacesUtils.promiseItemId(query.guid);
      await setAnnoUnchecked(queryId, PlacesUIUtils.ORGANIZER_QUERY_ANNO,
                             "Tags", PlacesUtils.annotations.TYPE_STRING);

      leftPaneId = PlacesUIUtils.maybeRebuildLeftPane();
      _(`Left pane root ID after removing dupe query: ${leftPaneId}`);

      let changes = await tracker.promiseChangedIDs();
      deepEqual(changes, {},
        "Should not track left pane items after removing dupe query");
    }
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_tracking() {
  _("Test starting and stopping the tracker");

  // Remove existing tracking information for roots.
  await startTracking();

  let folder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.bookmarksMenuFolder,
    "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);

  // creating the folder should have made 2 changes - the folder itself and
  // the parent of the folder.
  await verifyTrackedCount(2);
  // Reset the changes as the rest of the test doesn't want to see these.
  await resetTracker();

  function createBmk() {
    return PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
  }

  try {
    _("Tell the tracker to start tracking changes.");
    await startTracking();
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    await verifyTrackedCount(2);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    _("Notifying twice won't do any harm.");
    createBmk();
    await verifyTrackedCount(3);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_batch_tracking() {
  _("Test tracker does the correct thing during and after a places 'batch'");

  await startTracking();

  PlacesUtils.bookmarks.runInBatchMode({
    runBatched() {
      PlacesUtils.bookmarks.createFolder(
        PlacesUtils.bookmarks.bookmarksMenuFolder,
        "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
      // We should be tracking the new folder and its parent (and need to jump
      // through blocking hoops...)
      Async.promiseSpinningly(verifyTrackedCount(2));
      // But not have bumped the score.
      do_check_eq(tracker.score, 0);
    }
  }, null);

  // Out of batch mode - tracker should be the same, but score should be up.
  await verifyTrackedCount(2);
  do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  await cleanup();
});

add_task(async function test_nested_batch_tracking() {
  _("Test tracker does the correct thing if a places 'batch' is nested");

  await startTracking();

  PlacesUtils.bookmarks.runInBatchMode({
    runBatched() {

      PlacesUtils.bookmarks.runInBatchMode({
        runBatched() {
          PlacesUtils.bookmarks.createFolder(
            PlacesUtils.bookmarks.bookmarksMenuFolder,
            "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
          // We should be tracking the new folder and its parent (and need to jump
          // through blocking hoops...)
          Async.promiseSpinningly(verifyTrackedCount(2));
          // But not have bumped the score.
          do_check_eq(tracker.score, 0);
        }
      }, null);
      _("inner batch complete.");
      // should still not have a score as the outer batch is pending.
      do_check_eq(tracker.score, 0);
    }
  }, null);

  // Out of both batches - tracker should be the same, but score should be up.
  await verifyTrackedCount(2);
  do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  await cleanup();
});

add_task(async function test_tracker_sql_batching() {
  _("Test tracker does the correct thing when it is forced to batch SQL queries");

  const SQLITE_MAX_VARIABLE_NUMBER = 999;
  let numItems = SQLITE_MAX_VARIABLE_NUMBER * 2 + 10;
  let createdIDs = [];

  await startTracking();

  PlacesUtils.bookmarks.runInBatchMode({
    runBatched() {
      for (let i = 0; i < numItems; i++) {
        let syncBmkID = PlacesUtils.bookmarks.insertBookmark(
                          PlacesUtils.bookmarks.unfiledBookmarksFolder,
                          Utils.makeURI("https://example.org/" + i),
                          PlacesUtils.bookmarks.DEFAULT_INDEX,
                          "Sync Bookmark " + i);
        createdIDs.push(syncBmkID);
      }
    }
  }, null);

  do_check_eq(createdIDs.length, numItems);
  await verifyTrackedCount(numItems + 1); // the folder is also tracked.
  await resetTracker();

  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.unfiledBookmarksFolder);
  await verifyTrackedCount(numItems + 1);

  await cleanup();
});

add_task(async function test_onItemAdded() {
  _("Items inserted via the synchronous bookmarks API should be tracked");

  try {
    await startTracking();

    _("Insert a folder using the sync API");
    let syncFolderID = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Sync Folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let syncFolderGUID = engine._store.GUIDForId(syncFolderID);
    await verifyTrackedItems(["menu", syncFolderGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a bookmark using the sync API");
    let syncBmkID = PlacesUtils.bookmarks.insertBookmark(syncFolderID,
      Utils.makeURI("https://example.org/sync"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Sync Bookmark");
    let syncBmkGUID = engine._store.GUIDForId(syncBmkID);
    await verifyTrackedItems([syncFolderGUID, syncBmkGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a separator using the sync API");
    let syncSepID = PlacesUtils.bookmarks.insertSeparator(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      PlacesUtils.bookmarks.getItemIndex(syncFolderID));
    let syncSepGUID = engine._store.GUIDForId(syncSepID);
    await verifyTrackedItems(["menu", syncSepGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemAdded() {
  _("Items inserted via the asynchronous bookmarks API should be tracked");

  try {
    await startTracking();

    _("Insert a folder using the async API");
    let asyncFolder = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Async Folder",
    });
    await verifyTrackedItems(["menu", asyncFolder.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a bookmark using the async API");
    let asyncBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: asyncFolder.guid,
      url: "https://example.org/async",
      title: "Async Bookmark",
    });
    await verifyTrackedItems([asyncFolder.guid, asyncBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a separator using the async API");
    let asyncSep = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: asyncFolder.index,
    });
    await verifyTrackedItems(["menu", asyncSep.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemChanged() {
  _("Items updated using the asynchronous bookmarks API should be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark");
    let fxBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);

    await startTracking();

    _("Update the bookmark using the async API");
    await PlacesUtils.bookmarks.update({
      guid: fxBmk.guid,
      title: "Download Firefox",
      url: "https://www.mozilla.org/firefox",
      // PlacesUtils.bookmarks.update rejects last modified dates older than
      // the added date.
      lastModified: new Date(Date.now() + DAY_IN_MS),
    });

    await verifyTrackedItems([fxBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemChanged_itemDates() {
  _("Changes to item dates should be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark");
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    await startTracking();

    _("Reset the bookmark's added date");
    // Convert to microseconds for PRTime.
    let dateAdded = (Date.now() - DAY_IN_MS) * 1000;
    PlacesUtils.bookmarks.setItemDateAdded(fx_id, dateAdded);
    await verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    _("Set the bookmark's last modified date");
    let dateModified = Date.now() * 1000;
    PlacesUtils.bookmarks.setItemLastModified(fx_id, dateModified);
    await verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemChanged_changeBookmarkURI() {
  _("Changes to bookmark URIs should be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark");
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    _("Set a tracked annotation to make sure we only notify once");
    PlacesUtils.annotations.setItemAnnotation(
      fx_id, PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO, "A test description", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);

    await startTracking();

    _("Change the bookmark's URI");
    PlacesUtils.bookmarks.changeBookmarkURI(fx_id,
      Utils.makeURI("https://www.mozilla.org/firefox"));
    await verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemTagged() {
  _("Items tagged using the synchronous API should be tracked");

  try {
    await stopTracking();

    _("Create a folder");
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folderGUID = engine._store.GUIDForId(folder);
    _("Folder ID: " + folder);
    _("Folder GUID: " + folderGUID);

    _("Track changes to tags");
    let uri = Utils.makeURI("http://getfirefox.com");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    await startTracking();

    _("Tag the item");
    PlacesUtils.tagging.tagURI(uri, ["foo"]);

    // bookmark should be tracked, folder should not be.
    await verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemUntagged() {
  _("Items untagged using the synchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert tagged bookmarks");
    let uri = Utils.makeURI("http://getfirefox.com");
    let fx1ID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let fx1GUID = engine._store.GUIDForId(fx1ID);
    // Different parent and title; same URL.
    let fx2ID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Download Firefox");
    let fx2GUID = engine._store.GUIDForId(fx2ID);
    PlacesUtils.tagging.tagURI(uri, ["foo"]);

    await startTracking();

    _("Remove the tag");
    PlacesUtils.tagging.untagURI(uri, ["foo"]);

    await verifyTrackedItems([fx1GUID, fx2GUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemUntagged() {
  _("Items untagged using the asynchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert tagged bookmarks");
    let fxBmk1 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });
    let tag = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: "some tag",
    });
    let fxTag = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: tag.guid,
      url: "http://getfirefox.com",
    });

    await startTracking();

    _("Remove the tag using the async bookmarks API");
    await PlacesUtils.bookmarks.remove(fxTag.guid);

    await verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemTagged() {
  _("Items tagged using the asynchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert untagged bookmarks");
    let folder1 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Folder 1",
    });
    let fxBmk1 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: folder1.guid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let folder2 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Folder 2",
    });
    // Different parent and title; same URL.
    let fxBmk2 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: folder2.guid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });

    await startTracking();

    // This will change once tags are moved into a separate table (bug 424160).
    // We specifically test this case because Bookmarks.jsm updates tagged
    // bookmarks and notifies observers.
    _("Insert a tag using the async bookmarks API");
    let tag = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: "some tag",
    });

    _("Tag an item using the async bookmarks API");
    await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: tag.guid,
      url: "http://getfirefox.com",
    });

    await verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemKeywordChanged() {
  _("Keyword changes via the synchronous API should be tracked");

  try {
    await stopTracking();
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    _("Track changes to keywords");
    let uri = Utils.makeURI("http://getfirefox.com");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    await startTracking();

    _("Give the item a keyword");
    PlacesUtils.bookmarks.setKeywordForBookmark(b, "the_keyword");

    // bookmark should be tracked, folder should not be.
    await verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemKeywordChanged() {
  _("Keyword changes via the asynchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert two bookmarks with the same URL");
    let fxBmk1 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });

    await startTracking();

    _("Add a keyword for both items");
    await PlacesUtils.keywords.insert({
      keyword: "the_keyword",
      url: "http://getfirefox.com",
      postData: "postData",
    });

    await verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemKeywordDeleted() {
  _("Keyword deletions via the asynchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert two bookmarks with the same URL and keywords");
    let fxBmk1 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });
    await PlacesUtils.keywords.insert({
      keyword: "the_keyword",
      url: "http://getfirefox.com",
    });

    await startTracking();

    _("Remove the keyword");
    await PlacesUtils.keywords.remove("the_keyword");

    await verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemPostDataChanged() {
  _("Post data changes should be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark");
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    await startTracking();

    // PlacesUtils.setPostDataForBookmark is deprecated, but still used by
    // PlacesTransactions.NewBookmark.
    _("Post data for the bookmark should be ignored");
    await PlacesUtils.setPostDataForBookmark(fx_id, "postData");
    await verifyTrackedItems([]);
    do_check_eq(tracker.score, 0);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemAnnoChanged() {
  _("Item annotations should be tracked");

  try {
    await stopTracking();
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    _("Track changes to annos.");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    await startTracking();
    PlacesUtils.annotations.setItemAnnotation(
      b, PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO, "A test description", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    // bookmark should be tracked, folder should not.
    await verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    PlacesUtils.annotations.removeItemAnnotation(b,
      PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO);
    await verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemAdded_filtered_root() {
  _("Items outside the change roots should not be tracked");

  try {
    await startTracking();

    _("Create a new root");
    let rootID = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.placesRoot,
      "New root",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let rootGUID = engine._store.GUIDForId(rootID);
    _(`New root GUID: ${rootGUID}`);

    _("Insert a bookmark underneath the new root");
    let untrackedBmkID = PlacesUtils.bookmarks.insertBookmark(
      rootID,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let untrackedBmkGUID = engine._store.GUIDForId(untrackedBmkID);
    _(`New untracked bookmark GUID: ${untrackedBmkGUID}`);

    _("Insert a bookmark underneath the Places root");
    let rootBmkID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.placesRoot,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let rootBmkGUID = engine._store.GUIDForId(rootBmkID);
    _(`New Places root bookmark GUID: ${rootBmkGUID}`);

    _("New root and bookmark should be ignored");
    await verifyTrackedItems([]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_filtered_root() {
  _("Deleted items outside the change roots should not be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark underneath the Places root");
    let rootBmkID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.placesRoot,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let rootBmkGUID = engine._store.GUIDForId(rootBmkID);
    _(`New Places root bookmark GUID: ${rootBmkGUID}`);

    await startTracking();

    PlacesUtils.bookmarks.removeItem(rootBmkID);

    await verifyTrackedItems([]);
    // We'll still increment the counter for the removed item.
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onPageAnnoChanged() {
  _("Page annotations should not be tracked");

  try {
    await stopTracking();

    _("Insert a bookmark without an annotation");
    let pageURI = Utils.makeURI("http://getfirefox.com");
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      pageURI,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");

    await startTracking();

    _("Add a page annotation");
    PlacesUtils.annotations.setPageAnnotation(pageURI, "URIProperties/characterSet",
      "UTF-8", 0, PlacesUtils.annotations.EXPIRE_NEVER);
    await verifyTrackedItems([]);
    do_check_eq(tracker.score, 0);
    await resetTracker();

    _("Remove the page annotation");
    PlacesUtils.annotations.removePageAnnotation(pageURI,
      "URIProperties/characterSet");
    await verifyTrackedItems([]);
    do_check_eq(tracker.score, 0);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onFaviconChanged() {
  _("Favicon changes should not be tracked");

  try {
    await stopTracking();

    let pageURI = Utils.makeURI("http://getfirefox.com");
    let iconURI = Utils.makeURI("http://getfirefox.com/icon");
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      pageURI,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");

    await PlacesTestUtils.addVisits(pageURI);

    await startTracking();

    _("Favicon annotations should be ignored");
    let iconURL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAA" +
      "AAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

    PlacesUtils.favicons.replaceFaviconDataFromDataURL(iconURI, iconURL, 0,
      Services.scriptSecurityManager.getSystemPrincipal());

    await new Promise(resolve => {
      PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, iconURI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, (uri, dataLen, data, mimeType) => {
          resolve();
        },
        Services.scriptSecurityManager.getSystemPrincipal());
    });
    await verifyTrackedItems([]);
    do_check_eq(tracker.score, 0);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onLivemarkAdded() {
  _("New livemarks should be tracked");

  try {
    await startTracking();

    _("Insert a livemark");
    let livemark = await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      // Use a local address just in case, to avoid potential aborts for
      // non-local connections.
      feedURI: Utils.makeURI("http://localhost:0"),
    });
    // Prevent the livemark refresh timer from requesting the URI.
    livemark.terminate();

    await verifyTrackedItems(["menu", livemark.guid]);
    // Two observer notifications: one for creating the livemark folder, and
    // one for setting the "livemark/feedURI" anno on the folder.
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onLivemarkDeleted() {
  _("Deleted livemarks should be tracked");

  try {
    await stopTracking();

    _("Insert a livemark");
    let livemark = await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: Utils.makeURI("http://localhost:0"),
    });
    livemark.terminate();

    await startTracking();

    _("Remove a livemark");
    await PlacesUtils.livemarks.removeLivemark({
      guid: livemark.guid,
    });

    await verifyTrackedItems(["menu", livemark.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemMoved() {
  _("Items moved via the synchronous API should be tracked");

  try {
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _("Firefox GUID: " + fx_guid);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);
    _("Thunderbird GUID: " + tb_guid);

    await startTracking();

    // Moving within the folder will just track the folder.
    PlacesUtils.bookmarks.moveItem(
      tb_id, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
    await verifyTrackedItems(["menu"]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();
    await PlacesTestUtils.markBookmarksAsSynced();

    // Moving a bookmark to a different folder will track the old
    // folder, the new folder and the bookmark.
    PlacesUtils.bookmarks.moveItem(fx_id, PlacesUtils.bookmarks.toolbarFolder,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
    await verifyTrackedItems(["menu", "toolbar", fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemMoved_update() {
  _("Items moved via the asynchronous API should be tracked");

  try {
    await stopTracking();

    await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let tbBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });

    await startTracking();

    _("Repositioning a bookmark should track the folder");
    await PlacesUtils.bookmarks.update({
      guid: tbBmk.guid,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: 0,
    });
    await verifyTrackedItems(["menu"]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    _("Reparenting a bookmark should track both folders and the bookmark");
    await PlacesUtils.bookmarks.update({
      guid: tbBmk.guid,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    await verifyTrackedItems(["menu", "toolbar", tbBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemMoved_reorder() {
  _("Items reordered via the asynchronous API should be tracked");

  try {
    await stopTracking();

    _("Insert out-of-order bookmarks");
    let fxBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);

    let tbBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });
    _(`Thunderbird GUID: ${tbBmk.guid}`);

    let mozBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://mozilla.org",
      title: "Mozilla",
    });
    _(`Mozilla GUID: ${mozBmk.guid}`);

    await startTracking();

    _("Reorder bookmarks");
    await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.menuGuid,
      [mozBmk.guid, fxBmk.guid, tbBmk.guid]);

    // As with setItemIndex, we should only track the folder if we reorder
    // its children, but we should bump the score for every changed item.
    await verifyTrackedItems(["menu"]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemMoved_setItemIndex() {
  _("Items with updated indices should be tracked");

  try {
    await stopTracking();

    let folder_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      "Test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder_guid = engine._store.GUIDForId(folder_id);
    _(`Folder GUID: ${folder_guid}`);

    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Thunderbird");
    let tb_guid = engine._store.GUIDForId(tb_id);
    _(`Thunderbird GUID: ${tb_guid}`);

    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Firefox");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    let moz_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("https://mozilla.org"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Mozilla"
    );
    let moz_guid = engine._store.GUIDForId(moz_id);
    _(`Mozilla GUID: ${moz_guid}`);

    await startTracking();

    // PlacesSortFolderByNameTransaction exercises
    // PlacesUtils.bookmarks.setItemIndex.
    let txn = new PlacesSortFolderByNameTransaction(folder_id);

    // We're reordering items within the same folder, so only the folder
    // should be tracked.
    _("Execute the sort folder transaction");
    txn.doTransaction();
    await verifyTrackedItems([folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    _("Undo the sort folder transaction");
    txn.undoTransaction();
    await verifyTrackedItems([folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_removeFolderTransaction() {
  _("Folders removed in a transaction should be tracked");

  try {
    await stopTracking();

    _("Create a folder with two children");
    let folder_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      "Test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder_guid = engine._store.GUIDForId(folder_id);
    _(`Folder GUID: ${folder_guid}`);
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);
    _(`Thunderbird GUID: ${tb_guid}`);

    await startTracking();

    let txn = PlacesUtils.bookmarks.getRemoveFolderTransaction(folder_id);
    // We haven't executed the transaction yet.
    await verifyTrackerEmpty();

    _("Execute the remove folder transaction");
    txn.doTransaction();
    await verifyTrackedItems(["menu", folder_guid, fx_guid, tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
    await resetTracker();

    _("Undo the remove folder transaction");
    txn.undoTransaction();

    // At this point, the restored folder has the same ID, but a different GUID.
    let new_folder_guid = await PlacesUtils.promiseItemGuid(folder_id);

    await verifyTrackedItems(["menu", new_folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    _("Redo the transaction");
    txn.redoTransaction();
    await verifyTrackedItems(["menu", new_folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_treeMoved() {
  _("Moving an entire tree of bookmarks should track the parents");

  try {
    // Create a couple of parent folders.
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      "First test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder1_guid = engine._store.GUIDForId(folder1_id);

    // A second folder in the first.
    let folder2_id = PlacesUtils.bookmarks.createFolder(
      folder1_id,
      "Second test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder2_guid = engine._store.GUIDForId(folder2_id);

    // Create a couple of bookmarks in the second folder.
    PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");

    await startTracking();

    // Move folder 2 to be a sibling of folder1.
    PlacesUtils.bookmarks.moveItem(
      folder2_id, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
    // the menu and both folders should be tracked, the children should not be.
    await verifyTrackedItems(["menu", folder1_guid, folder2_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted() {
  _("Bookmarks deleted via the synchronous API should be tracked");

  try {
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);

    await startTracking();

    // Delete the last item - the item and parent should be tracked.
    PlacesUtils.bookmarks.removeItem(tb_id);

    await verifyTrackedItems(["menu", tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemDeleted() {
  _("Bookmarks deleted via the asynchronous API should be tracked");

  try {
    await stopTracking();

    let fxBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });

    await startTracking();

    _("Delete the first item");
    await PlacesUtils.bookmarks.remove(fxBmk.guid);

    await verifyTrackedItems(["menu", fxBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemDeleted_eraseEverything() {
  _("Erasing everything should track all deleted items");

  try {
    await stopTracking();

    let fxBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.mobileGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);
    let tbBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.mobileGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });
    _(`Thunderbird GUID: ${tbBmk.guid}`);
    let mozBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://mozilla.org",
      title: "Mozilla",
    });
    _(`Mozilla GUID: ${mozBmk.guid}`);
    let mdnBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://developer.mozilla.org",
      title: "MDN",
    });
    _(`MDN GUID: ${mdnBmk.guid}`);
    let bugsFolder = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      title: "Bugs",
    });
    _(`Bugs folder GUID: ${bugsFolder.guid}`);
    let bzBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: bugsFolder.guid,
      url: "https://bugzilla.mozilla.org",
      title: "Bugzilla",
    });
    _(`Bugzilla GUID: ${bzBmk.guid}`);
    let bugsChildFolder = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: bugsFolder.guid,
      title: "Bugs child",
    });
    _(`Bugs child GUID: ${bugsChildFolder.guid}`);
    let bugsGrandChildBmk = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: bugsChildFolder.guid,
      url: "https://example.com",
      title: "Bugs grandchild",
    });
    _(`Bugs grandchild GUID: ${bugsGrandChildBmk.guid}`);

    await startTracking();
    // Simulate moving a synced item into a new folder. Deleting the folder
    // should write a tombstone for the item, but not the folder.
    await PlacesTestUtils.setBookmarkSyncFields({
      guid: bugsChildFolder.guid,
      syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
    });
    await PlacesUtils.bookmarks.eraseEverything();

    // bugsChildFolder's sync status is still "NEW", so it shouldn't be
    // tracked. bugsGrandChildBmk is "NORMAL", so we *should* write a
    // tombstone and track it.
    await verifyTrackedItems(["menu", mozBmk.guid, mdnBmk.guid, "toolbar",
                              bugsFolder.guid, "mobile", fxBmk.guid,
                              tbBmk.guid, "unfiled", bzBmk.guid,
                              bugsGrandChildBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 8);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_removeFolderChildren() {
  _("Removing a folder's children should track the folder and its children");

  try {
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.mobileFolderId,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.mobileFolderId,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);
    _(`Thunderbird GUID: ${tb_guid}`);

    let moz_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("https://mozilla.org"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Mozilla"
    );
    let moz_guid = engine._store.GUIDForId(moz_id);
    _(`Mozilla GUID: ${moz_guid}`);

    await startTracking();

    _(`Mobile root ID: ${PlacesUtils.mobileFolderId}`);
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.mobileFolderId);

    await verifyTrackedItems(["mobile", fx_guid, tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_tree() {
  _("Deleting a tree of bookmarks should track all items");

  try {
    // Create a couple of parent folders.
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      "First test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder1_guid = engine._store.GUIDForId(folder1_id);

    // A second folder in the first.
    let folder2_id = PlacesUtils.bookmarks.createFolder(
      folder1_id,
      "Second test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder2_guid = engine._store.GUIDForId(folder2_id);

    // Create a couple of bookmarks in the second folder.
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);

    await startTracking();

    // Delete folder2 - everything we created should be tracked.
    PlacesUtils.bookmarks.removeItem(folder2_id);

    await verifyTrackedItems([fx_guid, tb_guid, folder1_guid, folder2_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_mobile_query() {
  _("Ensure we correctly create the mobile query");

  try {
    await startTracking();

    // Creates the organizer queries as a side effect.
    let leftPaneId = PlacesUIUtils.leftPaneFolderId;
    _(`Left pane root ID: ${leftPaneId}`);

    let allBookmarksGuids = await fetchGuidsWithAnno("PlacesOrganizer/OrganizerQuery",
                                                     "AllBookmarks");
    equal(allBookmarksGuids.length, 1, "Should create folder with all bookmarks queries");
    let allBookmarkGuid = allBookmarksGuids[0];

    _("Try creating query after organizer is ready");
    tracker._ensureMobileQuery();
    let queryGuids = await fetchGuidsWithAnno("PlacesOrganizer/OrganizerQuery",
                                              "MobileBookmarks");
    equal(queryGuids.length, 0, "Should not create query without any mobile bookmarks");

    _("Insert mobile bookmark, then create query");
    let mozBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.mobileGuid,
      url: "https://mozilla.org",
    });
    tracker._ensureMobileQuery();
    queryGuids = await fetchGuidsWithAnno("PlacesOrganizer/OrganizerQuery",
                                          "MobileBookmarks");
    equal(queryGuids.length, 1, "Should create query once mobile bookmarks exist");

    let queryGuid = queryGuids[0];

    let queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
    equal(queryInfo.url, `place:folder=${PlacesUtils.mobileFolderId}`, "Query should point to mobile root");
    equal(queryInfo.title, "Mobile Bookmarks", "Query title should be localized");
    equal(queryInfo.parentGuid, allBookmarkGuid, "Should append mobile query to all bookmarks queries");

    _("Rename root and query, then recreate");
    await PlacesUtils.bookmarks.update({
      guid: PlacesUtils.bookmarks.mobileGuid,
      title: "renamed root",
    });
    await PlacesUtils.bookmarks.update({
      guid: queryGuid,
      title: "renamed query",
    });
    tracker._ensureMobileQuery();
    let rootInfo = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.mobileGuid);
    equal(rootInfo.title, "Mobile Bookmarks", "Should fix root title");
    queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
    equal(queryInfo.title, "Mobile Bookmarks", "Should fix query title");

    _("Point query to different folder");
    await PlacesUtils.bookmarks.update({
      guid: queryGuid,
      url: "place:folder=BOOKMARKS_MENU",
    });
    tracker._ensureMobileQuery();
    queryInfo = await PlacesUtils.bookmarks.fetch(queryGuid);
    equal(queryInfo.url.href, `place:folder=${PlacesUtils.mobileFolderId}`,
      "Should fix query URL to point to mobile root");

    _("We shouldn't track the query or the left pane root");
    await verifyTrackedItems([mozBmk.guid, "mobile"]);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_skip_migration() {
  await insertBookmarksToMigrate();

  let originalTombstones = await PlacesTestUtils.fetchSyncTombstones();
  let originalFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    "0gtWTOgYcoJD", "0dbpnMdxKxfg", "r5ouWdPB3l28", "YK5Bdq5MIqL6");

  let filePath = OS.Path.join(OS.Constants.Path.profileDir, "weave", "changes",
    "bookmarks.json");

  _("No tracker file");
  {
    await Utils.jsonRemove("changes/bookmarks", tracker);
    ok(!(await OS.File.exists(filePath)), "Tracker file should not exist");

    await tracker._migrateOldEntries();

    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      "0gtWTOgYcoJD", "0dbpnMdxKxfg", "r5ouWdPB3l28", "YK5Bdq5MIqL6");
    deepEqual(fields, originalFields,
      "Sync fields should not change if tracker file is missing");
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones, originalTombstones,
      "Tombstones should not change if tracker file is missing");
  }

  _("Existing tracker file; engine disabled");
  {
    await Utils.jsonSave("changes/bookmarks", tracker, {});
    ok(await OS.File.exists(filePath),
      "Tracker file should exist before disabled engine migration");

    engine.disabled = true;
    await tracker._migrateOldEntries();
    engine.disabled = false;

    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      "0gtWTOgYcoJD", "0dbpnMdxKxfg", "r5ouWdPB3l28", "YK5Bdq5MIqL6");
    deepEqual(fields, originalFields,
      "Sync fields should not change on disabled engine migration");
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones, originalTombstones,
      "Tombstones should not change if tracker file is missing");

    ok(!(await OS.File.exists(filePath)),
      "Tracker file should be deleted after disabled engine migration");
  }

  _("Existing tracker file; first sync");
  {
    await Utils.jsonSave("changes/bookmarks", tracker, {});
    ok(await OS.File.exists(filePath),
      "Tracker file should exist before first sync migration");

    engine.lastSync = 0;
    await tracker._migrateOldEntries();

    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
      "0gtWTOgYcoJD", "0dbpnMdxKxfg", "r5ouWdPB3l28", "YK5Bdq5MIqL6");
    deepEqual(fields, originalFields,
      "Sync fields should not change on first sync migration");
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    deepEqual(tombstones, originalTombstones,
      "Tombstones should not change if tracker file is missing");

    ok(!(await OS.File.exists(filePath)),
      "Tracker file should be deleted after first sync migration");
  }

  await cleanup();
});

add_task(async function test_migrate_empty_tracker() {
  _("Migration with empty tracker file");
  await insertBookmarksToMigrate();

  await Utils.jsonSave("changes/bookmarks", tracker, {});

  engine.lastSync = Date.now() / 1000;
  await tracker._migrateOldEntries();

  let fields = await PlacesTestUtils.fetchBookmarkSyncFields(
    "0gtWTOgYcoJD", "0dbpnMdxKxfg", "r5ouWdPB3l28", "YK5Bdq5MIqL6");
  for (let field of fields) {
    equal(field.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      `Sync status of migrated bookmark ${field.guid} should be NORMAL`);
    strictEqual(field.syncChangeCounter, 0,
      `Change counter of migrated bookmark ${field.guid} should be 0`);
  }

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(tombstones, [], "Migration should delete old tombstones");

  let filePath = OS.Path.join(OS.Constants.Path.profileDir, "weave", "changes",
    "bookmarks.json");
  ok(!(await OS.File.exists(filePath)),
    "Tracker file should be deleted after empty tracker migration");

  await cleanup();
});

add_task(async function test_migrate_existing_tracker() {
  _("Migration with existing tracker entries");
  await insertBookmarksToMigrate();

  let mozBmk = await PlacesUtils.bookmarks.fetch("0gtWTOgYcoJD");
  let fxBmk = await PlacesUtils.bookmarks.fetch("0dbpnMdxKxfg");
  let mozChangeTime = Math.floor(mozBmk.lastModified / 1000) - 60;
  let fxChangeTime = Math.floor(fxBmk.lastModified / 1000) + 60;
  await Utils.jsonSave("changes/bookmarks", tracker, {
    "0gtWTOgYcoJD": mozChangeTime,
    "0dbpnMdxKxfg": {
      modified: fxChangeTime,
      deleted: false,
    },
    "3kdIPWHs9hHC": {
      modified: 1479494951,
      deleted: true,
    },
    "l7DlMy2lL1jL": 1479496460,
  });

  engine.lastSync = Date.now() / 1000;
  await tracker._migrateOldEntries();

  let changedFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    "0gtWTOgYcoJD", "0dbpnMdxKxfg");
  for (let field of changedFields) {
    if (field.guid == "0gtWTOgYcoJD") {
      ok(field.lastModified.getTime(), mozBmk.lastModified.getTime(),
        `Modified time for ${field.guid} should not be reset to older change time`);
    } else if (field.guid == "0dbpnMdxKxfg") {
      equal(field.lastModified.getTime(), fxChangeTime * 1000,
        `Modified time for ${field.guid} should be updated to newer change time`);
    }
    equal(field.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      `Sync status of migrated bookmark ${field.guid} should be NORMAL`);
    ok(field.syncChangeCounter > 0,
      `Change counter of migrated bookmark ${field.guid} should be > 0`);
  }

  let unchangedFields = await PlacesTestUtils.fetchBookmarkSyncFields(
    "r5ouWdPB3l28", "YK5Bdq5MIqL6");
  for (let field of unchangedFields) {
    equal(field.syncStatus, PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
      `Sync status of unchanged bookmark ${field.guid} should be NORMAL`);
    strictEqual(field.syncChangeCounter, 0,
      `Change counter of unchanged bookmark ${field.guid} should be 0`);
  }

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  await deepEqual(tombstones, [{
    guid: "3kdIPWHs9hHC",
    dateRemoved: new Date(1479494951 * 1000),
  }, {
    guid: "l7DlMy2lL1jL",
    dateRemoved: new Date(1479496460 * 1000),
  }], "Should write tombstones for deleted tracked items");

  let filePath = OS.Path.join(OS.Constants.Path.profileDir, "weave", "changes",
    "bookmarks.json");
  ok(!(await OS.File.exists(filePath)),
    "Tracker file should be deleted after existing tracker migration");

  await cleanup();
});
