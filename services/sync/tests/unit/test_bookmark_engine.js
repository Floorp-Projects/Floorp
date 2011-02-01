Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");

Cu.import("resource://services-sync/service.js");
try {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
}
catch(ex) {
  Cu.import("resource://gre/modules/utils.js");
}

Engines.register(BookmarksEngine);

function makeEngine() {
  return new BookmarksEngine();
}
var syncTesting = new SyncTestingInfrastructure(makeEngine);

function test_ID_caching() {

  _("Ensure that Places IDs are not cached.");
  let engine = new BookmarksEngine();
  let store = engine._store;
  _("All IDs: " + JSON.stringify(store.getAllIDs()));

  let mobileID = store.idForGUID("mobile");
  _("Change the GUID for that item, and drop the mobile anno.");
  store._setGUID(mobileID, "abcdefghijkl");
  Svc.Annos.removeItemAnnotation(mobileID, "mobile/bookmarksRoot");

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
}

function test_processIncoming_error_orderChildren() {
  _("Ensure that _orderChildren() is called even when _processIncoming() throws an error.");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  let collection = new ServerCollection();
  let engine = new BookmarksEngine();
  let store = engine._store;
  let global = new ServerWBO('global',
                             {engines: {bookmarks: {version: engine.version,
                                                    syncID: engine.syncID}}});
  let server = httpd_setup({
    "/1.0/foo/storage/meta/global": global.handler(),
    "/1.0/foo/storage/bookmarks": collection.handler()
  });

  try {

    let folder1_id = Svc.Bookmark.createFolder(
      Svc.Bookmark.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);

    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let tburi = Utils.makeURI("http://getthunderbird.com/");

    let bmk1_id = Svc.Bookmark.insertBookmark(
      folder1_id, fxuri, Svc.Bookmark.DEFAULT_INDEX, "Get Firefox!");
    let bmk1_guid = store.GUIDForId(bmk1_id);
    let bmk2_id = Svc.Bookmark.insertBookmark(
      folder1_id, tburi, Svc.Bookmark.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);

    // Create a server record for folder1 where we flip the order of
    // the children.
    let folder1_payload = store.createRecord(folder1_guid).cleartext;
    folder1_payload.children.reverse();
    collection.wbos[folder1_guid] = new ServerWBO(
      folder1_guid, encryptPayload(folder1_payload));

    // Create a bogus record that when synced down will provoke a
    // network error which in turn provokes an exception in _processIncoming.
    const BOGUS_GUID = "zzzzzzzzzzzz";
    let bogus_record = collection.wbos[BOGUS_GUID]
      = new ServerWBO(BOGUS_GUID, "I'm a bogus record!");
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

    do_check_eq(Svc.Bookmark.getItemIndex(bmk1_id), 1);
    do_check_eq(Svc.Bookmark.getItemIndex(bmk2_id), 0);

  } finally {
    store.wipe();
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeEngine);
  }
}

function test_restorePromptsReupload() {
  _("Ensure that restoring from a backup will reupload all records.");
  Svc.Prefs.set("username", "foo");
  Service.serverURL = "http://localhost:8080/";
  Service.clusterURL = "http://localhost:8080/";

  let collection = new ServerCollection({}, true);
  
  let engine = new BookmarksEngine();
  let store = engine._store;
  let global = new ServerWBO('global',
                             {engines: {bookmarks: {version: engine.version,
                                                    syncID: engine.syncID}}});
  let server = httpd_setup({
    "/1.0/foo/storage/meta/global": global.handler(),
    "/1.0/foo/storage/bookmarks": collection.handler()
  });

  Svc.Obs.notify("weave:engine:start-tracking");   // We skip usual startup...

  try {

    let folder1_id = Svc.Bookmark.createFolder(
      Svc.Bookmark.toolbarFolder, "Folder 1", 0);
    let folder1_guid = store.GUIDForId(folder1_id);
    _("Folder 1: " + folder1_id + ", " + folder1_guid);

    let fxuri = Utils.makeURI("http://getfirefox.com/");
    let tburi = Utils.makeURI("http://getthunderbird.com/");

    _("Create a single record.");
    let bmk1_id = Svc.Bookmark.insertBookmark(
      folder1_id, fxuri, Svc.Bookmark.DEFAULT_INDEX, "Get Firefox!");
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
    let bmk2_id = Svc.Bookmark.insertBookmark(
      folder1_id, tburi, Svc.Bookmark.DEFAULT_INDEX, "Get Thunderbird!");
    let bmk2_guid = store.GUIDForId(bmk2_id);
    _("Get Thunderbird!: " + bmk2_id + ", " + bmk2_guid);

    Svc.Bookmark.removeItem(bmk1_id);

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
    let wbos = [id for ([id, wbo] in Iterator(collection.wbos))
                   if (["menu", "toolbar", "mobile", folder1_guid].indexOf(id) == -1)];
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
      if (Svc.Bookmark.getItemType(id) == Svc.Bookmark.TYPE_BOOKMARK) {
        let uri = Svc.Bookmark.getBookmarkURI(id);
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
    wbos = [JSON.parse(JSON.parse(wbo.payload).ciphertext)
            for ([id, wbo] in Iterator(collection.wbos))
            if (wbo.payload)];

    _("WBOs: " + JSON.stringify(wbos));
    let bookmarks = [wbo for each (wbo in wbos) if (wbo.type == "bookmark")];
    do_check_eq(bookmarks.length, 1);
    do_check_eq(bookmarks[0].id, newFX);
    do_check_eq(bookmarks[0].bmkUri, fxuri.spec);
    do_check_eq(bookmarks[0].title, "Get Firefox!");

    _("Our old friend Folder 1 is still in play.");
    let folders = [wbo for each (wbo in wbos)
                       if ((wbo.type == "folder") &&
                           (wbo.id   != "menu") &&
                           (wbo.id   != "toolbar"))];
    do_check_eq(folders.length, 1);
    do_check_eq(folders[0].title, "Folder 1");

  } finally {
    store.wipe();
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeEngine);
  }
}

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Engine.Bookmarks").level = Log4Moz.Level.Trace;

  CollectionKeys.generateNewKeys();

  test_processIncoming_error_orderChildren();
  test_ID_caching();
  test_restorePromptsReupload();
}
