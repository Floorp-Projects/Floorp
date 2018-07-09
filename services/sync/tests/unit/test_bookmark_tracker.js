/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://services-sync/constants.js");
ChromeUtils.import("resource://services-sync/engines/bookmarks.js");
ChromeUtils.import("resource://services-sync/engines.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource:///modules/PlacesUIUtils.jsm");
ChromeUtils.import("resource://gre/modules/PlacesTransactions.jsm");

let engine;
let store;
let tracker;

const DAY_IN_MS = 24 * 60 * 60 * 1000;

add_task(async function setup() {
  await Service.engineManager.register(BookmarksEngine);
  engine = Service.engineManager.get("bookmarks");
  store  = engine._store;
  tracker = engine._tracker;
  tracker.persistChangedIDs = false;
});

// Test helpers.
async function verifyTrackerEmpty() {
  await PlacesTestUtils.promiseAsyncUpdates();
  let changes = await tracker.getChangedIDs();
  deepEqual(changes, {});
  equal(tracker.score, 0);
}

async function resetTracker() {
  await PlacesTestUtils.markBookmarksAsSynced();
  tracker.resetScore();
}

// We have some tests that uses Places "batch mode", which isn't async aware,
// so a couple of these tests spin an event loop waiting for a promise.
function promiseSpinningly(promise) {
  let resolved = false;
  let rv, rerror;
  promise.then(result => {
    rv = result;
  }, err => {
    rerror = err || new Error("Promise rejected without explicit error");
  }).finally(() => {
    resolved = true;
  });
  let tm = Cc["@mozilla.org/thread-manager;1"].getService();

  // Keep waiting until the promise resolves.
  tm.spinEventLoopUntil(() => resolved);
  if (rerror) {
    throw rerror;
  }
  return rv;
}




async function cleanup() {
  await engine.setLastSync(0);
  engine._needWeakUpload.clear();
  await store.wipe();
  await resetTracker();
  await tracker.stop();
}

// startTracking is a signal that the test wants to notice things that happen
// after this is called (ie, things already tracked should be discarded.)
async function startTracking() {
  engine._tracker.start();
  await PlacesTestUtils.markBookmarksAsSynced();
}

