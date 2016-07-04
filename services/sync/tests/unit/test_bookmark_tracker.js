/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-sync/bookmark_utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

Service.engineManager.register(BookmarksEngine);
var engine = Service.engineManager.get("bookmarks");
var store  = engine._store;
var tracker = engine._tracker;

store.wipe();
tracker.persistChangedIDs = false;

const DAY_IN_MS = 24 * 60 * 60 * 1000;

// Test helpers.
function* verifyTrackerEmpty() {
  do_check_empty(tracker.changedIDs);
  do_check_eq(tracker.score, 0);
}

function* resetTracker() {
  tracker.clearChangedIDs();
  tracker.resetScore();
}

function* cleanup() {
  store.wipe();
  yield resetTracker();
  yield stopTracking();
}

// startTracking is a signal that the test wants to notice things that happen
// after this is called (ie, things already tracked should be discarded.)
function* startTracking() {
  Svc.Obs.notify("weave:engine:start-tracking");
}

function* stopTracking() {
  Svc.Obs.notify("weave:engine:stop-tracking");
}

function* verifyTrackedItems(tracked) {
  let trackedIDs = new Set(Object.keys(tracker.changedIDs));
  for (let guid of tracked) {
    ok(tracker.changedIDs[guid] > 0, `${guid} should be tracked`);
    trackedIDs.delete(guid);
  }
  equal(trackedIDs.size, 0, `Unhandled tracked IDs: ${
    JSON.stringify(Array.from(trackedIDs))}`);
}

function* verifyTrackedCount(expected) {
  do_check_attribute_count(tracker.changedIDs, expected);
}

