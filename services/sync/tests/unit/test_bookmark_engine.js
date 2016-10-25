/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://gre/modules/Promise.jsm");

initTestLogging("Trace");

Service.engineManager.register(BookmarksEngine);

function* assertChildGuids(folderGuid, expectedChildGuids, message) {
  let tree = yield PlacesUtils.promiseBookmarksTree(folderGuid);
  let childGuids = tree.children.map(child => child.guid);
  deepEqual(childGuids, expectedChildGuids, message);
}

add_task(function* test_change_during_sync() {
  _("Ensure that we track changes made during a sync.");

  let engine  = new BookmarksEngine(Service);
  let store   = engine._store;
  let tracker = engine._tracker;
  let server = serverForFoo(engine);
  new SyncTestingInfrastructure(server.server);

  let collection = server.user("foo").collection("bookmarks");

  let bz_id = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.bookmarksMenuFolderId, Utils.makeURI("https://bugzilla.mozilla.org/"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Bugzilla");
  let bz_guid = yield PlacesUtils.promiseItemGuid(bz_id);
    _(`Bugzilla GUID: ${bz_guid}`);

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);
    _(`Folder GUID: ${folder1_guid}`);

    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, Utils.makeURI("http://getthunderbird.com/"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk1_guid = store.GUIDForId(bmk1_id);
    _(`Thunderbird GUID: ${bmk1_guid}`);

    // Sync is synchronous, so, to simulate a bookmark change made during a
    // sync, we create a server record that adds a bookmark as a side effect.
    let bmk2_guid = "get-firefox1"; // New child of Folder 1, created remotely.
    let bmk3_id = -1; // New child of Folder 1, created locally during sync.
    let folder2_guid = "folder2-1111"; // New folder, created remotely.
    let tagQuery_guid = "tag-query111"; // New tag query child of Folder 2, created remotely.
    let bmk4_guid = "example-org1"; // New tagged child of Folder 2, created remotely.
    {
      // An existing record changed on the server that should not trigger
      // another sync when applied.
      let bzBmk = new Bookmark("bookmarks", bz_guid);
      bzBmk.bmkUri      = "https://bugzilla.mozilla.org/";
      bzBmk.description = "New description";
      bzBmk.title       = "Bugzilla";
      bzBmk.tags        = ["new", "tags"];
      bzBmk.parentName  = "Bookmarks Toolbar";
      bzBmk.parentid    = "toolbar";
      collection.insert(bz_guid, encryptPayload(bzBmk.cleartext));

      let remoteFolder = new BookmarkFolder("bookmarks", folder2_guid);
      remoteFolder.title      = "Folder 2";
      remoteFolder.children   = [bmk4_guid, tagQuery_guid];
      remoteFolder.parentName = "Bookmarks Menu";
      remoteFolder.parentid   = "menu";
      collection.insert(folder2_guid, encryptPayload(remoteFolder.cleartext));

      let localFxBmk = new Bookmark("bookmarks", bmk2_guid);
      localFxBmk.bmkUri        = "http://getfirefox.com/";
      localFxBmk.description   = "Firefox is awesome.";
      localFxBmk.title         = "Get Firefox!";
      localFxBmk.tags          = ["firefox", "awesome", "browser"];
      localFxBmk.keyword       = "awesome";
      localFxBmk.loadInSidebar = false;
      localFxBmk.parentName    = "Folder 1";
      localFxBmk.parentid      = folder1_guid;
      let remoteFxBmk = collection.insert(bmk2_guid, encryptPayload(localFxBmk.cleartext));
      remoteFxBmk.get = function get() {
        _("Inserting bookmark into local store");
        bmk3_id = PlacesUtils.bookmarks.insertBookmark(
          folder1_id, Utils.makeURI("https://mozilla.org/"),
          PlacesUtils.bookmarks.DEFAULT_INDEX, "Mozilla");

        return ServerWBO.prototype.get.apply(this, arguments);
      };

      // A tag query referencing a nonexistent tag folder, which we should
      // create locally when applying the record.
      let localTagQuery = new BookmarkQuery("bookmarks", tagQuery_guid);
      localTagQuery.bmkUri     = "place:type=7&folder=999";
      localTagQuery.title      = "Taggy tags";
      localTagQuery.folderName = "taggy";
      localTagQuery.parentName = "Folder 2";
      localTagQuery.parentid   = folder2_guid;
      collection.insert(tagQuery_guid, encryptPayload(localTagQuery.cleartext));

      // A bookmark that should appear in the results for the tag query.
      let localTaggedBmk = new Bookmark("bookmarks", bmk4_guid);
      localTaggedBmk.bmkUri     = "https://example.org";
      localTaggedBmk.title      = "Tagged bookmark";
      localTaggedBmk.tags       = ["taggy"];
      localTaggedBmk.parentName = "Folder 2";
      localTaggedBmk.parentid   = folder2_guid;
      collection.insert(bmk4_guid, encryptPayload(localTaggedBmk.cleartext));
    }

    yield* assertChildGuids(folder1_guid, [bmk1_guid], "Folder should have 1 child before first sync");

    _("Perform first sync");
    {
      let changes = engine.pullNewChanges();
      deepEqual(changes.ids().sort(), [folder1_guid, bmk1_guid, "toolbar"].sort(),
        "Should track bookmark and folder created before first sync");
      yield sync_engine_and_validate_telem(engine, false);
    }

    let bmk2_id = store.idForGUID(bmk2_guid);
    let bmk3_guid = store.GUIDForId(bmk3_id);
    _(`Mozilla GUID: ${bmk3_guid}`);
    {
      equal(store.GUIDForId(bmk2_id), bmk2_guid,
        "Remote bookmark should be applied during first sync");
      ok(bmk3_id > -1,
        "Bookmark created during first sync should exist locally");
      ok(!collection.wbo(bmk3_guid),
        "Bookmark created during first sync shouldn't be uploaded yet");

      yield* assertChildGuids(folder1_guid, [bmk1_guid, bmk3_guid, bmk2_guid],
        "Folder 1 should have 3 children after first sync");
      yield* assertChildGuids(folder2_guid, [bmk4_guid, tagQuery_guid],
        "Folder 2 should have 2 children after first sync");
      let taggedURIs = PlacesUtils.tagging.getURIsForTag("taggy");
      equal(taggedURIs.length, 1, "Should have 1 tagged URI");
      equal(taggedURIs[0].spec, "https://example.org/",
        "Synced tagged bookmark should appear in tagged URI list");
    }

    _("Perform second sync");
    {
      let changes = engine.pullNewChanges();
      deepEqual(changes.ids().sort(), [bmk3_guid, folder1_guid].sort(),
        "Should track bookmark added during last sync and its parent");
      yield sync_engine_and_validate_telem(engine, false);

      ok(collection.wbo(bmk3_guid),
        "Bookmark created during first sync should be uploaded during second sync");

      yield* assertChildGuids(folder1_guid, [bmk1_guid, bmk3_guid, bmk2_guid],
        "Folder 1 should have same children after second sync");
      yield* assertChildGuids(folder2_guid, [bmk4_guid, tagQuery_guid],
        "Folder 2 should have same children after second sync");
    }
  } finally {
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    yield new Promise(resolve => server.stop(resolve));
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
});

add_task(function* bad_record_allIDs() {
  let server = new SyncServer();
  server.start();
  let syncTesting = new SyncTestingInfrastructure(server.server);

  _("Ensure that bad Places queries don't cause an error in getAllIDs.");
  let engine = new BookmarksEngine(Service);
  let store = engine._store;
  let badRecordID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder,
      Utils.makeURI("place:folder=1138"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      null);

  do_check_true(badRecordID > 0);
  _("Record is " + badRecordID);
  _("Type: " + PlacesUtils.bookmarks.getItemType(badRecordID));

  _("Fetching all IDs.");
  let all = store.getAllIDs();

  _("All IDs: " + JSON.stringify(all));
  do_check_true("menu" in all);
  do_check_true("toolbar" in all);

  _("Clean up.");
  PlacesUtils.bookmarks.removeItem(badRecordID);
  yield new Promise(r => server.stop(r));
});

function serverForFoo(engine) {
  return serverForUsers({"foo": "password"}, {
    meta: {global: {engines: {bookmarks: {version: engine.version,
                                          syncID: engine.syncID}}}},
    bookmarks: {}
  });
}

add_task(function* test_processIncoming_error_orderChildren() {
  _("Ensure that _orderChildren() is called even when _processIncoming() throws an error.");

  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  new SyncTestingInfrastructure(server.server);

  let collection = server.user("foo").collection("bookmarks");

  try {

    let folder1_id = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);

    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let tburi = Utils.makeURI("http://getthunderbird.com/");

    let bmk1_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, fxuri, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = store.GUIDForId(bmk1_id);
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);

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
      yield sync_engine_and_validate_telem(engine, true)
    } catch(ex) {
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
    yield new Promise(resolve => server.stop(resolve));
  }
});

