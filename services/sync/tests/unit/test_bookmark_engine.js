/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

initTestLogging("Trace");

async function fetchAllSyncIds() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(`
    WITH RECURSIVE
    syncedItems(id, guid) AS (
      SELECT b.id, b.guid FROM moz_bookmarks b
      WHERE b.guid IN ('menu________', 'toolbar_____', 'unfiled_____',
                       'mobile______')
      UNION ALL
      SELECT b.id, b.guid FROM moz_bookmarks b
      JOIN syncedItems s ON b.parent = s.id
    )
    SELECT guid FROM syncedItems`);
  let syncIds = new Set();
  for (let row of rows) {
    let syncId = PlacesSyncUtils.bookmarks.guidToSyncId(
      row.getResultByName("guid"));
    syncIds.add(syncId);
  }
  return syncIds;
}

add_task(async function test_delete_invalid_roots_from_server() {
  _("Ensure that we delete the Places and Reading List roots from the server.");

  let engine  = new BookmarksEngine(Service);
  let store   = engine._store;
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    collection.insert("places", encryptPayload(store.createRecord("places").cleartext));

    let listBmk = new Bookmark("bookmarks", Utils.makeGUID());
    listBmk.bmkUri = "https://example.com";
    listBmk.title = "Example reading list entry";
    listBmk.parentName = "Reading List";
    listBmk.parentid = "readinglist";
    collection.insert(listBmk.id, encryptPayload(listBmk.cleartext));

    let readingList = new BookmarkFolder("bookmarks", "readinglist");
    readingList.title = "Reading List";
    readingList.children = [listBmk.id];
    readingList.parentName = "";
    readingList.parentid = "places";
    collection.insert("readinglist", encryptPayload(readingList.cleartext));

    let newBmk = new Bookmark("bookmarks", Utils.makeGUID());
    newBmk.bmkUri = "http://getfirefox.com";
    newBmk.title = "Get Firefox!";
    newBmk.parentName = "Bookmarks Toolbar";
    newBmk.parentid = "toolbar";
    collection.insert(newBmk.id, encryptPayload(newBmk.cleartext));

    deepEqual(collection.keys().sort(), ["places", "readinglist", listBmk.id, newBmk.id].sort(),
      "Should store Places root, reading list items, and new bookmark on server");

    await sync_engine_and_validate_telem(engine, false);

    ok(!store.itemExists("readinglist"), "Should not apply Reading List root");
    ok(!store.itemExists(listBmk.id), "Should not apply items in Reading List");
    ok(store.itemExists(newBmk.id), "Should apply new bookmark");

    deepEqual(collection.keys().sort(), ["menu", "mobile", "toolbar", "unfiled", newBmk.id].sort(),
      "Should remove Places root and reading list items from server; upload local roots");
  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await promiseStopServer(server);
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
});

