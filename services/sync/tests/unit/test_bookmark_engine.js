/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");
Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");


initTestLogging("Trace");

async function fetchAllRecordIds() {
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
  let recordIds = new Set();
  for (let row of rows) {
    let recordId = PlacesSyncUtils.bookmarks.guidToRecordId(
      row.getResultByName("guid"));
    recordIds.add(recordId);
  }
  return recordIds;
}
add_task(async function setup() {
  initTestLogging("Trace");
  await generateNewKeys(Service.collectionKeys);
});

add_task(async function setup() {
  await Service.engineManager.unregister("bookmarks");

  initTestLogging("Trace");
  generateNewKeys(Service.collectionKeys);
});

add_task(async function test_delete_invalid_roots_from_server() {
  _("Ensure that we delete the Places and Reading List roots from the server.");

  let engine  = new BookmarksEngine(Service);
  await engine.initialize();
  let store   = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  Svc.Obs.notify("weave:engine:start-tracking");

  try {
    let placesRecord = await store.createRecord("places");
    collection.insert("places", encryptPayload(placesRecord.cleartext));

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

    ok(!(await store.itemExists("readinglist")), "Should not apply Reading List root");
    ok(!(await store.itemExists(listBmk.id)), "Should not apply items in Reading List");
    ok((await store.itemExists(newBmk.id)), "Should apply new bookmark");

    deepEqual(collection.keys().sort(), ["menu", "mobile", "toolbar", "unfiled", newBmk.id].sort(),
      "Should remove Places root and reading list items from server; upload local roots");
  } finally {
    await store.wipe();
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
  let badRecord = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "place:folder=1138",
  });

  _("Type: " + badRecord.type);

  _("Fetching all IDs.");
  let all = await fetchAllRecordIds();

  _("All IDs: " + JSON.stringify([...all]));
  Assert.ok(all.has("menu"));
  Assert.ok(all.has("toolbar"));

  _("Clean up.");
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_processIncoming_error_orderChildren() {
  _("Ensure that _orderChildren() is called even when _processIncoming() throws an error.");

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store  = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  try {

    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder 1",
    });

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

    // Create a server record for folder1 where we flip the order of
    // the children.
    let folder1_record = await store.createRecord(folder1.guid);
    let folder1_payload = folder1_record.cleartext;
    folder1_payload.children.reverse();
    collection.insert(folder1.guid, encryptPayload(folder1_payload));

    // Create a bogus record that when synced down will provoke a
    // network error which in turn provokes an exception in _processIncoming.
    const BOGUS_GUID = "zzzzzzzzzzzz";
    let bogus_record = collection.insert(BOGUS_GUID, "I'm a bogus record!");
    bogus_record.get = function get() {
      throw new Error("Sync this!");
    };

    // Make the 10 minutes old so it will only be synced in the toFetch phase.
    bogus_record.modified = Date.now() / 1000 - 60 * 10;
    engine.lastSync = Date.now() / 1000 - 60;
    engine.toFetch = [BOGUS_GUID];

    let error;
    try {
      await sync_engine_and_validate_telem(engine, true);
    } catch (ex) {
      error = ex;
    }
    ok(!!error);

    // Verify that the bookmark order has been applied.
    folder1_record = await store.createRecord(folder1.guid);
    let new_children = folder1_record.children;
    Assert.deepEqual(new_children,
      [folder1_payload.children[0], folder1_payload.children[1]]);

    let localChildIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      folder1.guid);
    Assert.deepEqual(localChildIds, [bmk2.guid, bmk1.guid]);

  } finally {
    await store.wipe();
    await engine.resetClient();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

add_task(async function test_restorePromptsReupload() {
  await test_restoreOrImport(true);
});

add_task(async function test_importPromptsReupload() {
  await test_restoreOrImport(false);
});

// Test a JSON restore or HTML import. Use JSON if `aReplace` is `true`, or
// HTML otherwise.
async function test_restoreOrImport(aReplace) {
  let verb = aReplace ? "restore" : "import";
  let verbing = aReplace ? "restoring" : "importing";
  let bookmarkUtils = aReplace ? BookmarkJSONUtils : BookmarkHTMLUtils;

  _(`Ensure that ${verbing} from a backup will reupload all records.`);

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store  = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  Svc.Obs.notify("weave:engine:start-tracking"); // We skip usual startup...

  try {

    let folder1 = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: "Folder 1",
    });

    _("Create a single record.");
    let bmk1 = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
    });
    _(`Get Firefox!: ${bmk1.guid}`);

    let backupFilePath = OS.Path.join(
      OS.Constants.Path.tmpDir, `t_b_e_${Date.now()}.json`);

    _("Make a backup.");

    await bookmarkUtils.exportToFile(backupFilePath);

    _("Create a different record and sync.");
    let bmk2 = await PlacesUtils.bookmarks.insert({
      parentGuid: folder1.guid,
      url: "http://getthunderbird.com/",
      title: "Get Thunderbird!",
    });
    _(`Get Thunderbird!: ${bmk2.guid}`);

    await PlacesUtils.bookmarks.remove(bmk1.guid);

    let error;
    try {
      await sync_engine_and_validate_telem(engine, false);
    } catch (ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    Assert.ok(!error);

    _("Verify that there's only one bookmark on the server, and it's Thunderbird.");
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let wbos = collection.keys(function(id) {
      return ["menu", "toolbar", "mobile", "unfiled", folder1.guid].indexOf(id) == -1;
    });
    Assert.equal(wbos.length, 1);
    Assert.equal(wbos[0], bmk2.guid);

    _(`Now ${verb} from a backup.`);
    await bookmarkUtils.importFromFile(backupFilePath, aReplace);

    let bookmarksCollection = server.user("foo").collection("bookmarks");
    if (aReplace) {
      _("Verify that we wiped the server.");
      Assert.ok(!bookmarksCollection);
    } else {
      _("Verify that we didn't wipe the server.");
      Assert.ok(!!bookmarksCollection);
    }

    _("Ensure we have the bookmarks we expect locally.");
    let guids = await fetchAllRecordIds();
    _("GUIDs: " + JSON.stringify([...guids]));
    let bookmarkGuids = new Map();
    let count = 0;
    for (let guid of guids) {
      count++;
      let info = await PlacesUtils.bookmarks.fetch(
        PlacesSyncUtils.bookmarks.recordIdToGuid(guid));
      // Only one bookmark, so _all_ should be Firefox!
      if (info.type == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
        _(`Found URI ${info.url.href} for GUID ${guid}`);
        bookmarkGuids.set(info.url.href, guid);
      }
    }
    Assert.ok(bookmarkGuids.has("http://getfirefox.com/"));
    if (!aReplace) {
      Assert.ok(bookmarkGuids.has("http://getthunderbird.com/"));
    }

    _("Have the correct number of IDs locally, too.");
    let expectedResults = ["menu", "toolbar", "mobile", "unfiled", folder1.guid,
                           bmk1.guid];
    if (!aReplace) {
      expectedResults.push("toolbar", folder1.guid, bmk2.guid);
    }
    Assert.equal(count, expectedResults.length);

    _("Sync again. This'll wipe bookmarks from the server.");
    try {
      await sync_engine_and_validate_telem(engine, false);
    } catch (ex) {
      error = ex;
      _("Got error: " + Log.exceptionStr(ex));
    }
    Assert.ok(!error);

    _("Verify that there's the right bookmarks on the server.");
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
                                 (wbo.id != "mobile") &&
                                 (wbo.parentid != "menu"));
                       });

    let expectedFX = {
      id: bookmarkGuids.get("http://getfirefox.com/"),
      bmkUri: "http://getfirefox.com/",
      title: "Get Firefox!"
    };
    let expectedTB = {
      id: bookmarkGuids.get("http://getthunderbird.com/"),
      bmkUri: "http://getthunderbird.com/",
      title: "Get Thunderbird!"
    };

    let expectedBookmarks;
    if (aReplace) {
      expectedBookmarks = [expectedFX];
    } else {
      expectedBookmarks = [expectedTB, expectedFX];
    }

    doCheckWBOs(bookmarkWBOs, expectedBookmarks);

    _("Our old friend Folder 1 is still in play.");
    let expectedFolder1 = { title: "Folder 1" };

    let expectedFolders;
    if (aReplace) {
      expectedFolders = [expectedFolder1];
    } else {
      expectedFolders = [expectedFolder1, expectedFolder1];
    }

    doCheckWBOs(folderWBOs, expectedFolders);

  } finally {
    await store.wipe();
    await engine.resetClient();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
}

