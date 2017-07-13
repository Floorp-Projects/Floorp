/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

const PARENT_ANNO = "sync/parent";

let engine;
let store;
let tracker;

const fxuri = Utils.makeURI("http://getfirefox.com/");
const tburi = Utils.makeURI("http://getthunderbird.com/");

add_task(async function setup() {
  await Service.engineManager.register(BookmarksEngine);

  engine = Service.engineManager.get("bookmarks");
  store = engine._store;
  tracker = engine._tracker;

  // Don't write some persistence files asynchronously.
  tracker.persistChangedIDs = false;
});

add_task(async function test_ignore_specials() {
  _("Ensure that we can't delete bookmark roots.");

  // Belt...
  let record = new BookmarkFolder("bookmarks", "toolbar", "folder");
  record.deleted = true;
  do_check_neq(null, (await store.idForGUID("toolbar")));

  await store.applyIncoming(record);
  await store.deletePending();

  // Ensure that the toolbar exists.
  do_check_neq(null, (await store.idForGUID("toolbar")));

  // This will fail painfully in getItemType if the deletion worked.
  await engine._buildGUIDMap();

  // Braces...
  await store.remove(record);
  await store.deletePending();
  do_check_neq(null, (await store.idForGUID("toolbar")));
  await engine._buildGUIDMap();

  await store.wipe();
});