async function verifyTrackedItems(tracked) {
  await PlacesTestUtils.promiseAsyncUpdates();
  let changedIDs = await tracker.getChangedIDs();
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
  await PlacesTestUtils.promiseAsyncUpdates();
  let changedIDs = await tracker.getChangedIDs();
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
  });
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
      folder, CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
  }

  try {
    _("Tell the tracker to start tracking changes.");
    await startTracking();
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    await verifyTrackedCount(2);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

    _("Notifying twice won't do any harm.");
    createBmk();
    await verifyTrackedCount(3);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);

  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_tracker_sql_batching() {
  _("Test tracker does the correct thing when it is forced to batch SQL queries");

  const SQLITE_MAX_VARIABLE_NUMBER = 999;
  let numItems = SQLITE_MAX_VARIABLE_NUMBER * 2 + 10;

  await startTracking();

  let children = [];
  for (let i = 0; i < numItems; i++) {
    children.push({
      url: "https://example.org/" + i,
      title: "Sync Bookmark " + i
    });
  }
  let inserted = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children
    }]
  });


  Assert.equal(children.length, numItems);
  Assert.equal(inserted.length, numItems + 1);
  await verifyTrackedCount(numItems + 2); // The parent and grandparent are also tracked.
  await resetTracker();

  await PlacesUtils.bookmarks.remove(inserted[0]);
  await verifyTrackedCount(numItems + 2);

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
    let syncFolderGUID = await PlacesUtils.promiseItemGuid(syncFolderID);
    await verifyTrackedItems(["menu", syncFolderGUID]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a bookmark using the sync API");
    let syncBmkID = PlacesUtils.bookmarks.insertBookmark(syncFolderID,
      CommonUtils.makeURI("https://example.org/sync"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Sync Bookmark");
    let syncBmkGUID = await PlacesUtils.promiseItemGuid(syncBmkID);
    await verifyTrackedItems([syncFolderGUID, syncBmkGUID]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();
    await startTracking();

    _("Insert a separator using the async API");
    let asyncSep = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: asyncFolder.index,
    });
    await verifyTrackedItems(["menu", asyncSep.guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemChanged() {
  _("Items updated using the asynchronous bookmarks API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemChanged_itemDates() {
  _("Changes to item dates should be tracked");

  try {
    await tracker.stop();

    _("Insert a bookmark");
    let fx_bm = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!"
    });
    _(`Firefox GUID: ${fx_bm.guid}`);

    await startTracking();

    _("Reset the bookmark's added date, should not be tracked");
    let dateAdded = new Date(Date.now() - DAY_IN_MS);
    await PlacesUtils.bookmarks.update({
      guid: fx_bm.guid,
      dateAdded
    });
    await verifyTrackedCount(0);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

    await resetTracker();

    _("Reset the bookmark's added date and another property, should be tracked");
    dateAdded = new Date();
    await PlacesUtils.bookmarks.update({
      guid: fx_bm.guid,
      dateAdded,
      title: "test"
    });
    await verifyTrackedItems([fx_bm.guid]);
    Assert.equal(tracker.score, 2 * SCORE_INCREMENT_XLARGE);

    await resetTracker();

    _("Set the bookmark's last modified date");
    let fx_id = await PlacesUtils.promiseItemId(fx_bm.guid);
    let dateModified = Date.now() * 1000;
    PlacesUtils.bookmarks.setItemLastModified(fx_id, dateModified);
    await verifyTrackedItems([fx_bm.guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemTagged() {
  _("Items tagged using the synchronous API should be tracked");

  try {
    await tracker.stop();

    _("Create a folder");
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folderGUID = await PlacesUtils.promiseItemGuid(folder);
    _("Folder ID: " + folder);
    _("Folder GUID: " + folderGUID);

    _("Track changes to tags");
    let uri = CommonUtils.makeURI("http://getfirefox.com");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = await PlacesUtils.promiseItemGuid(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    await startTracking();

    _("Tag the item");
    PlacesUtils.tagging.tagURI(uri, ["foo"]);

    // bookmark should be tracked, folder should not be.
    await verifyTrackedItems([bGUID]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemUntagged() {
  _("Items untagged using the synchronous API should be tracked");

  try {
    await tracker.stop();

    _("Insert tagged bookmarks");
    let uri = CommonUtils.makeURI("http://getfirefox.com");
    let fx1ID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let fx1GUID = await PlacesUtils.promiseItemGuid(fx1ID);
    // Different parent and title; same URL.
    let fx2ID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Download Firefox");
    let fx2GUID = await PlacesUtils.promiseItemGuid(fx2ID);
    PlacesUtils.tagging.tagURI(uri, ["foo"]);

    await startTracking();

    _("Remove the tag");
    PlacesUtils.tagging.untagURI(uri, ["foo"]);

    await verifyTrackedItems([fx1GUID, fx2GUID]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemUntagged() {
  _("Items untagged using the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemTagged() {
  _("Items tagged using the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemKeywordChanged() {
  _("Keyword changes via the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemKeywordDeleted() {
  _("Keyword deletions via the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);
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
    let rootGUID = await PlacesUtils.promiseItemGuid(rootID);
    _(`New root GUID: ${rootGUID}`);

    _("Insert a bookmark underneath the new root");
    let untrackedBmkID = PlacesUtils.bookmarks.insertBookmark(
      rootID,
      CommonUtils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let untrackedBmkGUID = await PlacesUtils.promiseItemGuid(untrackedBmkID);
    _(`New untracked bookmark GUID: ${untrackedBmkGUID}`);

    _("Insert a bookmark underneath the Places root");
    let rootBmkID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.placesRoot,
      CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let rootBmkGUID = await PlacesUtils.promiseItemGuid(rootBmkID);
    _(`New Places root bookmark GUID: ${rootBmkGUID}`);

    _("New root and bookmark should be ignored");
    await verifyTrackedItems([]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_filtered_root() {
  _("Deleted items outside the change roots should not be tracked");

  try {
    await tracker.stop();

    _("Insert a bookmark underneath the Places root");
    let rootBmkID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.placesRoot,
      CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let rootBmkGUID = await PlacesUtils.promiseItemGuid(rootBmkID);
    _(`New Places root bookmark GUID: ${rootBmkGUID}`);

    await startTracking();

    PlacesUtils.bookmarks.removeItem(rootBmkID);

    await verifyTrackedItems([]);
    // We'll still increment the counter for the removed item.
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onPageAnnoChanged() {
  _("Page annotations should not be tracked");

  try {
    await tracker.stop();

    _("Insert a bookmark without an annotation");
    let pageURI = CommonUtils.makeURI("http://getfirefox.com");
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
    Assert.equal(tracker.score, 0);
    await resetTracker();

    _("Remove the page annotation");
    PlacesUtils.annotations.removePageAnnotation(pageURI,
      "URIProperties/characterSet");
    await verifyTrackedItems([]);
    Assert.equal(tracker.score, 0);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onFaviconChanged() {
  _("Favicon changes should not be tracked");

  try {
    await tracker.stop();

    let pageURI = CommonUtils.makeURI("http://getfirefox.com");
    let iconURI = CommonUtils.makeURI("http://getfirefox.com/icon");
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
    Assert.equal(tracker.score, 0);
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
      feedURI: CommonUtils.makeURI("http://localhost:0"),
    });
    // Prevent the livemark refresh timer from requesting the URI.
    livemark.terminate();

    await verifyTrackedItems(["menu", livemark.guid]);
    // Two observer notifications: one for creating the livemark folder, and
    // one for setting the "livemark/feedURI" anno on the folder.
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onLivemarkDeleted() {
  _("Deleted livemarks should be tracked");

  try {
    await tracker.stop();

    _("Insert a livemark");
    let livemark = await PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: CommonUtils.makeURI("http://localhost:0"),
    });
    livemark.terminate();

    await startTracking();

    _("Remove a livemark");
    await PlacesUtils.livemarks.removeLivemark({
      guid: livemark.guid,
    });

    await verifyTrackedItems(["menu", livemark.guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemMoved_moveToFolder() {
  _("Items moved via `moveToFolder` should be tracked");

  try {
    await tracker.stop();

    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.menuGuid,
      children: [{
        guid: "bookmarkAAAA",
        title: "A",
        url: "http://example.com/a",
      }, {
        guid: "bookmarkBBBB",
        title: "B",
        url: "http://example.com/b",
      }, {
        guid: "bookmarkCCCC",
        title: "C",
        url: "http://example.com/c",
      }, {
        guid: "bookmarkDDDD",
        title: "D",
        url: "http://example.com/d",
      }],
    });
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.toolbarGuid,
      children: [{
        guid: "bookmarkEEEE",
        title: "E",
        url: "http://example.com/e",
      }],
    });

    await startTracking();

    _("Move (A B D) to the toolbar");
    await PlacesUtils.bookmarks.moveToFolder(
      ["bookmarkAAAA", "bookmarkBBBB", "bookmarkDDDD"],
      PlacesUtils.bookmarks.toolbarGuid,
      PlacesUtils.bookmarks.DEFAULT_INDEX);

    // Moving multiple bookmarks between two folders should track the old
    // folder, new folder, and moved bookmarks.
    await verifyTrackedItems(["menu", "toolbar", "bookmarkAAAA",
      "bookmarkBBBB", "bookmarkDDDD"]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
    await resetTracker();

    _("Reorder toolbar children: (D A B E)");
    await PlacesUtils.bookmarks.moveToFolder(
      ["bookmarkDDDD", "bookmarkAAAA", "bookmarkBBBB"],
      PlacesUtils.bookmarks.toolbarGuid,
      0);

    // Reordering bookmarks in a folder should only track the folder, not the
    // bookmarks.
    await verifyTrackedItems(["toolbar"]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemMoved_update() {
  _("Items moved via the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
    await resetTracker();

    _("Reparenting a bookmark should track both folders and the bookmark");
    await PlacesUtils.bookmarks.update({
      guid: tbBmk.guid,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    await verifyTrackedItems(["menu", "toolbar", tbBmk.guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemMoved_reorder() {
  _("Items reordered via the asynchronous API should be tracked");

  try {
    await tracker.stop();

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

    // We only track the folder if we reorder its children, but we should
    // bump the score for every changed item.
    await verifyTrackedItems(["menu"]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_onItemDeleted_removeFolderTransaction() {
  _("Folders removed in a transaction should be tracked");

  try {
    await tracker.stop();

    _("Create a folder with two children");
    let folder_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      "Test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder_guid = await PlacesUtils.promiseItemGuid(folder_id);
    _(`Folder GUID: ${folder_guid}`);
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = await PlacesUtils.promiseItemGuid(fx_id);
    _(`Firefox GUID: ${fx_guid}`);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      folder_id,
      CommonUtils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = await PlacesUtils.promiseItemGuid(tb_id);
    _(`Thunderbird GUID: ${tb_guid}`);

    await startTracking();

    let txn = PlacesTransactions.Remove({guid: folder_guid});
    // We haven't executed the transaction yet.
    await verifyTrackerEmpty();

    _("Execute the remove folder transaction");
    await txn.transact();
    await verifyTrackedItems(["menu", folder_guid, fx_guid, tb_guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
    await resetTracker();

    _("Undo the remove folder transaction");
    await PlacesTransactions.undo();

    await verifyTrackedItems(["menu", folder_guid, fx_guid, tb_guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
    await resetTracker();

    _("Redo the transaction");
    await PlacesTransactions.redo();
    await verifyTrackedItems(["menu", folder_guid, fx_guid, tb_guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_treeMoved() {
  _("Moving an entire tree of bookmarks should track the parents");

  try {
    // Create a couple of parent folders.
    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      test: "First test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER
    });

    // A second folder in the first.
    let folder2 = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      title: "Second test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER
    });

    // Create a couple of bookmarks in the second folder.
    await PlacesUtils.bookmarks.insert({
      parentGuid: folder2.guid,
      url: "http://getfirefox.com",
      title: "Get Firefox!"
    });
    await PlacesUtils.bookmarks.insert({
      parentGuid: folder2.guid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!"
    });

    await startTracking();

    // Move folder 2 to be a sibling of folder1.
    await PlacesUtils.bookmarks.update({
      guid: folder2.guid,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: 0
    });

    // the menu and both folders should be tracked, the children should not be.
    await verifyTrackedItems(["menu", folder1.guid, folder2.guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
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
      CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      CommonUtils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = await PlacesUtils.promiseItemGuid(tb_id);

    await startTracking();

    // Delete the last item - the item and parent should be tracked.
    PlacesUtils.bookmarks.removeItem(tb_id);

    await verifyTrackedItems(["menu", tb_guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemDeleted() {
  _("Bookmarks deleted via the asynchronous API should be tracked");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});

add_task(async function test_async_onItemDeleted_eraseEverything() {
  _("Erasing everything should track all deleted items");

  try {
    await tracker.stop();

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
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 8);
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
    let folder1_guid = await PlacesUtils.promiseItemGuid(folder1_id);

    // A second folder in the first.
    let folder2_id = PlacesUtils.bookmarks.createFolder(
      folder1_id,
      "Second test folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folder2_guid = await PlacesUtils.promiseItemGuid(folder2_id);

    // Create a couple of bookmarks in the second folder.
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      CommonUtils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = await PlacesUtils.promiseItemGuid(fx_id);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      folder2_id,
      CommonUtils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = await PlacesUtils.promiseItemGuid(tb_id);

    await startTracking();

    // Delete folder2 - everything we created should be tracked.
    PlacesUtils.bookmarks.removeItem(folder2_id);

    await verifyTrackedItems([fx_guid, tb_guid, folder1_guid, folder2_guid]);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    await cleanup();
  }
});