function doCheckWBOs(WBOs, expected) {
  Assert.equal(WBOs.length, expected.length);
  for (let i = 0; i < expected.length; i++) {
    let lhs = WBOs[i];
    let rhs = expected[i];
    if ("id" in rhs) {
      Assert.equal(lhs.id, rhs.id);
    }
    if ("bmkUri" in rhs) {
      Assert.equal(lhs.bmkUri, rhs.bmkUri);
    }
    if ("title" in rhs) {
      Assert.equal(lhs.title, rhs.title);
    }
  }
}

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
  await engine.initialize();
  let store  = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    let oldR = new FakeRecord(BookmarkFolder, oldRecord);
    let newR = new FakeRecord(Livemark, newRecord);
    oldR.parentid = PlacesUtils.bookmarks.toolbarGuid;
    newR.parentid = PlacesUtils.bookmarks.toolbarGuid;

    await store.applyIncoming(oldR);
    _("Applied old. It's a folder.");
    let oldID = await PlacesUtils.promiseItemId(oldR.id);
    _("Old ID: " + oldID);
    let oldInfo = await PlacesUtils.bookmarks.fetch(oldR.id);
    Assert.equal(oldInfo.type, PlacesUtils.bookmarks.TYPE_FOLDER);
    Assert.ok(!PlacesUtils.annotations
                          .itemHasAnnotation(oldID, PlacesUtils.LMANNO_FEEDURI));

    await store.applyIncoming(newR);
    let newID = await PlacesUtils.promiseItemId(newR.id);
    _("New ID: " + newID);

    _("Applied new. It's a livemark.");
    let newInfo = await PlacesUtils.bookmarks.fetch(newR.id);
    Assert.equal(newInfo.type, PlacesUtils.bookmarks.TYPE_FOLDER);
    Assert.ok(PlacesUtils.annotations
                         .itemHasAnnotation(newID, PlacesUtils.LMANNO_FEEDURI));

  } finally {
    await store.wipe();
    await engine.resetClient();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

add_task(async function test_bookmark_guidMap_fail() {
  _("Ensure that failures building the GUID map cause early death.");

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;

  let server = await serverForFoo(engine);
  let coll   = server.user("foo").collection("bookmarks");
  await SyncTestingInfrastructure(server);

  // Add one item to the server.
  let item = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "Folder 1",
  });
  let itemRecord = await store.createRecord(item.guid);
  let itemPayload = itemRecord.cleartext;
  coll.insert(item.guid, encryptPayload(itemPayload));

  engine.lastSync = 1; // So we don't back up.

  // Make building the GUID map fail.

  let pbt = PlacesUtils.promiseBookmarksTree;
  PlacesUtils.promiseBookmarksTree = function() { return Promise.reject("Nooo"); };

  // Ensure that we throw when calling getGuidMap().
  await engine._syncStartup();
  _("No error.");

  _("We get an error if building _guidMap fails in use.");
  let err;
  try {
    _(await engine.getGuidMap());
  } catch (ex) {
    err = ex;
  }
  Assert.equal(err.code, Engine.prototype.eEngineAbortApplyIncoming);
  Assert.equal(err.cause, "Nooo");

  _("We get an error and abort during processIncoming.");
  err = undefined;
  try {
    await engine._processIncoming();
  } catch (ex) {
    err = ex;
  }
  Assert.equal(err, "Nooo");

  _("Sync the engine and validate that we didn't put the error code in the wrong place");
  let ping;
  try {
    // Clear processIncoming so that we initialize the guid map inside uploadOutgoing
    engine._processIncoming = async function() {};
    await sync_engine_and_validate_telem(engine, true, p => { ping = p; });
  } catch (e) {}

  deepEqual(ping.engines.find(e => e.name == "bookmarks").failureReason, {
    name: "unexpectederror",
    error: "Nooo"
  });

  PlacesUtils.promiseBookmarksTree = pbt;
  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_bookmark_tag_but_no_uri() {
  _("Ensure that a bookmark record with tags, but no URI, doesn't throw an exception.");

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;

  // We're simply checking that no exception is thrown, so
  // no actual checks in this test.

  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    recordId: Utils.makeGUID(),
    parentRecordId: "toolbar",
    url: "http://example.com",
    tags: ["foo"],
  });
  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    recordId: Utils.makeGUID(),
    parentRecordId: "toolbar",
    url: "http://example.org",
    tags: null,
  });
  await PlacesSyncUtils.bookmarks.insert({
    kind: PlacesSyncUtils.bookmarks.KINDS.BOOKMARK,
    recordId: Utils.makeGUID(),
    url: "about:fake",
    parentRecordId: "toolbar",
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

  await store.create(record);
  record.tags = ["bar"];
  await store.update(record);
});

