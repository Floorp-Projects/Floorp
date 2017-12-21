/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

add_task(async function test_ignore_specials() {
  _("Ensure that we can't delete bookmark roots.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  // Belt...
  let record = new BookmarkFolder("bookmarks", "toolbar", "folder");
  record.deleted = true;
  Assert.notEqual(null, (await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.toolbarGuid)));

  await store.applyIncoming(record);
  await store.deletePending();

  // Ensure that the toolbar exists.
  Assert.notEqual(null, (await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.toolbarGuid)));

  // This will fail to build the local tree if the deletion worked.
  await engine._buildGUIDMap();

  // Braces...
  await store.remove(record);
  await store.deletePending();
  Assert.notEqual(null, (await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.toolbarGuid)));
  await engine._buildGUIDMap();

  await store.wipe();

  await engine.finalize();
});

add_task(async function test_bookmark_create() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    _("Ensure the record isn't present yet.");
    let item = await PlacesUtils.bookmarks.fetch({
      url: "http://getfirefox.com/",
    });
    Assert.equal(null, item);

    _("Let's create a new record.");
    let fxrecord = new Bookmark("bookmarks", "get-firefox1");
    fxrecord.bmkUri        = "http://getfirefox.com/";
    fxrecord.description   = "Firefox is awesome.";
    fxrecord.title         = "Get Firefox!";
    fxrecord.tags          = ["firefox", "awesome", "browser"];
    fxrecord.keyword       = "awesome";
    fxrecord.loadInSidebar = false;
    fxrecord.parentName    = "Bookmarks Toolbar";
    fxrecord.parentid      = "toolbar";
    await store.applyIncoming(fxrecord);

    _("Verify it has been created correctly.");
    item = await PlacesUtils.bookmarks.fetch(fxrecord.id);
    Assert.equal(item.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    Assert.equal(item.url.href, "http://getfirefox.com/");
    Assert.equal(item.title, fxrecord.title);
    let id = await PlacesUtils.promiseItemId(item.guid);
    let description = PlacesUtils.annotations.getItemAnnotation(id,
      PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO);
    Assert.equal(description, fxrecord.description);
    Assert.equal(item.parentGuid, PlacesUtils.bookmarks.toolbarGuid);
    let keyword = await PlacesUtils.keywords.fetch(fxrecord.keyword);
    Assert.equal(keyword.url.href, "http://getfirefox.com/");

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = await store.createRecord(fxrecord.id);
    Assert.ok(newrecord instanceof Bookmark);
    for (let property of ["type", "bmkUri", "description", "title",
                          "keyword", "parentName", "parentid"]) {
      Assert.equal(newrecord[property], fxrecord[property]);
    }
    Assert.ok(Utils.deepEquals(newrecord.tags.sort(),
                               fxrecord.tags.sort()));

    _("The calculated sort index is based on frecency data.");
    Assert.ok(newrecord.sortindex >= 150);

    _("Create a record with some values missing.");
    let tbrecord = new Bookmark("bookmarks", "thunderbird1");
    tbrecord.bmkUri        = "http://getthunderbird.com/";
    tbrecord.parentName    = "Bookmarks Toolbar";
    tbrecord.parentid      = "toolbar";
    await store.applyIncoming(tbrecord);

    _("Verify it has been created correctly.");
    item = await PlacesUtils.bookmarks.fetch(tbrecord.id);
    id = await PlacesUtils.promiseItemId(item.guid);
    Assert.equal(item.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    Assert.equal(item.url.href, "http://getthunderbird.com/");
    Assert.equal(item.title, "");
    do_check_throws(function() {
      PlacesUtils.annotations.getItemAnnotation(id,
        PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO);
    }, Cr.NS_ERROR_NOT_AVAILABLE);
    Assert.equal(item.parentGuid, PlacesUtils.bookmarks.toolbarGuid);
    keyword = await PlacesUtils.keywords.fetch({
      url: "http://getthunderbird.com/",
    });
    Assert.equal(null, keyword);
  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_bookmark_update() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    _("Create a bookmark whose values we'll change.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });
    let bmk1_id = await PlacesUtils.promiseItemId(bmk1.guid);
    PlacesUtils.annotations.setItemAnnotation(
      bmk1_id, PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO,
      "Firefox is awesome.", 0, PlacesUtils.annotations.EXPIRE_NEVER);
    await PlacesUtils.keywords.insert({
      url: "http://getfirefox.com/",
      keyword: "firefox",
    });

    _("Update the record with some null values.");
    let record = await store.createRecord(bmk1.guid);
    record.title = null;
    record.description = null;
    record.keyword = null;
    record.tags = null;
    await store.applyIncoming(record);

    _("Verify that the values have been cleared.");
    do_check_throws(function() {
      PlacesUtils.annotations.getItemAnnotation(
        bmk1_id, PlacesSyncUtils.bookmarks.DESCRIPTION_ANNO);
    }, Cr.NS_ERROR_NOT_AVAILABLE);
    let item = await PlacesUtils.bookmarks.fetch(bmk1.guid);
    Assert.equal(item.title, "");
    let keyword = await PlacesUtils.keywords.fetch({
      url: "http://getfirefox.com/",
    });
    Assert.equal(null, keyword);
  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_bookmark_createRecord() {
  let engine = Service.engineManager.get("bookmarks");
  let store = engine._store;

  try {
    _("Create a bookmark without a description or title.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com/",
    });

    _("Verify that the record is created accordingly.");
    let record = await store.createRecord(bmk1.guid);
    Assert.equal(record.title, "");
    Assert.equal(record.description, null);
    Assert.equal(record.keyword, null);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_folder_create() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    _("Create a folder.");
    let folder = new BookmarkFolder("bookmarks", "testfolder-1");
    folder.parentName = "Bookmarks Toolbar";
    folder.parentid   = "toolbar";
    folder.title      = "Test Folder";
    await store.applyIncoming(folder);

    _("Verify it has been created correctly.");
    let item = await PlacesUtils.bookmarks.fetch(folder.id);
    Assert.equal(item.type, PlacesUtils.bookmarks.TYPE_FOLDER);
    Assert.equal(item.title, folder.title);
    Assert.equal(item.parentGuid, PlacesUtils.bookmarks.toolbarGuid);

    _("Have the store create a new record object. Verify that it has the same data.");
    let newrecord = await store.createRecord(folder.id);
    Assert.ok(newrecord instanceof BookmarkFolder);
    for (let property of ["title", "parentName", "parentid"]) {
      Assert.equal(newrecord[property], folder[property]);
    }

    _("Folders have high sort index to ensure they're synced first.");
    Assert.equal(newrecord.sortindex, 1000000);
  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_folder_createRecord() {
  let engine = Service.engineManager.get("bookmarks");
  let store = engine._store;

  try {
    _("Create a folder.");
    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder1",
    });

    _("Create two bookmarks in that folder without assigning them GUIDs.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });
    let bmk2 = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      url: "http://getthunderbird.com/",
      title: "Get Thunderbird!",
    });

    _("Create a record for the folder and verify basic properties.");
    let record = await store.createRecord(folder1.guid);
    Assert.ok(record instanceof BookmarkFolder);
    Assert.equal(record.title, "Folder1");
    Assert.equal(record.parentid, "toolbar");
    Assert.equal(record.parentName, "Bookmarks Toolbar");

    _("Verify the folder's children. Ensures that the bookmarks were given GUIDs.");
    Assert.deepEqual(record.children, [bmk1.guid, bmk2.guid]);

  } finally {
    _("Clean up.");
    await store.wipe();
  }
});

add_task(async function test_deleted() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    _("Create a bookmark that will be deleted.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });

    _("Delete the bookmark through the store.");
    let record = new PlacesItem("bookmarks", bmk1.guid);
    record.deleted = true;
    await store.applyIncoming(record);
    await store.deletePending();
    _("Ensure it has been deleted.");
    let item = await PlacesUtils.bookmarks.fetch(bmk1.guid);
    Assert.equal(null, item);

    let newrec = await store.createRecord(bmk1.guid);
    Assert.equal(newrec.deleted, true);

  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_move_folder() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    _("Create two folders and a bookmark in one of them.");
    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder1",
    });
    let folder2 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder2",
    });
    let bmk = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });

    _("Get a record, reparent it and apply it to the store.");
    let record = await store.createRecord(bmk.guid);
    Assert.equal(record.parentid, folder1.guid);
    record.parentid = folder2.guid;
    await store.applyIncoming(record);

    _("Verify the new parent.");
    let movedBmk = await PlacesUtils.bookmarks.fetch(bmk.guid);
    Assert.equal(movedBmk.parentGuid, folder2.guid);
  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_move_order() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;
  let tracker = engine._tracker;

  // Make sure the tracker is turned on.
  Svc.Obs.notify("weave:engine:start-tracking");
  try {
    _("Create two bookmarks");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });
    let bmk2 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getthunderbird.com/",
      title: "Get Thunderbird!",
    });

    _("Verify order.");
    let childIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      "toolbar");
    Assert.deepEqual(childIds, [bmk1.guid, bmk2.guid]);
    let toolbar = await store.createRecord("toolbar");
    Assert.deepEqual(toolbar.children, [bmk1.guid, bmk2.guid]);

    _("Move bookmarks around.");
    store._childrenToOrder = {};
    toolbar.children = [bmk2.guid, bmk1.guid];
    await store.applyIncoming(toolbar);
    // Bookmarks engine does this at the end of _processIncoming
    tracker.ignoreAll = true;
    await store._orderChildren();
    tracker.ignoreAll = false;
    delete store._childrenToOrder;

    _("Verify new order.");
    let newChildIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      "toolbar");
    Assert.deepEqual(newChildIds, [bmk2.guid, bmk1.guid]);

  } finally {
    Svc.Obs.notify("weave:engine:stop-tracking");
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_orphan() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {

    _("Add a new bookmark locally.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });
    let bmk1_id = await PlacesUtils.promiseItemId(bmk1.guid);
    do_check_throws(function() {
      PlacesUtils.annotations.getItemAnnotation(bmk1_id,
        PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO);
    }, Cr.NS_ERROR_NOT_AVAILABLE);

    _("Apply a server record that is the same but refers to non-existent folder.");
    let record = await store.createRecord(bmk1.guid);
    record.parentid = "non-existent";
    await store.applyIncoming(record);

    _("Verify that bookmark has been flagged as orphan, has not moved.");
    let item = await PlacesUtils.bookmarks.fetch(bmk1.guid);
    Assert.equal(item.parentGuid, PlacesUtils.bookmarks.toolbarGuid);
    let orphanAnno = PlacesUtils.annotations.getItemAnnotation(bmk1_id,
      PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO);
    Assert.equal(orphanAnno, "non-existent");

  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

add_task(async function test_reparentOrphans() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  try {
    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder1",
    });
    let folder1_id = await PlacesUtils.promiseItemId(folder1.guid);

    _("Create a bogus orphan record and write the record back to the store to trigger _reparentOrphans.");
    PlacesUtils.annotations.setItemAnnotation(
      folder1_id, PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO, folder1.guid, 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    let record = await store.createRecord(folder1.guid);
    record.title = "New title for Folder 1";
    store._childrenToOrder = {};
    await store.applyIncoming(record);

    _("Verify that is has been marked as an orphan even though it couldn't be moved into itself.");
    let orphanAnno = PlacesUtils.annotations.getItemAnnotation(folder1_id,
      PlacesSyncUtils.bookmarks.SYNC_PARENT_ANNO);
    Assert.equal(orphanAnno, folder1.guid);

  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});

// Tests Bug 806460, in which query records arrive with empty folder
// names and missing bookmark URIs.
add_task(async function test_empty_query_doesnt_die() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  let record = new BookmarkQuery("bookmarks", "8xoDGqKrXf1P");
  record.folderName    = "";
  record.queryId       = "";
  record.parentName    = "Toolbar";
  record.parentid      = "toolbar";

  // These should not throw.
  await store.applyIncoming(record);

  delete record.folderName;
  await store.applyIncoming(record);

  await engine.finalize();
});

async function assertDeleted(guid) {
  let item = await PlacesUtils.bookmarks.fetch(guid);
  ok(!item);
}

add_task(async function test_delete_buffering() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

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
    fxRecord.bmkUri        = "http://getfirefox.com/";
    fxRecord.title         = "Get Firefox!";
    fxRecord.parentName    = "Test Folder";
    fxRecord.parentid      = "testfolder-1";

    let tbRecord = new Bookmark("bookmarks", "get-tndrbrd1");
    tbRecord.bmkUri        = "http://getthunderbird.com";
    tbRecord.title         = "Get Thunderbird!";
    tbRecord.parentName    = "Test Folder";
    tbRecord.parentid      = "testfolder-1";

    await store.applyIncoming(fxRecord);
    await store.applyIncoming(tbRecord);

    _("Check everything was created correctly.");

    let folderItem = await PlacesUtils.bookmarks.fetch(folder.id);
    let fxItem = await PlacesUtils.bookmarks.fetch(fxRecord.id);
    let tbItem = await PlacesUtils.bookmarks.fetch(tbRecord.id);

    equal(fxItem.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    equal(tbItem.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    equal(folderItem.type, PlacesUtils.bookmarks.TYPE_FOLDER);

    equal(fxItem.parentGuid, folderItem.guid);
    equal(tbItem.parentGuid, folderItem.guid);
    equal(folderItem.parentGuid, PlacesUtils.bookmarks.toolbarGuid);

    _("Delete the folder and one bookmark.");

    let deleteFolder = new PlacesItem("bookmarks", "testfolder-1");
    deleteFolder.deleted = true;

    let deleteFxRecord = new PlacesItem("bookmarks", "get-firefox1");
    deleteFxRecord.deleted = true;

    await store.applyIncoming(deleteFolder);
    await store.applyIncoming(deleteFxRecord);

    _("Check that we haven't deleted them yet, but that the deletions are queued");
    // these will return `null` if we've deleted them
    fxItem = await PlacesUtils.bookmarks.fetch(fxRecord.id);
    ok(fxItem);

    folderItem = await PlacesUtils.bookmarks.fetch(folder.id);
    ok(folderItem);

    equal(fxItem.parentGuid, folderItem.guid);

    ok(store._itemsToDelete.has(folder.id));
    ok(store._itemsToDelete.has(fxRecord.id));
    ok(!store._itemsToDelete.has(tbRecord.id));

    _("Process pending deletions and ensure that the right things are deleted.");
    let newChangeRecords = await store.deletePending();

    deepEqual(Object.keys(newChangeRecords).sort(), ["get-tndrbrd1", "toolbar"]);

    await assertDeleted(fxItem.guid);
    await assertDeleted(folderItem.guid);

    ok(!store._itemsToDelete.has(folder.id));
    ok(!store._itemsToDelete.has(fxRecord.id));

    tbItem = await PlacesUtils.bookmarks.fetch(tbRecord.id);
    equal(tbItem.parentGuid, PlacesUtils.bookmarks.toolbarGuid);

  } finally {
    _("Clean up.");
    await store.wipe();
    await engine.finalize();
  }
});
