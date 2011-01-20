Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");

function makeEngine() {
  return new BookmarksEngine();
}
var syncTesting = new SyncTestingInfrastructure(makeEngine);

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

    // Also create a bogus server record (no parent) to provoke an exception.
    const BOGUS_GUID = "zzzzzzzzzzzz";
    collection.wbos[BOGUS_GUID] = new ServerWBO(
      BOGUS_GUID, encryptPayload({
        id: BOGUS_GUID,
        type: "folder",
        title: "Bogus Folder",
        parentid: null,
        parentName: null,
        children: []
    }));

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

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Engine.Bookmarks").level = Log4Moz.Level.Trace;

  CollectionKeys.generateNewKeys();

  test_processIncoming_error_orderChildren();
}