add_task(async function test_misreconciled_root() {
  _("Ensure that we don't reconcile an arbitrary record with a root.");

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  // Log real hard for this test.
  store._log.trace = store._log.debug;
  engine._log.trace = engine._log.debug;

  await engine._syncStartup();

  // Let's find out where the toolbar is right now.
  let toolbarBefore = await store.createRecord("toolbar", "bookmarks");
  let toolbarIDBefore = await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.toolbarGuid);
  Assert.notEqual(-1, toolbarIDBefore);

  let parentGUIDBefore = toolbarBefore.parentid;
  let parentIDBefore = await PlacesUtils.promiseItemId(
    PlacesSyncUtils.bookmarks.recordIdToGuid(parentGUIDBefore));
  Assert.notEqual(-1, parentIDBefore);
  Assert.equal("string", typeof(parentGUIDBefore));

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
  let toolbarAfter = await store.createRecord("toolbar", "bookmarks");
  let parentGUIDAfter = toolbarAfter.parentid;
  let parentIDAfter = await PlacesUtils.promiseItemId(
    PlacesSyncUtils.bookmarks.recordIdToGuid(parentGUIDAfter));
  Assert.equal((await PlacesUtils.promiseItemGuid(toolbarIDBefore)),
    PlacesUtils.bookmarks.toolbarGuid);
  Assert.equal(parentGUIDBefore, parentGUIDAfter);
  Assert.equal(parentIDBefore, parentIDAfter);

  await store.wipe();
  await engine.resetClient();
  await PlacesSyncUtils.bookmarks.reset();
  await promiseStopServer(server);
});