add_task(async function test_bookmark_create() {
  try {
    _("Ensure the record isn't present yet.");
    let ids = PlacesUtils.bookmarks.getBookmarkIdsForURI(fxuri, {});
    do_check_eq(ids.length, 0);

    _("Let's create a new record.");
    let fxrecord = new Bookmark("bookmarks", "get-firefox1");
    fxrecord.bmkUri        = fxuri.spec;
    fxrecord.description   = "Firefox is awesome.";
    fxrecord.title         = "Get Firefox!";
    fxrecord.tags          = ["firefox", "awesome", "browser"];
    fxrecord.keyword       = "awesome";
    fxrecord.loadInSidebar = false;
    fxrecord.parentName    = "Bookmarks Toolbar";
    fxrecord.parentid      = "toolbar";
    await store.applyIncoming(fxrecord);

    _("Verify it has been created correctly.");
    let id = await store.idForGUID(fxrecord.id);
    do_check_eq((await store.GUIDForId(id)), fxrecord.id);
    do_check_eq(PlacesUtils.bookmarks.getItemType(id),
                PlacesUtils.bookmarks.TYPE_BOOKMARK);
    do_check_true(PlacesUtils.bookmarks.getBookmarkURI(id).equals(fxuri));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(id), fxrecord.title);
    do_check_eq(PlacesUtils.annotations.getItemAnnotation(id, "bookmarkProperties/description"),
                fxrecord.description);
    do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(id),
                PlacesUtils.bookmarks.toolbarFolder);
    do_check_eq(PlacesUtils.bookmarks.getKeywordForBookmark(id), fxrecord.keyword);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = await store.createRecord(fxrecord.id);
    do_check_true(newrecord instanceof Bookmark);
    for (let property of ["type", "bmkUri", "description", "title",
                          "keyword", "parentName", "parentid"]) {
      do_check_eq(newrecord[property], fxrecord[property]);
    }
    do_check_true(Utils.deepEquals(newrecord.tags.sort(),
                                   fxrecord.tags.sort()));

    _("The calculated sort index is based on frecency data.");
    do_check_true(newrecord.sortindex >= 150);

    _("Create a record with some values missing.");
    let tbrecord = new Bookmark("bookmarks", "thunderbird1");
    tbrecord.bmkUri        = tburi.spec;
    tbrecord.parentName    = "Bookmarks Toolbar";
    tbrecord.parentid      = "toolbar";
    await store.applyIncoming(tbrecord);

    _("Verify it has been created correctly.");
    id = await store.idForGUID(tbrecord.id);
    do_check_eq((await store.GUIDForId(id)), tbrecord.id);
    do_check_eq(PlacesUtils.bookmarks.getItemType(id),
                PlacesUtils.bookmarks.TYPE_BOOKMARK);
    do_check_true(PlacesUtils.bookmarks.getBookmarkURI(id).equals(tburi));
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(id), "");
    let error;
    try {
      PlacesUtils.annotations.getItemAnnotation(id, "bookmarkProperties/description");
    } catch (ex) {
      error = ex;
    }
    do_check_eq(error.result, Cr.NS_ERROR_NOT_AVAILABLE);
    do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(id),
                PlacesUtils.bookmarks.toolbarFolder);
    do_check_eq(PlacesUtils.bookmarks.getKeywordForBookmark(id), null);
  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_bookmark_update() {
  try {
    _("Create a bookmark whose values we'll change.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, fxuri,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    PlacesUtils.annotations.setItemAnnotation(
      bmk1_id, "bookmarkProperties/description", "Firefox is awesome.", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    PlacesUtils.bookmarks.setKeywordForBookmark(bmk1_id, "firefox");
    let bmk1_guid = await store.GUIDForId(bmk1_id);

    _("Update the record with some null values.");
    let record = await store.createRecord(bmk1_guid);
    record.title = null;
    record.description = null;
    record.keyword = null;
    record.tags = null;
    await store.applyIncoming(record);

    _("Verify that the values have been cleared.");
    do_check_throws(function() {
      PlacesUtils.annotations.getItemAnnotation(
        bmk1_id, "bookmarkProperties/description");
    }, Cr.NS_ERROR_NOT_AVAILABLE);
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(bmk1_id), "");
    do_check_eq(PlacesUtils.bookmarks.getKeywordForBookmark(bmk1_id), null);
  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_bookmark_createRecord() {
  try {
    _("Create a bookmark without a description or title.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, fxuri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, null);
    let bmk1_guid = await store.GUIDForId(bmk1_id);

    _("Verify that the record is created accordingly.");
    let record = await store.createRecord(bmk1_guid);
    do_check_eq(record.title, "");
    do_check_eq(record.description, null);
    do_check_eq(record.keyword, null);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_folder_create() {
  try {
    _("Create a folder.");
    let folder = new BookmarkFolder("bookmarks", "testfolder-1");
    folder.parentName = "Bookmarks Toolbar";
    folder.parentid   = "toolbar";
    folder.title      = "Test Folder";
    await store.applyIncoming(folder);

    _("Verify it has been created correctly.");
    let id = await store.idForGUID(folder.id);
    do_check_eq(PlacesUtils.bookmarks.getItemType(id),
                PlacesUtils.bookmarks.TYPE_FOLDER);
    do_check_eq(PlacesUtils.bookmarks.getItemTitle(id), folder.title);
    do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(id),
                PlacesUtils.bookmarks.toolbarFolder);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = await store.createRecord(folder.id);
    do_check_true(newrecord instanceof BookmarkFolder);
    for (let property of ["title", "parentName", "parentid"])
      do_check_eq(newrecord[property], folder[property]);

    _("Folders have high sort index to ensure they're synced first.");
    do_check_eq(newrecord.sortindex, 1000000);
  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_folder_createRecord() {
  try {
    _("Create a folder.");
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder1", 0);
    let folder1_guid = await store.GUIDForId(folder1_id);

    _("Create two bookmarks in that folder without assigning them GUIDs.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, fxuri, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");

    _("Create a record for the folder and verify basic properties.");
    let record = await store.createRecord(folder1_guid);
    do_check_true(record instanceof BookmarkFolder);
    do_check_eq(record.title, "Folder1");
    do_check_eq(record.parentid, "toolbar");
    do_check_eq(record.parentName, "Bookmarks Toolbar");

    _("Verify the folder's children. Ensures that the bookmarks were given GUIDs.");
    let bmk1_guid = await store.GUIDForId(bmk1_id);
    let bmk2_guid = await store.GUIDForId(bmk2_id);
    do_check_eq(record.children.length, 2);
    do_check_eq(record.children[0], bmk1_guid);
    do_check_eq(record.children[1], bmk2_guid);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_deleted() {
  try {
    _("Create a bookmark that will be deleted.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, fxuri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = await store.GUIDForId(bmk1_id);

    _("Delete the bookmark through the store.");
    let record = new PlacesItem("bookmarks", bmk1_guid);
    record.deleted = true;
    await store.applyIncoming(record);
    await store.deletePending();
    _("Ensure it has been deleted.");
    let error;
    try {
      PlacesUtils.bookmarks.getBookmarkURI(bmk1_id);
    } catch (ex) {
      error = ex;
    }
    do_check_eq(error.result, Cr.NS_ERROR_ILLEGAL_VALUE);

    let newrec = await store.createRecord(bmk1_guid);
    do_check_eq(newrec.deleted, true);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_move_folder() {
  try {
    _("Create two folders and a bookmark in one of them.");
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder1", 0);
    let folder1_guid = await store.GUIDForId(folder1_id);
    let folder2_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder2", 0);
    let folder2_guid = await store.GUIDForId(folder2_id);
    let bmk_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, fxuri, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk_guid = await store.GUIDForId(bmk_id);

    _("Get a record, reparent it and apply it to the store.");
    let record = await store.createRecord(bmk_guid);
    do_check_eq(record.parentid, folder1_guid);
    record.parentid = folder2_guid;
    await store.applyIncoming(record);

    _("Verify the new parent.");
    let new_folder_id = PlacesUtils.bookmarks.getFolderIdForItem(bmk_id);
    do_check_eq((await store.GUIDForId(new_folder_id)), folder2_guid);
  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_move_order() {
  // Make sure the tracker is turned on.
  Svc.Obs.notify("weave:engine:start-tracking");
  try {
    _("Create two bookmarks");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, fxuri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = await store.GUIDForId(bmk1_id);
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, tburi,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = await store.GUIDForId(bmk2_id);

    _("Verify order.");
    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk1_id), 0);
    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk2_id), 1);
    let toolbar = await store.createRecord("toolbar");
    do_check_eq(toolbar.children.length, 2);
    do_check_eq(toolbar.children[0], bmk1_guid);
    do_check_eq(toolbar.children[1], bmk2_guid);

    _("Move bookmarks around.");
    store._childrenToOrder = {};
    toolbar.children = [bmk2_guid, bmk1_guid];
    await store.applyIncoming(toolbar);
    // Bookmarks engine does this at the end of _processIncoming
    tracker.ignoreAll = true;
    await store._orderChildren();
    tracker.ignoreAll = false;
    delete store._childrenToOrder;

    _("Verify new order.");
    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk2_id), 0);
    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk1_id), 1);

  } finally {
    Svc.Obs.notify("weave:engine:stop-tracking");
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_orphan() {
  try {

    _("Add a new bookmark locally.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder, fxuri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = await store.GUIDForId(bmk1_id);
    do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(bmk1_id),
                PlacesUtils.bookmarks.toolbarFolder);
    let error;
    try {
      PlacesUtils.annotations.getItemAnnotation(bmk1_id, PARENT_ANNO);
    } catch (ex) {
      error = ex;
    }
    do_check_eq(error.result, Cr.NS_ERROR_NOT_AVAILABLE);

    _("Apply a server record that is the same but refers to non-existent folder.");
    let record = await store.createRecord(bmk1_guid);
    record.parentid = "non-existent";
    await store.applyIncoming(record);

    _("Verify that bookmark has been flagged as orphan, has not moved.");
    do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(bmk1_id),
                PlacesUtils.bookmarks.toolbarFolder);
    do_check_eq(PlacesUtils.annotations.getItemAnnotation(bmk1_id, PARENT_ANNO),
                "non-existent");

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_reparentOrphans() {
  try {
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder1", 0);
    let folder1_guid = await store.GUIDForId(folder1_id);

    _("Create a bogus orphan record and write the record back to the store to trigger _reparentOrphans.");
    PlacesUtils.annotations.setItemAnnotation(
      folder1_id, PARENT_ANNO, folder1_guid, 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    let record = await store.createRecord(folder1_guid);
    record.title = "New title for Folder 1";
    store._childrenToOrder = {};
    await store.applyIncoming(record);

    _("Verify that is has been marked as an orphan even though it couldn't be moved into itself.");
    do_check_eq(PlacesUtils.annotations.getItemAnnotation(folder1_id, PARENT_ANNO),
                folder1_guid);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

// Tests Bug 806460, in which query records arrive with empty folder
// names and missing bookmark URIs.
add_task(async function test_empty_query_doesnt_die() {
  let record = new BookmarkQuery("bookmarks", "8xoDGqKrXf1P");
  record.folderName    = "";
  record.queryId       = "";
  record.parentName    = "Toolbar";
  record.parentid      = "toolbar";

  // These should not throw.
  await store.applyIncoming(record);

  delete record.folderName;
  await store.applyIncoming(record);

});

function assertDeleted(id) {
  let error;
  try {
    PlacesUtils.bookmarks.getItemType(id);
  } catch (e) {
    error = e;
  }
  equal(error.result, Cr.NS_ERROR_ILLEGAL_VALUE)
}

add_task(async function test_delete_buffering() {
  await store.wipe();
  await PlacesTestUtils.markBookmarksAsSynced();

  try {
    _("Create a folder with two bookmarks.");
    let folder = new BookmarkFolder("bookmarks", "testfolder-1");
    folder.parentName = "Bookmarks Toolbar";
    folder.parentid = "toolbar";
    folder.title = "Test Folder";
    await store.applyIncoming(folder);


    let fxRecord = new Bookmark("bookmarks", "get-firefox1");
    fxRecord.bmkUri        = fxuri.spec;
    fxRecord.title         = "Get Firefox!";
    fxRecord.parentName    = "Test Folder";
    fxRecord.parentid      = "testfolder-1";

    let tbRecord = new Bookmark("bookmarks", "get-tndrbrd1");
    tbRecord.bmkUri        = tburi.spec;
    tbRecord.title         = "Get Thunderbird!";
    tbRecord.parentName    = "Test Folder";
    tbRecord.parentid      = "testfolder-1";

    await store.applyIncoming(fxRecord);
    await store.applyIncoming(tbRecord);

    let folderId = await store.idForGUID(folder.id);
    let fxRecordId = await store.idForGUID(fxRecord.id);
    let tbRecordId = await store.idForGUID(tbRecord.id);

    _("Check everything was created correctly.");

    equal(PlacesUtils.bookmarks.getItemType(fxRecordId),
          PlacesUtils.bookmarks.TYPE_BOOKMARK);
    equal(PlacesUtils.bookmarks.getItemType(tbRecordId),
          PlacesUtils.bookmarks.TYPE_BOOKMARK);
    equal(PlacesUtils.bookmarks.getItemType(folderId),
          PlacesUtils.bookmarks.TYPE_FOLDER);

    equal(PlacesUtils.bookmarks.getFolderIdForItem(fxRecordId), folderId);
    equal(PlacesUtils.bookmarks.getFolderIdForItem(tbRecordId), folderId);
    equal(PlacesUtils.bookmarks.getFolderIdForItem(folderId),
          PlacesUtils.bookmarks.toolbarFolder);

    _("Delete the folder and one bookmark.");

    let deleteFolder = new PlacesItem("bookmarks", "testfolder-1");
    deleteFolder.deleted = true;

    let deleteFxRecord = new PlacesItem("bookmarks", "get-firefox1");
    deleteFxRecord.deleted = true;

    await store.applyIncoming(deleteFolder);
    await store.applyIncoming(deleteFxRecord);

    _("Check that we haven't deleted them yet, but that the deletions are queued");
    // these will throw if we've deleted them
    equal(PlacesUtils.bookmarks.getItemType(fxRecordId),
           PlacesUtils.bookmarks.TYPE_BOOKMARK);

    equal(PlacesUtils.bookmarks.getItemType(folderId),
           PlacesUtils.bookmarks.TYPE_FOLDER);

    equal(PlacesUtils.bookmarks.getFolderIdForItem(fxRecordId), folderId);

    ok(store._itemsToDelete.has(folder.id));
    ok(store._itemsToDelete.has(fxRecord.id));
    ok(!store._itemsToDelete.has(tbRecord.id));

    _("Process pending deletions and ensure that the right things are deleted.");
    let newChangeRecords = await store.deletePending();

    deepEqual(Object.keys(newChangeRecords).sort(), ["get-tndrbrd1", "toolbar"]);

    assertDeleted(fxRecordId);
    assertDeleted(folderId);

    ok(!store._itemsToDelete.has(folder.id));
    ok(!store._itemsToDelete.has(fxRecord.id));

    equal(PlacesUtils.bookmarks.getFolderIdForItem(tbRecordId),
          PlacesUtils.bookmarks.toolbarFolder);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});


function run_test() {
  initTestLogging("Trace");
  run_next_test();
}
