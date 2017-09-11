/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
const IOService = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);

async function newSmartBookmark(parentGuid, url, position, title, queryID) {
  let info = await PlacesUtils.bookmarks.insert({
    parentGuid,
    url,
    position,
    title,
  });
  let id = await PlacesUtils.promiseItemId(info.guid);
  PlacesUtils.annotations.setItemAnnotation(id, SMART_BOOKMARKS_ANNO,
                                            queryID, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  return info;
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
}

let engine;
let store;

add_task(async function setup() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;

  await generateNewKeys(Service.collectionKeys);
});

add_task(async function setup() {
  await Service.engineManager.register(BookmarksEngine);
  engine = Service.engineManager.get("bookmarks");
  store = engine._store;
});

// Verify that Places smart bookmarks have their annotation uploaded and
// handled locally.
add_task(async function test_annotation_uploaded() {
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let startCount = smartBookmarkCount();

  _("Start count is " + startCount);

  if (startCount > 0) {
    // This can happen in XULRunner.
    clearBookmarks();
    _("Start count is now " + startCount);
  }

  _("Create a smart bookmark in the toolbar.");
  let url = "place:sort=" +
            Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
            "&maxResults=10";
  let title = "Most Visited";

  let mostVisitedInfo = await newSmartBookmark(
    PlacesUtils.bookmarks.toolbarGuid, url, -1, title, "MostVisited");
  let mostVisitedID = await PlacesUtils.promiseItemId(mostVisitedInfo.guid);

  _("New item ID: " + mostVisitedID);
  do_check_true(!!mostVisitedID);

  let annoValue = PlacesUtils.annotations.getItemAnnotation(mostVisitedID,
                                              SMART_BOOKMARKS_ANNO);
  _("Anno: " + annoValue);
  do_check_eq("MostVisited", annoValue);

  let guid = await store.GUIDForId(mostVisitedID);
  _("GUID: " + guid);
  do_check_true(!!guid);

  _("Create record object and verify that it's sane.");
  let record = await store.createRecord(guid);
  do_check_true(record instanceof Bookmark);
  do_check_true(record instanceof BookmarkQuery);

  do_check_eq(record.bmkUri, url);

  _("Make sure the new record carries with it the annotation.");
  do_check_eq("MostVisited", record.queryId);

  _("Our count has increased since we started.");
  do_check_eq(smartBookmarkCount(), startCount + 1);

  _("Sync record to the server.");
  let collection = server.user("foo").collection("bookmarks");

  try {
    await sync_engine_and_validate_telem(engine, false);
    let wbos = collection.keys(function(id) {
                 return ["menu", "toolbar", "mobile", "unfiled"].indexOf(id) == -1;
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
      mostVisitedID, CommonUtils.makeURI("http://something/else"));
    PlacesUtils.annotations.removeItemAnnotation(mostVisitedID,
                                                 SMART_BOOKMARKS_ANNO);
    await store.wipe();
    await engine.resetClient();
    do_check_eq(smartBookmarkCount(), startCount);

    _("Sync. Verify that the downloaded record carries the annotation.");
    await sync_engine_and_validate_telem(engine, false);

    _("Verify that the Places DB now has an annotated bookmark.");
    _("Our count has increased again.");
    do_check_eq(smartBookmarkCount(), startCount + 1);

    _("Find by GUID and verify that it's annotated.");
    let newID = await store.idForGUID(serverGUID);
    let newAnnoValue = PlacesUtils.annotations.getItemAnnotation(
      newID, SMART_BOOKMARKS_ANNO);
    do_check_eq(newAnnoValue, "MostVisited");
    do_check_eq(PlacesUtils.bookmarks.getBookmarkURI(newID).spec, url);

    _("Test updating.");
    let newRecord = await store.createRecord(serverGUID);
    do_check_eq(newRecord.queryId, newAnnoValue);
    newRecord.queryId = "LeastVisited";
    collection.insert(serverGUID, encryptPayload(newRecord.cleartext));
    engine.lastModified = collection.timestamp + 1;
    await sync_engine_and_validate_telem(engine, false);
    do_check_eq("LeastVisited", PlacesUtils.annotations.getItemAnnotation(
      newID, SMART_BOOKMARKS_ANNO));

  } finally {
    // Clean up.
    await store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await promiseStopServer(server);
  }
});

add_task(async function test_smart_bookmarks_duped() {
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);
  let collection = server.user("foo").collection("bookmarks");

  let url = "place:sort=" +
            Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
            "&maxResults=10";
  let title = "Most Visited";

  try {

    _("Verify that queries with the same anno and URL dupe");
    {
      let info = await newSmartBookmark(PlacesUtils.bookmarks.toolbarGuid, url,
                                        -1, title, "MostVisited");
      let idForOldGUID = await PlacesUtils.promiseItemId(info.guid);

      let record = await store.createRecord(info.guid);
      record.id = Utils.makeGUID();
      collection.insert(record.id, encryptPayload(record.cleartext));

      collection.insert("toolbar", encryptPayload({
        id: "toolbar",
        parentid: "places",
        type: "folder",
        title: "Bookmarks Toolbar",
        children: [record.id],
      }));

      await sync_engine_and_validate_telem(engine, false);

      let idForNewGUID = await PlacesUtils.promiseItemId(record.id);
      equal(idForOldGUID, idForNewGUID);
    }

    _("Verify that queries with the same anno and different URL dupe");
    {
      let info = await newSmartBookmark(PlacesUtils.bookmarks.menuGuid,
                                        "place:bar", -1, title,
                                        "MostVisited");
      let idForOldGUID = await PlacesUtils.promiseItemId(info.guid);

      let record = await store.createRecord(info.guid);
      record.id = Utils.makeGUID();
      collection.insert(record.id, encryptPayload(record.cleartext), engine.lastSync + 1);

      collection.insert("menu", encryptPayload({
        id: "menu",
        parentid: "places",
        type: "folder",
        title: "Bookmarks Menu",
        children: [record.id],
      }), engine.lastSync + 1);

      engine.lastModified = collection.timestamp;
      await sync_engine_and_validate_telem(engine, false);

      let idForNewGUID = await PlacesUtils.promiseItemId(record.id);
      equal(idForOldGUID, idForNewGUID);
    }

    _("Verify that different annos don't dupe.");
    {
      let info = await newSmartBookmark(PlacesUtils.bookmarks.unfiledGuid,
                                        "place:foo", -1, title, "LeastVisited");
      let idForOldGUID = await PlacesUtils.promiseItemId(info.guid);

      let other = await store.createRecord(info.guid);
      other.id = "abcdefabcdef";
      other.queryId = "MostVisited";
      collection.insert(other.id, encryptPayload(other.cleartext), engine.lastSync + 1);

      collection.insert("unfiled", encryptPayload({
        id: "unfiled",
        parentid: "places",
        type: "folder",
        title: "Other Bookmarks",
        children: [other.id],
      }), engine.lastSync + 1);

      engine.lastModified = collection.timestamp;
      await sync_engine_and_validate_telem(engine, false);

      let idForNewGUID = await PlacesUtils.promiseItemId(other.id);
      notEqual(idForOldGUID, idForNewGUID);
    }

    _("Handle records without a queryId entry.");
    {
      let info = await newSmartBookmark(PlacesUtils.bookmarks.mobileGuid, url,
                                        -1, title, "MostVisited");
      let idForOldGUID = await PlacesUtils.promiseItemId(info.guid);

      let record = await store.createRecord(info.guid);
      record.id = Utils.makeGUID();
      delete record.queryId;
      collection.insert(record.id, encryptPayload(record.cleartext), engine.lastSync + 1);

      collection.insert("mobile", encryptPayload({
        id: "mobile",
        parentid: "places",
        type: "folder",
        title: "Mobile Bookmarks",
        children: [record.id],
      }), engine.lastSync + 1);

      engine.lastModified = collection.timestamp;
      await sync_engine_and_validate_telem(engine, false);

      let idForNewGUID = await PlacesUtils.promiseItemId(record.id);
      equal(idForOldGUID, idForNewGUID);
    }

  } finally {
    // Clean up.
    await store.wipe();
    await promiseStopServer(server);
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
  }
});
