Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/async.js");
Cu.import("resource://services-sync/util.js");

Cu.import("resource://services-sync/service.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

Engines.register(BookmarksEngine);
var syncTesting = new SyncTestingInfrastructure();

add_test(function bad_record_allIDs() {
  let syncTesting = new SyncTestingInfrastructure();

  _("Ensure that bad Places queries don't cause an error in getAllIDs.");
  let engine = new BookmarksEngine();
  let store = engine._store;
  let badRecordID = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.toolbarFolder,
      Utils.makeURI("place:folder=1138"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      null);

  do_check_true(badRecordID > 0);
  _("Record is " + badRecordID);
  _("Type: " + PlacesUtils.bookmarks.getItemType(badRecordID));

  _("Fetching children.");
  store._getChildren("toolbar", {});

  _("Fetching all IDs.");
  let all = store.getAllIDs();

  _("All IDs: " + JSON.stringify(all));
  do_check_true("menu" in all);
  do_check_true("toolbar" in all);

  _("Clean up.");
  PlacesUtils.bookmarks.removeItem(badRecordID);
  run_next_test();
});

add_test(function test_ID_caching() {
  let syncTesting = new SyncTestingInfrastructure();

  _("Ensure that Places IDs are not cached.");
  let engine = new BookmarksEngine();
  let store = engine._store;
  _("All IDs: " + JSON.stringify(store.getAllIDs()));

  let mobileID = store.idForGUID("mobile");
  _("Change the GUID for that item, and drop the mobile anno.");
  store._setGUID(mobileID, "abcdefghijkl");
  PlacesUtils.annotations.removeItemAnnotation(mobileID, "mobile/bookmarksRoot");

  let err;
  let newMobileID;

  // With noCreate, we don't find an entry.
  try {
    newMobileID = store.idForGUID("mobile", true);
    _("New mobile ID: " + newMobileID);
  } catch (ex) {
    err = ex;
    _("Error: " + Utils.exceptionStr(err));
  }

  do_check_true(!err);

  // With !noCreate, lookup works, and it's different.
  newMobileID = store.idForGUID("mobile", false);
  _("New mobile ID: " + newMobileID);
  do_check_true(!!newMobileID);
  do_check_neq(newMobileID, mobileID);

  // And it's repeatable, even with creation enabled.
  do_check_eq(newMobileID, store.idForGUID("mobile", false));

  do_check_eq(store.GUIDForId(mobileID), "abcdefghijkl");
  run_next_test();
});

function serverForFoo(engine) {
  return serverForUsers({"foo": "password"}, {
    meta: {global: {engines: {bookmarks: {version: engine.version,
                                          syncID: engine.syncID}}}},
    bookmarks: {}
  });
}

add_test(function test_processIncoming_error_orderChildren() {
  _("Ensure that _orderChildren() is called even when _processIncoming() throws an error.");
  new SyncTestingInfrastructure();

  let engine = new BookmarksEngine();
  let store  = engine._store;
  let server = serverForFoo(engine);

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
      engine.sync();
    } catch(ex) {
      error = ex;
    }
    do_check_true(!!error);

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
    Records.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_restorePromptsReupload() {
  _("Ensure that restoring from a backup will reupload all records.");
  new SyncTestingInfrastructure();

  let engine = new BookmarksEngine();
  let store  = engine._store;
  let server = serverForFoo(engine);

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
    backupFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
    PlacesUtils.backupBookmarksToFile(backupFile);

    _("Create a different record and sync.");
    let bmk2_id = PlacesUtils.bookmarks.insertBookmark(
      folder1_id, tburi, PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);
    _("Get Thunderbird!: " + bmk2_id + ", " + bmk2_guid);

    PlacesUtils.bookmarks.removeItem(bmk1_id);

    let error;
    try {
      engine.sync();
    } catch(ex) {
      error = ex;
      _("Got error: " + Utils.exceptionStr(ex));
    }
    do_check_true(!error);

    _("Verify that there's only one bookmark on the server, and it's Thunderbird.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let wbos = collection.keys(function (id) {
      return ["menu", "toolbar", "mobile", folder1_guid].indexOf(id) == -1;
    });
    do_check_eq(wbos.length, 1);
    do_check_eq(wbos[0], bmk2_guid);

    _("Now restore from a backup.");
    PlacesUtils.restoreBookmarksFromJSONFile(backupFile);

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
    do_check_eq(count, ["menu", "toolbar", folder1_id, bmk1_id].length);

    _("Sync again. This'll wipe bookmarks from the server.");
    try {
      engine.sync();
    } catch(ex) {
      error = ex;
      _("Got error: " + Utils.exceptionStr(ex));
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
                                 (wbo.id   != "toolbar"));
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
    Records.clearCache();
    server.stop(run_next_test);
  }
});

// Bug 632287.
add_test(function test_mismatched_types() {
  _("Ensure that handling a record that changes type causes deletion " +
    "then re-adding.");

  function FakeRecord(constructor, r) {
    constructor.call(this, "bookmarks", r.id);
    for (let x in r) {
      this[x] = r[x];
    }
  }

  let oldRecord = {
    "id": "l1nZZXfB8nC7",
    "type":"folder",
    "parentName":"Bookmarks Toolbar",
    "title":"Innerst i Sneglehode",
    "description":null,
    "parentid": "toolbar"
  };

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

  new SyncTestingInfrastructure();

  let engine = new BookmarksEngine();
  let store  = engine._store;
  let server = serverForFoo(engine);

  _("GUID: " + store.GUIDForId(6, true));

  try {
    let bms = PlacesUtils.bookmarks;
    let oldR = new FakeRecord(BookmarkFolder, oldRecord);
    let newR = new FakeRecord(Livemark, newRecord);
    oldR._parent = PlacesUtils.bookmarks.toolbarFolder;
    newR._parent = PlacesUtils.bookmarks.toolbarFolder;

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
    Records.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_bookmark_guidMap_fail() {
  _("Ensure that failures building the GUID map cause early death.");

  new SyncTestingInfrastructure();

  let engine = new BookmarksEngine();
  let store = engine._store;

  let store  = engine._store;
  let server = serverForFoo(engine);
  let coll   = server.user("foo").collection("bookmarks");

  // Add one item to the server.
  let itemID = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.toolbarFolder, "Folder 1", 0);
  let itemGUID    = store.GUIDForId(itemID);
  let itemPayload = store.createRecord(itemGUID).cleartext;
  coll.insert(itemGUID, encryptPayload(itemPayload));

  engine.lastSync = 1;   // So we don't back up.

  // Make building the GUID map fail.
  store.getAllIDs = function () { throw "Nooo"; };

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

  server.stop(run_next_test);
});


function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Bookmarks").level = Log4Moz.Level.Trace;

  generateNewKeys();

  run_next_test();
}