add_task(async function bad_record_allIDs() {
  let server = new SyncServer();
  server.start();
  await SyncTestingInfrastructure(server);

  _("Ensure that bad Places queries don't cause an error in getAllIDs.");
  let badRecordID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder,
      Utils.makeURI("place:folder=1138"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      null);

  do_check_true(badRecordID > 0);
  _("Record is " + badRecordID);
  _("Type: " + PlacesUtils.bookmarks.getItemType(badRecordID));

  _("Fetching all IDs.");
  let all = await fetchAllSyncIds();

  _("All IDs: " + JSON.stringify([...all]));
  do_check_true(all.has("menu"));
  do_check_true(all.has("toolbar"));

  _("Clean up.");
  PlacesUtils.bookmarks.removeItem(badRecordID);
  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_processIncoming_error_orderChildren() {
  _("Ensure that _orderChildren() is called even when _processIncoming() throws an error.");

  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  try {

    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);

    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let tburi = Utils.makeURI("http://getthunderbird.com/");

    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, fxuri, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");

    // Create a server record for folder1 where we flip the order of
    // the children.
    let folder1_payload = store.createRecord(folder1_guid).cleartext;
    folder1_payload.children.reverse();
    collection.insert(folder1_guid, encryptPayload(folder1_payload));

    // Create a bogus record that when synced down will provoke a
    // network error which in turn provokes an exception in _processIncoming.
    const BOGUS_GUID = "zzzzzzzzzzzz";
    let bogus_record = collection.insert(BOGUS_GUID, "I'm a bogus record!");
    bogus_record.get = function get() {
      throw "Sync this!";
    };

    // Make the 10 minutes old so it will only be synced in the toFetch phase.
    bogus_record.modified = Date.now() / 1000 - 60 * 10;
    engine.lastSync = Date.now() / 1000 - 60;
    engine.toFetch = [BOGUS_GUID];

    let error;
    try {
      await sync_engine_and_validate_telem(engine, true)
    } catch (ex) {
      error = ex;
    }
    ok(!!error);

    // Verify that the bookmark order has been applied.
    let new_children = store.createRecord(folder1_guid).children;
    do_check_eq(new_children.length, 2);
    do_check_eq(new_children[0], folder1_payload.children[0]);
    do_check_eq(new_children[1], folder1_payload.children[1]);

    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk1_id), 1);
    do_check_eq(PlacesUtils.bookmarks.getItemIndex(bmk2_id), 0);

  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

add_task(async function test_restorePromptsReupload() {
  _("Ensure that restoring from a backup will reupload all records.");
  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  Svc.Obs.notify("weave:engine:start-tracking");   // We skip usual startup...

  try {

    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);
    _("Folder 1: " + folder1_id + ", " + folder1_guid);

    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let tburi = Utils.makeURI("http://getthunderbird.com/");

    _("Create a single record.");
    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, fxuri, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = store.GUIDForId(bmk1_id);
    _("Get Firefox!: " + bmk1_id + ", " + bmk1_guid);


    let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
      .getService(Ci.nsIProperties);

    let backupFile = dirSvc.get("TmpD", Ci.nsILocalFile);

    _("Make a backup.");
    backupFile.append("t_b_e_" + Date.now() + ".json");

    _("Backing up to file " + backupFile.path);
    await BookmarkJSONUtils.exportToFile(backupFile.path);

    _("Create a different record and sync.");
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);
    _("Get Thunderbird!: " + bmk2_id + ", " + bmk2_guid);

    PlacesUtils.bookmarks.removeItem(bmk1_id);

    let error;
    try {
      await sync_engine_and_validate_telem(engine, false);
    } catch (ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    do_check_true(!error);

    _("Verify that there's only one bookmark on the server, and it's Thunderbird.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let wbos = collection.keys(function(id) {
      return ["menu", "toolbar", "mobile", "unfiled", folder1_guid].indexOf(id) == -1;
    });
    do_check_eq(wbos.length, 1);
    do_check_eq(wbos[0], bmk2_guid);

    _("Now restore from a backup.");
    await BookmarkJSONUtils.importFromFile(backupFile, true);

    _("Ensure we have the bookmarks we expect locally.");
    let guids = await fetchAllSyncIds();
    _("GUIDs: " + JSON.stringify([...guids]));
    let found = false;
    let count = 0;
    let newFX;
    for (let guid of guids) {
      count++;
      let id = store.idForGUID(guid, true);
      // Only one bookmark, so _all_ should be Firefox!
      if (PlacesUtils.bookmarks.getItemType(id) == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
        let uri = PlacesUtils.bookmarks.getBookmarkURI(id);
        _("Found URI " + uri.spec + " for GUID " + guid);
        do_check_eq(uri.spec, fxuri.spec);
        newFX = guid;   // Save the new GUID after restore.
        found = true;   // Only runs if the above check passes.
      }
    }
    _("We found it: " + found);
    do_check_true(found);

    _("Have the correct number of IDs locally, too.");
    do_check_eq(count, ["menu", "toolbar", "mobile", "unfiled", folder1_id, bmk1_id].length);

    _("Sync again. This'll wipe bookmarks from the server.");
    try {
      await sync_engine_and_validate_telem(engine, false);
    } catch (ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    do_check_true(!error);

    _("Verify that there's only one bookmark on the server, and it's Firefox.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let payloads     = server.user("foo").collection("bookmarks").payloads();
    let bookmarkWBOs = payloads.filter(function(wbo) {
                         return wbo.type == "bookmark";
                       });
    let folderWBOs   = payloads.filter(function(wbo) {
                         return ((wbo.type == "folder") &&
                                 (wbo.id != "menu") &&
                                 (wbo.id != "toolbar") &&
                                 (wbo.id != "unfiled") &&
                                 (wbo.id != "mobile"));
                       });

    do_check_eq(bookmarkWBOs.length, 1);
    do_check_eq(bookmarkWBOs[0].id, newFX);
    do_check_eq(bookmarkWBOs[0].bmkUri, fxuri.spec);
    do_check_eq(bookmarkWBOs[0].title, "Get Firefox!");

    _("Our old friend Folder 1 is still in play.");
    do_check_eq(folderWBOs.length, 1);
    do_check_eq(folderWBOs[0].title, "Folder 1");

  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

function FakeRecord(constructor, r) {
  constructor.call(this, "bookmarks", r.id);
  for (let x in r) {
    this[x] = r[x];
  }
  // Borrow the constructor's conversion functions.
  this.toSyncBookmark = constructor.prototype.toSyncBookmark;
}

// Bug 632287.
add_task(async function test_mismatched_types() {
  _("Ensure that handling a record that changes type causes deletion " +
    "then re-adding.");

  let oldRecord = {
    "id": "l1nZZXfB8nC7",
    "type": "folder",
    "parentName": "Bookmarks Toolbar",
    "title": "Innerst i Sneglehode",
    "description": null,
    "parentid": "toolbar"
  };
  oldRecord.cleartext = oldRecord;

  let newRecord = {
    "id": "l1nZZXfB8nC7",
    "type": "livemark",
    "siteUri": "http://sneglehode.wordpress.com/",
    "feedUri": "http://sneglehode.wordpress.com/feed/",
    "parentName": "Bookmarks Toolbar",
    "title": "Innerst i Sneglehode",
    "description": null,
    "children":
      ["HCRq40Rnxhrd", "YeyWCV1RVsYw", "GCceVZMhvMbP", "sYi2hevdArlF",
       "vjbZlPlSyGY8", "UtjUhVyrpeG6", "rVq8WMG2wfZI", "Lx0tcy43ZKhZ",
       "oT74WwV8_j4P", "IztsItWVSo3-"],
    "parentid": "toolbar"
  };
  newRecord.cleartext = newRecord;

  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  _("GUID: " + store.GUIDForId(6, true));

  try {
    let bms = PlacesUtils.bookmarks;
    let oldR = new FakeRecord(BookmarkFolder, oldRecord);
    let newR = new FakeRecord(Livemark, newRecord);
    oldR.parentid = PlacesUtils.bookmarks.toolbarGuid;
    newR.parentid = PlacesUtils.bookmarks.toolbarGuid;

    store.applyIncoming(oldR);
    _("Applied old. It's a folder.");
    let oldID = store.idForGUID(oldR.id);
    _("Old ID: " + oldID);
    do_check_eq(bms.getItemType(oldID), bms.TYPE_FOLDER);
    do_check_false(PlacesUtils.annotations
                              .itemHasAnnotation(oldID, PlacesUtils.LMANNO_FEEDURI));

    store.applyIncoming(newR);
    let newID = store.idForGUID(newR.id);
    _("New ID: " + newID);

    _("Applied new. It's a livemark.");
    do_check_eq(bms.getItemType(newID), bms.TYPE_FOLDER);
    do_check_true(PlacesUtils.annotations
                             .itemHasAnnotation(newID, PlacesUtils.LMANNO_FEEDURI));

  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

add_task(async function test_bookmark_guidMap_fail() {
  _("Ensure that failures building the GUID map cause early death.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  let server = serverForFoo(engine);
  let coll   = server.user("foo").collection("bookmarks");
  await SyncTestingInfrastructure(server);

  // Add one item to the server.
  let itemID = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
  let itemGUID    = store.GUIDForId(itemID);
  let itemPayload = store.createRecord(itemGUID).cleartext;
  coll.insert(itemGUID, encryptPayload(itemPayload));

  engine.lastSync = 1;   // So we don't back up.

  // Make building the GUID map fail.

  let pbt = PlacesUtils.promiseBookmarksTree;
  PlacesUtils.promiseBookmarksTree = function() { return Promise.reject("Nooo"); };

  // Ensure that we throw when accessing _guidMap.
  engine._syncStartup();
  _("No error.");
  do_check_false(engine._guidMapFailed);

  _("We get an error if building _guidMap fails in use.");
  let err;
  try {
    _(engine._guidMap);
  } catch (ex) {
    err = ex;
  }
  do_check_eq(err.code, Engine.prototype.eEngineAbortApplyIncoming);
  do_check_eq(err.cause, "Nooo");

  _("We get an error and abort during processIncoming.");
  err = undefined;
  try {
    engine._processIncoming();
  } catch (ex) {
    err = ex;
  }
  do_check_eq(err, "Nooo");

  PlacesUtils.promiseBookmarksTree = pbt;
  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_bookmark_tag_but_no_uri() {
  _("Ensure that a bookmark record with tags, but no URI, doesn't throw an exception.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  // We're simply checking that no exception is thrown, so
  // no actual checks in this test.

  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    syncId: Utils.makeGUID(),
    parentSyncId: "toolbar",
    url: "http://example.com",
    tags: ["foo"],
  });
  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    syncId: Utils.makeGUID(),
    parentSyncId: "toolbar",
    url: "http://example.org",
    tags: null,
  });
  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    syncId: Utils.makeGUID(),
    url: "about:fake",
    parentSyncId: "toolbar",
    tags: null,
  });

  let record = new FakeRecord(BookmarkFolder, {
    parentid:    "toolbar",
    id:          Utils.makeGUID(),
    description: "",
    tags:        ["foo"],
    title:       "Taggy tag",
    type:        "folder"
  });

  store.create(record);
  record.tags = ["bar"];
  store.update(record);
});

add_task(async function test_misreconciled_root() {
  _("Ensure that we don't reconcile an arbitrary record with a root.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;
  let server = serverForFoo(engine);

  // Log real hard for this test.
  store._log.trace = store._log.debug;
  engine._log.trace = engine._log.debug;

  engine._syncStartup();

  // Let's find out where the toolbar is right now.
  let toolbarBefore = store.createRecord("toolbar", "bookmarks");
  let toolbarIDBefore = store.idForGUID("toolbar");
  do_check_neq(-1, toolbarIDBefore);

  let parentGUIDBefore = toolbarBefore.parentid;
  let parentIDBefore = store.idForGUID(parentGUIDBefore);
  do_check_neq(-1, parentIDBefore);
  do_check_eq("string", typeof(parentGUIDBefore));

  _("Current parent: " + parentGUIDBefore + " (" + parentIDBefore + ").");

  let to_apply = {
    id: "zzzzzzzzzzzz",
    type: "folder",
    title: "Bookmarks Toolbar",
    description: "Now you're for it.",
    parentName: "",
    parentid: "mobile",   // Why not?
    children: [],
  };

  let rec = new FakeRecord(BookmarkFolder, to_apply);

  _("Applying record.");
  store.applyIncoming(rec);

  // Ensure that afterwards, toolbar is still there.
  // As of 2012-12-05, this only passes because Places doesn't use "toolbar" as
  // the real GUID, instead using a generated one. Sync does the translation.
  let toolbarAfter = store.createRecord("toolbar", "bookmarks");
  let parentGUIDAfter = toolbarAfter.parentid;
  let parentIDAfter = store.idForGUID(parentGUIDAfter);
  do_check_eq(store.GUIDForId(toolbarIDBefore), "toolbar");
  do_check_eq(parentGUIDBefore, parentGUIDAfter);
  do_check_eq(parentIDBefore, parentIDAfter);

  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_sync_dateAdded() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  // TODO: Avoid random orange (bug 1374599), this is only necessary
  // intermittently - reset the last sync date so that we'll get all bookmarks.
  engine.lastSync = 1;

  Svc.Obs.notify("weave:engine:start-tracking");   // We skip usual startup...

  // Just matters that it's in the past, not how far.
  let now = Date.now();
  let oneYearMS = 365 * 24 * 60 * 60 * 1000;

  try {
    let item1GUID = "abcdefabcdef";
    let item1 = new Bookmark("bookmarks", item1GUID);
    item1.bmkUri = "https://example.com";
    item1.title = "asdf";
    item1.parentName = "Bookmarks Toolbar";
    item1.parentid = "toolbar";
    item1.dateAdded = now - oneYearMS;
    collection.insert(item1GUID, encryptPayload(item1.cleartext));

    let item2GUID = "aaaaaaaaaaaa";
    let item2 = new Bookmark("bookmarks", item2GUID);
    item2.bmkUri = "https://example.com/2";
    item2.title = "asdf2";
    item2.parentName = "Bookmarks Toolbar";
    item2.parentid = "toolbar";
    item2.dateAdded = now + oneYearMS;
    const item2LastModified = now / 1000 - 100;
    collection.insert(item2GUID, encryptPayload(item2.cleartext), item2LastModified);

    let item3GUID = "bbbbbbbbbbbb";
    let item3 = new Bookmark("bookmarks", item3GUID);
    item3.bmkUri = "https://example.com/3";
    item3.title = "asdf3";
    item3.parentName = "Bookmarks Toolbar";
    item3.parentid = "toolbar";
    // no dateAdded
    collection.insert(item3GUID, encryptPayload(item3.cleartext));

    let item4GUID = "cccccccccccc";
    let item4 = new Bookmark("bookmarks", item4GUID);
    item4.bmkUri = "https://example.com/4";
    item4.title = "asdf4";
    item4.parentName = "Bookmarks Toolbar";
    item4.parentid = "toolbar";
    // no dateAdded, but lastModified in past
    const item4LastModified = (now - oneYearMS) / 1000;
    collection.insert(item4GUID, encryptPayload(item4.cleartext), item4LastModified);

    let item5GUID = "dddddddddddd";
    let item5 = new Bookmark("bookmarks", item5GUID);
    item5.bmkUri = "https://example.com/5";
    item5.title = "asdf5";
    item5.parentName = "Bookmarks Toolbar";
    item5.parentid = "toolbar";
    // no dateAdded, lastModified in (near) future.
    const item5LastModified = (now + 60000) / 1000;
    collection.insert(item5GUID, encryptPayload(item5.cleartext), item5LastModified);

    let item6GUID = "eeeeeeeeeeee";
    let item6 = new Bookmark("bookmarks", item6GUID);
    item6.bmkUri = "https://example.com/6";
    item6.title = "asdf6";
    item6.parentName = "Bookmarks Toolbar";
    item6.parentid = "toolbar";
    const item6LastModified = (now - oneYearMS) / 1000;
    collection.insert(item6GUID, encryptPayload(item6.cleartext), item6LastModified);

    let origBuildWeakReuploadMap = engine.buildWeakReuploadMap;
    engine.buildWeakReuploadMap = set => {
      let fullMap = origBuildWeakReuploadMap.call(engine, set);
      fullMap.delete(item6GUID);
      return fullMap;
    };

    await sync_engine_and_validate_telem(engine, false);

    let record1 = await store.createRecord(item1GUID);
    let record2 = await store.createRecord(item2GUID);

    equal(item1.dateAdded, record1.dateAdded, "dateAdded in past should be synced");
    equal(record2.dateAdded, item2LastModified * 1000, "dateAdded in future should be ignored in favor of last modified");

    let record3 = await store.createRecord(item3GUID);

    ok(record3.dateAdded);
    // Make sure it's within 24 hours of the right timestamp... This is a little
    // dodgey but we only really care that it's basically accurate and has the
    // right day.
    ok(Math.abs(Date.now() - record3.dateAdded) < 24 * 60 * 60 * 1000);

    let record4 = await store.createRecord(item4GUID);
    equal(record4.dateAdded, item4LastModified * 1000,
          "If no dateAdded is provided, lastModified should be used");

    let record5 = await store.createRecord(item5GUID);
    equal(record5.dateAdded, item5LastModified * 1000,
          "If no dateAdded is provided, lastModified should be used (even if it's in the future)");

    let item6WBO = JSON.parse(JSON.parse(collection._wbos[item6GUID].payload).ciphertext);
    ok(!item6WBO.dateAdded,
       "If we think an item has been modified locally, we don't upload it to the server");

    let record6 = await store.createRecord(item6GUID);
    equal(record6.dateAdded, item6LastModified * 1000,
       "We still remember the more accurate dateAdded if we don't upload a record due to local changes");
    engine.buildWeakReuploadMap = origBuildWeakReuploadMap;

    // Update item2 and try resyncing it.
    item2.dateAdded = now - 100000;
    collection.insert(item2GUID, encryptPayload(item2.cleartext), now / 1000 - 50);


    // Also, add a local bookmark and make sure it's date added makes it up to the server
    let bzid = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarksMenuFolderId, Utils.makeURI("https://bugzilla.mozilla.org/"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Bugzilla");

    let bzguid = await PlacesUtils.promiseItemGuid(bzid);


    await sync_engine_and_validate_telem(engine, false);

    let newRecord2 = await store.createRecord(item2GUID);
    equal(newRecord2.dateAdded, item2.dateAdded, "dateAdded update should work for earlier date");

    let bzWBO = JSON.parse(JSON.parse(collection._wbos[bzguid].payload).ciphertext);
    ok(bzWBO.dateAdded, "Locally added dateAdded lost");

    let localRecord = await store.createRecord(bzguid);
    equal(bzWBO.dateAdded, localRecord.dateAdded, "dateAdded should not change during upload");

    item2.dateAdded += 10000;
    collection.insert(item2GUID, encryptPayload(item2.cleartext), now / 1000 - 10);
    engine.lastSync = now / 1000 - 20;

    await sync_engine_and_validate_telem(engine, false);

    let newerRecord2 = await store.createRecord(item2GUID);
    equal(newerRecord2.dateAdded, newRecord2.dateAdded,
      "dateAdded update should be ignored for later date if we know an earlier one ");



  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

function run_test() {
  initTestLogging("Trace");
  generateNewKeys(Service.collectionKeys);
  run_next_test();
}