add_task(function* test_restorePromptsReupload() {
  _("Ensure that restoring from a backup will reupload all records.");
  let engine = new BookmarksEngine(Service);
  let store  = engine._store;
  let server = serverForFoo(engine);
  new SyncTestingInfrastructure(server.server);

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
    yield BookmarkJSONUtils.exportToFile(backupFile.path);

    _("Create a different record and sync.");
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);
    _("Get Thunderbird!: " + bmk2_id + ", " + bmk2_guid);

    PlacesUtils.bookmarks.removeItem(bmk1_id);

    let error;
    try {
      yield sync_engine_and_validate_telem(engine, false);
    } catch(ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    do_check_true(!error);

    _("Verify that there's only one bookmark on the server, and it's Thunderbird.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let wbos = collection.keys(function (id) {
      return ["menu", "toolbar", "mobile", "unfiled", folder1_guid].indexOf(id) == -1;
    });
    do_check_eq(wbos.length, 1);
    do_check_eq(wbos[0], bmk2_guid);

    _("Now restore from a backup.");
    yield BookmarkJSONUtils.importFromFile(backupFile, true);

    _("Ensure we have the bookmarks we expect locally.");
    let guids = store.getAllIDs();
    _("GUIDs: " + JSON.stringify(guids));
    let found = false;
    let count = 0;
    let newFX;
    for (let guid in guids) {
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
      yield sync_engine_and_validate_telem(engine, false);
    } catch(ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    do_check_true(!error);

    _("Verify that there's only one bookmark on the server, and it's Firefox.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let payloads     = server.user("foo").collection("bookmarks").payloads();
    let bookmarkWBOs = payloads.filter(function (wbo) {
                         return wbo.type == "bookmark";
                       });
    let folderWBOs   = payloads.filter(function (wbo) {
                         return ((wbo.type == "folder") &&
                                 (wbo.id   != "menu") &&
                                 (wbo.id   != "toolbar") &&
                                 (wbo.id   != "unfiled") &&
                                 (wbo.id   != "mobile"));
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
    let deferred = Promise.defer();
    server.stop(deferred.resolve);
    yield deferred.promise;
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
add_task(function* test_mismatched_types() {
  _("Ensure that handling a record that changes type causes deletion " +
    "then re-adding.");

  let oldRecord = {
    "id": "l1nZZXfB8nC7",
    "type":"folder",
    "parentName":"Bookmarks Toolbar",
    "title":"Innerst i Sneglehode",
    "description":null,
    "parentid": "toolbar"
  };
  oldRecord.cleartext = oldRecord;

  let newRecord = {
    "id": "l1nZZXfB8nC7",
    "type":"livemark",
    "siteUri":"http://sneglehode.wordpress.com/",
    "feedUri":"http://sneglehode.wordpress.com/feed/",
    "parentName":"Bookmarks Toolbar",
    "title":"Innerst i Sneglehode",
    "description":null,
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
  new SyncTestingInfrastructure(server.server);

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
    yield new Promise(r => server.stop(r));
  }
});

add_task(function* test_bookmark_guidMap_fail() {
  _("Ensure that failures building the GUID map cause early death.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  let server = serverForFoo(engine);
  let coll   = server.user("foo").collection("bookmarks");
  new SyncTestingInfrastructure(server.server);

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
  yield new Promise(r => server.stop(r));
});

add_task(function* test_bookmark_tag_but_no_uri() {
  _("Ensure that a bookmark record with tags, but no URI, doesn't throw an exception.");

  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  // We're simply checking that no exception is thrown, so
  // no actual checks in this test.

  yield PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    syncId: Utils.makeGUID(),
    parentSyncId: "toolbar",
    url: "http://example.com",
    tags: ["foo"],
  });
  yield PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    syncId: Utils.makeGUID(),
    parentSyncId: "toolbar",
    url: "http://example.org",
    tags: null,
  });
  yield PlacesSyncUtils.bookmarks.insert({
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

add_task(function* test_misreconciled_root() {
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
  let encrypted = encryptPayload(rec.cleartext);
  encrypted.decrypt = function () {
    for (let x in rec) {
      encrypted[x] = rec[x];
    }
  };

  _("Applying record.");
  engine._processIncoming({
    getBatched() {
      return this.get();
    },
    get: function () {
      this.recordHandler(encrypted);
      return {success: true}
    },
  });

  // Ensure that afterwards, toolbar is still there.
  // As of 2012-12-05, this only passes because Places doesn't use "toolbar" as
  // the real GUID, instead using a generated one. Sync does the translation.
  let toolbarAfter = store.createRecord("toolbar", "bookmarks");
  let parentGUIDAfter = toolbarAfter.parentid;
  let parentIDAfter = store.idForGUID(parentGUIDAfter);
  do_check_eq(store.GUIDForId(toolbarIDBefore), "toolbar");
  do_check_eq(parentGUIDBefore, parentGUIDAfter);
  do_check_eq(parentIDBefore, parentIDAfter);

  yield new Promise(r => server.stop(r));
});

function run_test() {
  initTestLogging("Trace");
  generateNewKeys(Service.collectionKeys);
  run_next_test();
}