add_task(async function test_sync_dateAdded() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store  = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  // TODO: Avoid random orange (bug 1374599), this is only necessary
  // intermittently - reset the last sync date so that we'll get all bookmarks.
  engine.lastSync = 1;

  Svc.Obs.notify("weave:engine:start-tracking"); // We skip usual startup...

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

    // Update item2 and try resyncing it.
    item2.dateAdded = now - 100000;
    collection.insert(item2GUID, encryptPayload(item2.cleartext), now / 1000 - 50);


    // Also, add a local bookmark and make sure its date added makes it up to the server
    let bz = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://bugzilla.mozilla.org/",
      title: "Bugzilla",
    });

    // last sync did a POST, which doesn't advance its lastModified value.
    // Next sync of the engine doesn't hit info/collections, so lastModified
    // remains stale. Setting it to null side-steps that.
    engine.lastModified = null;
    await sync_engine_and_validate_telem(engine, false);

    let newRecord2 = await store.createRecord(item2GUID);
    equal(newRecord2.dateAdded, item2.dateAdded, "dateAdded update should work for earlier date");

    let bzWBO = JSON.parse(JSON.parse(collection._wbos[bz.guid].payload).ciphertext);
    ok(bzWBO.dateAdded, "Locally added dateAdded lost");

    let localRecord = await store.createRecord(bz.guid);
    equal(bzWBO.dateAdded, localRecord.dateAdded, "dateAdded should not change during upload");

    item2.dateAdded += 10000;
    collection.insert(item2GUID, encryptPayload(item2.cleartext), now / 1000 - 10);

    engine.lastModified = null;
    await sync_engine_and_validate_telem(engine, false);

    let newerRecord2 = await store.createRecord(item2GUID);
    equal(newerRecord2.dateAdded, newRecord2.dateAdded,
      "dateAdded update should be ignored for later date if we know an earlier one ");



  } finally {
    await store.wipe();
    await engine.resetClient();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

// Bug 890217.
add_task(async function test_sync_imap_URLs() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store  = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  Svc.Obs.notify("weave:engine:start-tracking"); // We skip usual startup...

  try {
    collection.insert("menu", encryptPayload({
      id: "menu",
      type: "folder",
      parentid: "places",
      title: "Bookmarks Menu",
      children: ["bookmarkAAAA"],
    }));
    collection.insert("bookmarkAAAA", encryptPayload({
      id: "bookmarkAAAA",
      type: "bookmark",
      parentid: "menu",
      bmkUri: "imap://vs@eleven.vs.solnicky.cz:993/fetch%3EUID%3E/" +
              "INBOX%3E56291?part=1.2&type=image/jpeg&filename=" +
              "invalidazPrahy.jpg",
      title: "invalidazPrahy.jpg (JPEG Image, 1280x1024 pixels) - Scaled (71%)",
    }));

    await PlacesUtils.bookmarks.insert({
      guid: "bookmarkBBBB",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "imap://eleven.vs.solnicky.cz:993/fetch%3EUID%3E/" +
           "CURRENT%3E2433?part=1.2&type=text/html&filename=TomEdwards.html",
      title: "TomEdwards.html",
    });

    await sync_engine_and_validate_telem(engine, false);

    let aInfo = await PlacesUtils.bookmarks.fetch("bookmarkAAAA");
    equal(aInfo.url.href, "imap://vs@eleven.vs.solnicky.cz:993/" +
      "fetch%3EUID%3E/INBOX%3E56291?part=1.2&type=image/jpeg&filename=" +
      "invalidazPrahy.jpg",
      "Remote bookmark A with IMAP URL should exist locally");

    let bPayload = JSON.parse(JSON.parse(
      collection.payload("bookmarkBBBB")).ciphertext);
    equal(bPayload.bmkUri, "imap://eleven.vs.solnicky.cz:993/" +
      "fetch%3EUID%3E/CURRENT%3E2433?part=1.2&type=text/html&filename=" +
      "TomEdwards.html",
      "Local bookmark B with IMAP URL should exist remotely");
  } finally {
    await store.wipe();
    Svc.Prefs.resetBranch("");
    Service.recordManager.clearCache();
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});