add_task(function* test_tracking() {
  _("Test starting and stopping the tracker");

  let folder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.bookmarksMenuFolder,
    "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
  function createBmk() {
    return PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    createBmk();
    yield verifyTrackedCount(0);
    do_check_eq(tracker.score, 0);

    _("Tell the tracker to start tracking changes.");
    yield startTracking();
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    yield verifyTrackedCount(2);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    _("Notifying twice won't do any harm.");
    yield startTracking();
    createBmk();
    yield verifyTrackedCount(3);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);

    _("Let's stop tracking again.");
    yield resetTracker();
    yield stopTracking();
    createBmk();
    yield verifyTrackedCount(0);
    do_check_eq(tracker.score, 0);

    _("Notifying twice won't do any harm.");
    yield stopTracking();
    createBmk();
    yield verifyTrackedCount(0);
    do_check_eq(tracker.score, 0);

  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_batch_tracking() {
  _("Test tracker does the correct thing during and after a places 'batch'");

  yield startTracking();

  PlacesUtils.bookmarks.runInBatchMode({
    runBatched: function() {
      let folder = PlacesUtils.bookmarks.createFolder(
        PlacesUtils.bookmarks.bookmarksMenuFolder,
        "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
      // We should be tracking the new folder and its parent (and need to jump
      // through blocking hoops...)
      Async.promiseSpinningly(Task.spawn(verifyTrackedCount(2)));
      // But not have bumped the score.
      do_check_eq(tracker.score, 0);
    }
  }, null);

  // Out of batch mode - tracker should be the same, but score should be up.
  yield verifyTrackedCount(2);
  do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  yield cleanup();
});

add_task(function* test_nested_batch_tracking() {
  _("Test tracker does the correct thing if a places 'batch' is nested");

  yield startTracking();

  PlacesUtils.bookmarks.runInBatchMode({
    runBatched: function() {

      PlacesUtils.bookmarks.runInBatchMode({
        runBatched: function() {
          let folder = PlacesUtils.bookmarks.createFolder(
            PlacesUtils.bookmarks.bookmarksMenuFolder,
            "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
          // We should be tracking the new folder and its parent (and need to jump
          // through blocking hoops...)
          Async.promiseSpinningly(Task.spawn(verifyTrackedCount(2)));
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
  yield verifyTrackedCount(2);
  do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  yield cleanup();
});

add_task(function* test_onItemAdded() {
  _("Items inserted via the synchronous bookmarks API should be tracked");

  try {
    yield startTracking();

    _("Insert a folder using the sync API");
    let syncFolderID = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Sync Folder",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let syncFolderGUID = engine._store.GUIDForId(syncFolderID);
    yield verifyTrackedItems(["menu", syncFolderGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    yield resetTracker();
    yield startTracking();

    _("Insert a bookmark using the sync API");
    let syncBmkID = PlacesUtils.bookmarks.insertBookmark(syncFolderID,
      Utils.makeURI("https://example.org/sync"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Sync Bookmark");
    let syncBmkGUID = engine._store.GUIDForId(syncBmkID);
    yield verifyTrackedItems([syncFolderGUID, syncBmkGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    yield resetTracker();
    yield startTracking();

    _("Insert a separator using the sync API");
    let syncSepID = PlacesUtils.bookmarks.insertSeparator(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      PlacesUtils.bookmarks.getItemIndex(syncFolderID));
    let syncSepGUID = engine._store.GUIDForId(syncSepID);
    yield verifyTrackedItems(["menu", syncSepGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemAdded() {
  _("Items inserted via the asynchronous bookmarks API should be tracked");

  try {
    yield startTracking();

    _("Insert a folder using the async API");
    let asyncFolder = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Async Folder",
    });
    yield verifyTrackedItems(["menu", asyncFolder.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    yield resetTracker();
    yield startTracking();

    _("Insert a bookmark using the async API");
    let asyncBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: asyncFolder.guid,
      url: "https://example.org/async",
      title: "Async Bookmark",
    });
    yield verifyTrackedItems([asyncFolder.guid, asyncBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    yield resetTracker();
    yield startTracking();

    _("Insert a separator using the async API");
    let asyncSep = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: asyncFolder.index,
    });
    yield verifyTrackedItems(["menu", asyncSep.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemChanged() {
  _("Items updated using the asynchronous bookmarks API should be tracked");

  try {
    yield stopTracking();

    _("Insert a bookmark");
    let fxBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);

    yield startTracking();

    _("Update the bookmark using the async API");
    yield PlacesUtils.bookmarks.update({
      guid: fxBmk.guid,
      title: "Download Firefox",
      url: "https://www.mozilla.org/firefox",
      // PlacesUtils.bookmarks.update rejects last modified dates older than
      // the added date.
      lastModified: new Date(Date.now() + DAY_IN_MS),
    });

    yield verifyTrackedItems([fxBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemChanged_itemDates() {
  _("Changes to item dates should be tracked");

  try {
    yield stopTracking();

    _("Insert a bookmark");
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    yield startTracking();

    _("Reset the bookmark's added date");
    // Convert to microseconds for PRTime.
    let dateAdded = (Date.now() - DAY_IN_MS) * 1000;
    PlacesUtils.bookmarks.setItemDateAdded(fx_id, dateAdded);
    yield verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    _("Set the bookmark's last modified date");
    let dateModified = Date.now() * 1000;
    PlacesUtils.bookmarks.setItemLastModified(fx_id, dateModified);
    yield verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemChanged_changeBookmarkURI() {
  _("Changes to bookmark URIs should be tracked");

  try {
    yield stopTracking();

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
      fx_id, BookmarkAnnos.DESCRIPTION_ANNO, "A test description", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);

    yield startTracking();

    _("Change the bookmark's URI");
    PlacesUtils.bookmarks.changeBookmarkURI(fx_id,
      Utils.makeURI("https://www.mozilla.org/firefox"));
    yield verifyTrackedItems([fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemTagged() {
  _("Items tagged using the synchronous API should be tracked");

  try {
    yield stopTracking();

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

    yield startTracking();

    _("Tag the item");
    PlacesUtils.tagging.tagURI(uri, ["foo"]);

    // bookmark should be tracked, folder should not be.
    yield verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemUntagged() {
  _("Items untagged using the synchronous API should be tracked");

  try {
    yield stopTracking();

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

    yield startTracking();

    _("Remove the tag");
    PlacesUtils.tagging.untagURI(uri, ["foo"]);

    yield verifyTrackedItems([fx1GUID, fx2GUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemUntagged() {
  _("Items untagged using the asynchronous API should be tracked");

  try {
    yield stopTracking();

    _("Insert tagged bookmarks");
    let fxBmk1 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });
    let tag = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: "some tag",
    });
    let fxTag = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: tag.guid,
      url: "http://getfirefox.com",
    });

    yield startTracking();

    _("Remove the tag using the async bookmarks API");
    yield PlacesUtils.bookmarks.remove(fxTag.guid);

    yield verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemTagged() {
  _("Items tagged using the asynchronous API should be tracked");

  try {
    yield stopTracking();

    _("Insert untagged bookmarks");
    let folder1 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Folder 1",
    });
    let fxBmk1 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: folder1.guid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let folder2 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "Folder 2",
    });
    // Different parent and title; same URL.
    let fxBmk2 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: folder2.guid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });

    yield startTracking();

    // This will change once tags are moved into a separate table (bug 424160).
    // We specifically test this case because Bookmarks.jsm updates tagged
    // bookmarks and notifies observers.
    _("Insert a tag using the async bookmarks API");
    let tag = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.tagsGuid,
      title: "some tag",
    });

    _("Tag an item using the async bookmarks API");
    yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: tag.guid,
      url: "http://getfirefox.com",
    });

    yield verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemKeywordChanged() {
  _("Keyword changes via the synchronous API should be tracked");

  try {
    yield stopTracking();
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folderGUID = engine._store.GUIDForId(folder);
    _("Track changes to keywords");
    let uri = Utils.makeURI("http://getfirefox.com");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    yield startTracking();

    _("Give the item a keyword");
    PlacesUtils.bookmarks.setKeywordForBookmark(b, "the_keyword");

    // bookmark should be tracked, folder should not be.
    yield verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemKeywordChanged() {
  _("Keyword changes via the asynchronous API should be tracked");

  try {
    yield stopTracking();

    _("Insert two bookmarks with the same URL");
    let fxBmk1 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });

    yield startTracking();

    _("Add a keyword for both items");
    yield PlacesUtils.keywords.insert({
      keyword: "the_keyword",
      url: "http://getfirefox.com",
      postData: "postData",
    });

    yield verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemKeywordDeleted() {
  _("Keyword deletions via the asynchronous API should be tracked");

  try {
    yield stopTracking();

    _("Insert two bookmarks with the same URL and keywords");
    let fxBmk1 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let fxBmk2 = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com",
      title: "Download Firefox",
    });
    yield PlacesUtils.keywords.insert({
      keyword: "the_keyword",
      url: "http://getfirefox.com",
    });

    yield startTracking();

    _("Remove the keyword");
    yield PlacesUtils.keywords.remove("the_keyword");

    yield verifyTrackedItems([fxBmk1.guid, fxBmk2.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemPostDataChanged() {
  _("Post data changes should be tracked");

  try {
    yield stopTracking();

    _("Insert a bookmark");
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    yield startTracking();

    // PlacesUtils.setPostDataForBookmark is deprecated, but still used by
    // PlacesTransactions.NewBookmark.
    _("Post data for the bookmark should be ignored");
    yield PlacesUtils.setPostDataForBookmark(fx_id, "postData");
    yield verifyTrackerEmpty();
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemAnnoChanged() {
  _("Item annotations should be tracked");

  try {
    yield stopTracking();
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let folderGUID = engine._store.GUIDForId(folder);
    _("Track changes to annos.");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    yield startTracking();
    PlacesUtils.annotations.setItemAnnotation(
      b, BookmarkAnnos.DESCRIPTION_ANNO, "A test description", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    // bookmark should be tracked, folder should not.
    yield verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    PlacesUtils.annotations.removeItemAnnotation(b,
      BookmarkAnnos.DESCRIPTION_ANNO);
    yield verifyTrackedItems([bGUID]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemExcluded() {
  _("Items excluded from backups should not be tracked");

  try {
    yield stopTracking();

    _("Create a bookmark");
    let b = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);

    yield startTracking();

    _("Exclude the bookmark from backups");
    PlacesUtils.annotations.setItemAnnotation(
      b, BookmarkAnnos.EXCLUDEBACKUP_ANNO, "Don't back this up", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);

    _("Modify the bookmark");
    PlacesUtils.bookmarks.setItemTitle(b, "Download Firefox");

    _("Excluded items should be ignored");
    yield verifyTrackerEmpty();
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onPageAnnoChanged() {
  _("Page annotations should not be tracked");

  try {
    yield stopTracking();

    _("Insert a bookmark without an annotation");
    let pageURI = Utils.makeURI("http://getfirefox.com");
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      pageURI,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");

    yield startTracking();

    _("Add a page annotation");
    PlacesUtils.annotations.setPageAnnotation(pageURI, "URIProperties/characterSet",
      "UTF-8", 0, PlacesUtils.annotations.EXPIRE_NEVER);
    yield verifyTrackerEmpty();
    yield resetTracker();

    _("Remove the page annotation");
    PlacesUtils.annotations.removePageAnnotation(pageURI,
      "URIProperties/characterSet");
    yield verifyTrackerEmpty();
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onFaviconChanged() {
  _("Favicon changes should not be tracked");

  try {
    yield stopTracking();

    let pageURI = Utils.makeURI("http://getfirefox.com");
    let iconURI = Utils.makeURI("http://getfirefox.com/icon");
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      pageURI,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");

    yield PlacesTestUtils.addVisits(pageURI);

    yield startTracking();

    _("Favicon annotations should be ignored");
    let iconURL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAA" +
      "AAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

    PlacesUtils.favicons.replaceFaviconDataFromDataURL(iconURI, iconURL, 0,
      Services.scriptSecurityManager.getSystemPrincipal());

    yield new Promise(resolve => {
      PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, iconURI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, (iconURI, dataLen, data, mimeType) => {
          resolve();
        },
        Services.scriptSecurityManager.getSystemPrincipal());
    });
    yield verifyTrackerEmpty();
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onLivemarkAdded() {
  _("New livemarks should be tracked");

  try {
    yield startTracking();

    _("Insert a livemark");
    let livemark = yield PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      // Use a local address just in case, to avoid potential aborts for
      // non-local connections.
      feedURI: Utils.makeURI("http://localhost:0"),
    });
    // Prevent the livemark refresh timer from requesting the URI.
    livemark.terminate();

    yield verifyTrackedItems(["menu", livemark.guid]);
    // Three changes: one for the parent, one for creating the livemark
    // folder, and one for setting the "livemark/feedURI" anno on the folder.
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onLivemarkDeleted() {
  _("Deleted livemarks should be tracked");

  try {
    yield stopTracking();

    _("Insert a livemark");
    let livemark = yield PlacesUtils.livemarks.addLivemark({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      feedURI: Utils.makeURI("http://localhost:0"),
    });
    livemark.terminate();

    yield startTracking();

    _("Remove a livemark");
    yield PlacesUtils.livemarks.removeLivemark({
      guid: livemark.guid,
    });

    yield verifyTrackedItems(["menu", livemark.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemMoved() {
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

    yield startTracking();

    // Moving within the folder will just track the folder.
    PlacesUtils.bookmarks.moveItem(
      tb_id, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
    yield verifyTrackedItems(['menu']);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    // Moving a bookmark to a different folder will track the old
    // folder, the new folder and the bookmark.
    PlacesUtils.bookmarks.moveItem(fx_id, PlacesUtils.bookmarks.toolbarFolder,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
    yield verifyTrackedItems(['menu', 'toolbar', fx_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);

  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemMoved_update() {
  _("Items moved via the asynchronous API should be tracked");

  try {
    yield stopTracking();

    let fxBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let tbBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });

    yield startTracking();

    _("Repositioning a bookmark should track the folder");
    yield PlacesUtils.bookmarks.update({
      guid: tbBmk.guid,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: 0,
    });
    yield verifyTrackedItems(['menu']);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    _("Reparenting a bookmark should track both folders and the bookmark");
    yield PlacesUtils.bookmarks.update({
      guid: tbBmk.guid,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    });
    yield verifyTrackedItems(['menu', 'toolbar', tbBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemMoved_reorder() {
  _("Items reordered via the asynchronous API should be tracked");

  try {
    yield stopTracking();

    _("Insert out-of-order bookmarks");
    let fxBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);

    let tbBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });
    _(`Thunderbird GUID: ${tbBmk.guid}`);

    let mozBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://mozilla.org",
      title: "Mozilla",
    });
    _(`Mozilla GUID: ${mozBmk.guid}`);

    yield startTracking();

    _("Reorder bookmarks");
    yield PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.menuGuid,
      [mozBmk.guid, fxBmk.guid, tbBmk.guid]);

    // As with setItemIndex, we should only track the folder if we reorder
    // its children, but we should bump the score for every changed item.
    yield verifyTrackedItems(["menu"]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemMoved_setItemIndex() {
  _("Items with updated indices should be tracked");

  try {
    yield stopTracking();

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

    yield startTracking();

    // PlacesSortFolderByNameTransaction exercises
    // PlacesUtils.bookmarks.setItemIndex.
    let txn = new PlacesSortFolderByNameTransaction(folder_id);

    // We're reordering items within the same folder, so only the folder
    // should be tracked.
    _("Execute the sort folder transaction");
    txn.doTransaction();
    yield verifyTrackedItems([folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    _("Undo the sort folder transaction");
    txn.undoTransaction();
    yield verifyTrackedItems([folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemDeleted_removeFolderTransaction() {
  _("Folders removed in a transaction should be tracked");

  try {
    yield stopTracking();

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

    yield startTracking();

    let txn = PlacesUtils.bookmarks.getRemoveFolderTransaction(folder_id);
    // We haven't executed the transaction yet.
    yield verifyTrackerEmpty();

    _("Execute the remove folder transaction");
    txn.doTransaction();
    yield verifyTrackedItems(["menu", folder_guid, fx_guid, tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 6);
    yield resetTracker();

    _("Undo the remove folder transaction");
    txn.undoTransaction();
    yield verifyTrackedItems(["menu"]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    yield resetTracker();

    // At this point, the restored folder has the same ID, but a different GUID.
    let new_folder_guid = yield PlacesUtils.promiseItemGuid(folder_id);

    _("Redo the transaction");
    txn.redoTransaction();
    yield verifyTrackedItems(["menu", new_folder_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_treeMoved() {
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

    yield startTracking();

    // Move folder 2 to be a sibling of folder1.
    PlacesUtils.bookmarks.moveItem(
      folder2_id, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
    // the menu and both folders should be tracked, the children should not be.
    yield verifyTrackedItems(['menu', folder1_guid, folder2_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemDeleted() {
  _("Bookmarks deleted via the synchronous API should be tracked");

  try {
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);

    yield startTracking();

    // Delete the last item - the item and parent should be tracked.
    PlacesUtils.bookmarks.removeItem(tb_id);

    yield verifyTrackedItems(['menu', tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemDeleted() {
  _("Bookmarks deleted via the asynchronous API should be tracked");

  try {
    yield stopTracking();

    let fxBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    let tbBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });

    yield startTracking();

    _("Delete the first item");
    yield PlacesUtils.bookmarks.remove(fxBmk.guid);

    yield verifyTrackedItems(["menu", fxBmk.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_async_onItemDeleted_eraseEverything() {
  _("Erasing everything should track all deleted items");

  try {
    yield stopTracking();

    let mobileRoot = BookmarkSpecialIds.findMobileRoot(true);
    let mobileGUID = yield PlacesUtils.promiseItemGuid(mobileRoot);

    let fxBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: mobileGUID,
      url: "http://getfirefox.com",
      title: "Get Firefox!",
    });
    _(`Firefox GUID: ${fxBmk.guid}`);
    let tbBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: mobileGUID,
      url: "http://getthunderbird.com",
      title: "Get Thunderbird!",
    });
    _(`Thunderbird GUID: ${tbBmk.guid}`);
    let mozBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://mozilla.org",
      title: "Mozilla",
    });
    _(`Mozilla GUID: ${mozBmk.guid}`);
    let mdnBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://developer.mozilla.org",
      title: "MDN",
    });
    let bugsFolder = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      title: "Bugs",
    });
    let bzBmk = yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: bugsFolder.guid,
      url: "https://bugzilla.mozilla.org",
      title: "Bugzilla",
    });

    yield startTracking();

    yield PlacesUtils.bookmarks.eraseEverything();

    yield verifyTrackedItems(["menu", mozBmk.guid, mdnBmk.guid, "toolbar",
                              bugsFolder.guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 6);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemDeleted_removeFolderChildren() {
  _("Removing a folder's children should track the folder and its children");

  try {
    let mobileRoot = BookmarkSpecialIds.findMobileRoot(true);
    let mobileGUID = engine._store.GUIDForId(mobileRoot);
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      mobileRoot,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    _(`Firefox GUID: ${fx_guid}`);

    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      mobileRoot,
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

    yield startTracking();

    _(`Mobile root ID: ${mobileRoot}`);
    PlacesUtils.bookmarks.removeFolderChildren(mobileRoot);

    yield verifyTrackedItems(["mobile", fx_guid, tb_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

add_task(function* test_onItemDeleted_tree() {
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

    yield startTracking();

    // Delete folder2 - everything we created should be tracked.
    PlacesUtils.bookmarks.removeItem(folder2_id);

    yield verifyTrackedItems([fx_guid, tb_guid, folder1_guid, folder2_guid]);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 6);
  } finally {
    _("Clean up.");
    yield cleanup();
  }
});

function run_test() {
  initTestLogging("Trace");

  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Tracker.Bookmarks").level = Log.Level.Trace;

  run_next_test();
}

