/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
var IOService = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);
("http://www.mozilla.com", null, null);


Service.engineManager.register(BookmarksEngine);
let engine = Service.engineManager.get("bookmarks");
let store = engine._store;

// Clean up after other tests. Only necessary in XULRunner.
store.wipe();

function newSmartBookmark(parent, uri, position, title, queryID) {
  let id = PlacesUtils.bookmarks.insertBookmark(parent, uri, position, title);
  PlacesUtils.annotations.setItemAnnotation(id, SMART_BOOKMARKS_ANNO,
                                            queryID, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  return id;
}

function smartBookmarkCount() {
  // We do it this way because PlacesUtils.annotations.getItemsWithAnnotation
  // doesn't work the same (or at all?) between 3.6 and 4.0.
  let out = {};
  PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO, out);
  return out.value;
}

function clearBookmarks() {
  _("Cleaning up existing items.");
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.bookmarksMenuFolder);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.tagsFolder);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.toolbarFolder);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.unfiledBookmarksFolder);
  startCount = smartBookmarkCount();
}

function serverForFoo(engine) {
  return serverForUsers({"foo": "password"}, {
    meta: {global: {engines: {bookmarks: {version: engine.version,
                                          syncID: engine.syncID}}}},
    bookmarks: {}
  });
}

// Verify that Places smart bookmarks have their annotation uploaded and
// handled locally.
add_test(function test_annotation_uploaded() {
  let server = serverForFoo(engine);
  new SyncTestingInfrastructure(server.server);

  let startCount = smartBookmarkCount();

  _("Start count is " + startCount);

  if (startCount > 0) {
    // This can happen in XULRunner.
    clearBookmarks();
    _("Start count is now " + startCount);
  }

  _("Create a smart bookmark in the toolbar.");
  let parent = PlacesUtils.toolbarFolderId;
  let uri =
    Utils.makeURI("place:sort=" +
                  Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                  "&maxResults=10");
  let title = "Most Visited";

  let mostVisitedID = newSmartBookmark(parent, uri, -1, title, "MostVisited");

  _("New item ID: " + mostVisitedID);
  do_check_true(!!mostVisitedID);

  let annoValue = PlacesUtils.annotations.getItemAnnotation(mostVisitedID,
                                              SMART_BOOKMARKS_ANNO);
  _("Anno: " + annoValue);
  do_check_eq("MostVisited", annoValue);

  let guid = store.GUIDForId(mostVisitedID);
  _("GUID: " + guid);
  do_check_true(!!guid);

  _("Create record object and verify that it's sane.");
  let record = store.createRecord(guid);
  do_check_true(record instanceof Bookmark);
  do_check_true(record instanceof BookmarkQuery);

  do_check_eq(record.bmkUri, uri.spec);

  _("Make sure the new record carries with it the annotation.");
  do_check_eq("MostVisited", record.queryId);

  _("Our count has increased since we started.");
  do_check_eq(smartBookmarkCount(), startCount + 1);

  _("Sync record to the server.");
  let collection = server.user("foo").collection("bookmarks");

  try {
    engine.sync();
    let wbos = collection.keys(function (id) {
                 return ["menu", "toolbar", "mobile"].indexOf(id) == -1;
               });
    do_check_eq(wbos.length, 1);

    _("Verify that the server WBO has the annotation.");
    let serverGUID = wbos[0];
    do_check_eq(serverGUID, guid);
    let serverWBO = collection.wbo(serverGUID);
    do_check_true(!!serverWBO);
    let body = JSON.parse(JSON.parse(serverWBO.payload).ciphertext);
    do_check_eq(body.queryId, "MostVisited");

    _("We still have the right count.");
    do_check_eq(smartBookmarkCount(), startCount + 1);

    _("Clear local records; now we can't find it.");

    // "Clear" by changing attributes: if we delete it, apparently it sticks
    // around as a deleted record...
    PlacesUtils.bookmarks.setItemTitle(mostVisitedID, "Not Most Visited");
    PlacesUtils.bookmarks.changeBookmarkURI(
      mostVisitedID, Utils.makeURI("http://something/else"));
    PlacesUtils.annotations.removeItemAnnotation(mostVisitedID,
                                                 SMART_BOOKMARKS_ANNO);
    store.wipe();
    engine.resetClient();
    do_check_eq(smartBookmarkCount(), startCount);

    _("Sync. Verify that the downloaded record carries the annotation.");
    engine.sync();

    _("Verify that the Places DB now has an annotated bookmark.");
    _("Our count has increased again.");
    do_check_eq(smartBookmarkCount(), startCount + 1);

    _("Find by GUID and verify that it's annotated.");
    let newID = store.idForGUID(serverGUID);
    let newAnnoValue = PlacesUtils.annotations.getItemAnnotation(
      newID, SMART_BOOKMARKS_ANNO);
    do_check_eq(newAnnoValue, "MostVisited");
    do_check_eq(PlacesUtils.bookmarks.getBookmarkURI(newID).spec, uri.spec);

    _("Test updating.");
    let newRecord = store.createRecord(serverGUID);
    do_check_eq(newRecord.queryId, newAnnoValue);
    newRecord.queryId = "LeastVisited";
    store.update(newRecord);
    do_check_eq("LeastVisited", PlacesUtils.annotations.getItemAnnotation(
      newID, SMART_BOOKMARKS_ANNO));


  } finally {
    // Clean up.
    store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_smart_bookmarks_duped() {
  let server = serverForFoo(engine);
  new SyncTestingInfrastructure(server.server);

  let parent = PlacesUtils.toolbarFolderId;
  let uri =
    Utils.makeURI("place:sort=" +
                  Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                  "&maxResults=10");
  let title = "Most Visited";
  let mostVisitedID = newSmartBookmark(parent, uri, -1, title, "MostVisited");
  let mostVisitedGUID = store.GUIDForId(mostVisitedID);

  let record = store.createRecord(mostVisitedGUID);

  _("Prepare sync.");
  let collection = server.user("foo").collection("bookmarks");

  try {
    engine._syncStartup();

    _("Verify that mapDupe uses the anno, discovering a dupe regardless of URI.");
    do_check_eq(mostVisitedGUID, engine._mapDupe(record));

    record.bmkUri = "http://foo/";
    do_check_eq(mostVisitedGUID, engine._mapDupe(record));
    do_check_neq(PlacesUtils.bookmarks.getBookmarkURI(mostVisitedID).spec,
                 record.bmkUri);

    _("Verify that different annos don't dupe.");
    let other = new BookmarkQuery("bookmarks", "abcdefabcdef");
    other.queryId = "LeastVisited";
    other.parentName = "Bookmarks Toolbar";
    other.bmkUri = "place:foo";
    other.title = "";
    do_check_eq(undefined, engine._findDupe(other));

    _("Handle records without a queryId entry.");
    record.bmkUri = uri;
    delete record.queryId;
    do_check_eq(mostVisitedGUID, engine._mapDupe(record));

    engine._syncFinish();

  } finally {
    // Clean up.
    store.wipe();
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
  }
});

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Bookmarks").level = Log4Moz.Level.Trace;

  generateNewKeys(Service.collectionKeys);

  run_next_test();
}
