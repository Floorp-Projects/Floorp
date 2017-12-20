/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

async function newSmartBookmark(parentGuid, url, position, title, queryID) {
  let info = await PlacesUtils.bookmarks.insert({
    parentGuid,
    url,
    position,
    title,
  });
  let id = await PlacesUtils.promiseItemId(info.guid);
  PlacesUtils.annotations.setItemAnnotation(id,
    PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO, queryID, 0,
    PlacesUtils.annotations.EXPIRE_NEVER);
  return info;
}

function smartBookmarkCount() {
  // We do it this way because PlacesUtils.annotations.getItemsWithAnnotation
  // doesn't work the same (or at all?) between 3.6 and 4.0.
  let out = {};
  PlacesUtils.annotations.getItemsWithAnnotation(
    PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO, out);
  return out.value;
}

let engine;
let store;

add_task(async function setup() {
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

  _("Cleaning up existing items.");
  await PlacesUtils.bookmarks.eraseEverything();

  let startCount = smartBookmarkCount();

  _("Start count is " + startCount);

  _("Create a smart bookmark in the toolbar.");
  let url = "place:sort=" +
            Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
            "&maxResults=10";
  let title = "Most Visited";

  let mostVisitedInfo = await newSmartBookmark(
    PlacesUtils.bookmarks.toolbarGuid, url, -1, title, "MostVisited");
  let mostVisitedID = await PlacesUtils.promiseItemId(mostVisitedInfo.guid);

  _("New item ID: " + mostVisitedID);
  Assert.ok(!!mostVisitedID);

  let annoValue = PlacesUtils.annotations.getItemAnnotation(mostVisitedID,
    PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO);
  _("Anno: " + annoValue);
  Assert.equal("MostVisited", annoValue);

  let guid = await PlacesUtils.promiseItemGuid(mostVisitedID);
  _("GUID: " + guid);
  Assert.ok(!!guid);

  _("Create record object and verify that it's sane.");
  let record = await store.createRecord(guid);
  Assert.ok(record instanceof Bookmark);
  Assert.ok(record instanceof BookmarkQuery);

  Assert.equal(record.bmkUri, url);

  _("Make sure the new record carries with it the annotation.");
  Assert.equal("MostVisited", record.queryId);

  _("Our count has increased since we started.");
  Assert.equal(smartBookmarkCount(), startCount + 1);

  _("Sync record to the server.");
  let collection = server.user("foo").collection("bookmarks");

  try {
    await sync_engine_and_validate_telem(engine, false);
    let wbos = collection.keys(function(id) {
                 return ["menu", "toolbar", "mobile", "unfiled"].indexOf(id) == -1;
               });
    Assert.equal(wbos.length, 1);

    _("Verify that the server WBO has the annotation.");
    let serverGUID = wbos[0];
    Assert.equal(serverGUID, guid);
    let serverWBO = collection.wbo(serverGUID);
    Assert.ok(!!serverWBO);
    let body = JSON.parse(JSON.parse(serverWBO.payload).ciphertext);
    Assert.equal(body.queryId, "MostVisited");

    _("We still have the right count.");
    Assert.equal(smartBookmarkCount(), startCount + 1);

    _("Clear local records; now we can't find it.");

    // "Clear" by changing attributes: if we delete it, apparently it sticks
    // around as a deleted record...
    await PlacesUtils.bookmarks.update({
      guid: mostVisitedInfo.guid,
      title: "Not Most Visited",
      url: "http://something/else",
    });
    PlacesUtils.annotations.removeItemAnnotation(mostVisitedID,
      PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO);
    await store.wipe();
    await engine.resetClient();
    Assert.equal(smartBookmarkCount(), startCount);

    _("Sync. Verify that the downloaded record carries the annotation.");
    await sync_engine_and_validate_telem(engine, false);

    _("Verify that the Places DB now has an annotated bookmark.");
    _("Our count has increased again.");
    Assert.equal(smartBookmarkCount(), startCount + 1);

    _("Find by GUID and verify that it's annotated.");
    let newID = await PlacesUtils.promiseItemId(serverGUID);
    let newAnnoValue = PlacesUtils.annotations.getItemAnnotation(
      newID, PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO);
    Assert.equal(newAnnoValue, "MostVisited");
    let newInfo = await PlacesUtils.bookmarks.fetch(serverGUID);
    Assert.equal(newInfo.url.href, url);

    _("Test updating.");
    let newRecord = await store.createRecord(serverGUID);
    Assert.equal(newRecord.queryId, newAnnoValue);
    newRecord.queryId = "LeastVisited";
    collection.insert(serverGUID, encryptPayload(newRecord.cleartext));
    engine.lastModified = collection.timestamp + 1;
    await sync_engine_and_validate_telem(engine, false);
    Assert.equal("LeastVisited", PlacesUtils.annotations.getItemAnnotation(
      newID, PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO));

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
