/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { BookmarkHTMLUtils } = ChromeUtils.import(
  "resource://gre/modules/BookmarkHTMLUtils.jsm"
);
const { BookmarkJSONUtils } = ChromeUtils.import(
  "resource://gre/modules/BookmarkJSONUtils.jsm"
);
const { SyncedBookmarksMirror } = ChromeUtils.import(
  "resource://gre/modules/SyncedBookmarksMirror.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const {
  Bookmark,
  BookmarkFolder,
  BookmarksEngine,
  BufferedBookmarksEngine,
  Livemark,
} = ChromeUtils.import("resource://services-sync/engines/bookmarks.js");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

var recordedEvents = [];

function checkRecordedEvents(object, expected, message) {
  // Ignore event telemetry from the merger.
  let repairEvents = recordedEvents.filter(event => event.object == object);
  deepEqual(repairEvents, expected, message);
  // and clear the list so future checks are easier to write.
  recordedEvents = [];
}

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
      row.getResultByName("guid")
    );
    recordIds.add(recordId);
  }
  return recordIds;
}

async function cleanupEngine(engine) {
  await engine._tracker.stop();
  await engine._store.wipe();
  await engine.resetClient();
  Svc.Prefs.resetBranch("");
  Service.recordManager.clearCache();
}

async function cleanup(engine, server) {
  await promiseStopServer(server);
  await cleanupEngine(engine);
}

add_task(async function setup() {
  await generateNewKeys(Service.collectionKeys);
  await Service.engineManager.unregister("bookmarks");

  Service.recordTelemetryEvent = (object, method, value, extra = undefined) => {
    recordedEvents.push({ object, method, value, extra });
  };
});

function add_bookmark_test(task) {
  add_task(async function() {
    _(`Running test ${task.name} with legacy bookmarks engine`);
    let legacyEngine = new BookmarksEngine(Service);
    await legacyEngine.initialize();
    try {
      await task(legacyEngine);
    } finally {
      await legacyEngine.finalize();
    }

    _(`Running test ${task.name} with buffered bookmarks engine`);
    let bufferedEngine = new BufferedBookmarksEngine(Service);
    await bufferedEngine.initialize();
    try {
      await task(bufferedEngine);
    } finally {
      await bufferedEngine.finalize();
    }
  });
}

add_task(async function test_buffer_timeout() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BufferedBookmarksEngine(Service);
  // Abort merges immediately.
  engine._applyTimeout = 0;
  await engine.initialize();
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);
  let collection = server.user("foo").collection("bookmarks");

  try {
    info("Insert local bookmarks");
    await PlacesUtils.bookmarks.insertTree({
      guid: PlacesUtils.bookmarks.unfiledGuid,
      children: [
        {
          guid: "bookmarkAAAA",
          url: "http://example.com/a",
          title: "A",
        },
        {
          guid: "bookmarkBBBB",
          url: "http://example.com/b",
          title: "B",
        },
      ],
    });

    info("Insert remote bookmarks");
    collection.insert(
      "menu",
      encryptPayload({
        id: "menu",
        type: "folder",
        parentid: "places",
        title: "menu",
        children: ["bookmarkCCCC", "bookmarkDDDD"],
      })
    );
    collection.insert(
      "bookmarkCCCC",
      encryptPayload({
        id: "bookmarkCCCC",
        type: "bookmark",
        parentid: "menu",
        bmkUri: "http://example.com/c",
        title: "C",
      })
    );
    collection.insert(
      "bookmarkDDDD",
      encryptPayload({
        id: "bookmarkDDDD",
        type: "bookmark",
        parentid: "menu",
        bmkUri: "http://example.com/d",
        title: "D",
      })
    );

    info("We expect this sync to fail");
    await Assert.rejects(
      sync_engine_and_validate_telem(engine, true),
      ex => ex.name == "InterruptedError"
    );
  } finally {
    await cleanup(engine, server);
    await engine.finalize();
  }
});

add_bookmark_test(async function test_maintenance_after_failure(engine) {
  _("Ensure we try to run maintenance if the engine fails to sync");

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    let syncStartup = engine._syncStartup;
    let syncError = new Error("Something is rotten in the state of Places");
    engine._syncStartup = function() {
      throw syncError;
    };

    Services.prefs.clearUserPref("places.database.lastMaintenance");

    _("Ensure the sync fails and we run maintenance");
    await Assert.rejects(
      sync_engine_and_validate_telem(engine, true),
      ex => ex == syncError
    );
    checkRecordedEvents(
      "maintenance",
      [
        {
          object: "maintenance",
          method: "run",
          value: "bookmarks",
          extra: undefined,
        },
      ],
      "Should record event for first maintenance run"
    );

    _("Sync again, but ensure maintenance doesn't run");
    await Assert.rejects(
      sync_engine_and_validate_telem(engine, true),
      ex => ex == syncError
    );
    checkRecordedEvents(
      "maintenance",
      [],
      "Should not record event if maintenance didn't run"
    );

    _("Fast-forward last maintenance pref; ensure maintenance runs");
    Services.prefs.setIntPref(
      "places.database.lastMaintenance",
      Date.now() / 1000 - 14400
    );
    await Assert.rejects(
      sync_engine_and_validate_telem(engine, true),
      ex => ex == syncError
    );
    checkRecordedEvents(
      "maintenance",
      [
        {
          object: "maintenance",
          method: "run",
          value: "bookmarks",
          extra: undefined,
        },
      ],
      "Should record event for second maintenance run"
    );

    _("Fix sync failure; ensure we report success after maintenance");
    engine._syncStartup = syncStartup;
    await sync_engine_and_validate_telem(engine, false);
    checkRecordedEvents(
      "maintenance",
      [
        {
          object: "maintenance",
          method: "fix",
          value: "bookmarks",
          extra: undefined,
        },
      ],
      "Should record event for successful sync after second maintenance"
    );

    await sync_engine_and_validate_telem(engine, false);
    checkRecordedEvents(
      "maintenance",
      [],
      "Should not record maintenance events after successful sync"
    );
  } finally {
    await cleanup(engine, server);
  }
});

add_bookmark_test(async function test_delete_invalid_roots_from_server(engine) {
  _("Ensure that we delete the Places and Reading List roots from the server.");

  enableValidationPrefs();

  let store = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  engine._tracker.start();

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

    // Note that we don't insert a record for the toolbar, so the buffered
    // engine will report a parent-child disagreement, since Firefox's
    // `parentid` is `toolbar`.
    let newBmk = new Bookmark("bookmarks", Utils.makeGUID());
    newBmk.bmkUri = "http://getfirefox.com";
    newBmk.title = "Get Firefox!";
    newBmk.parentName = "Bookmarks Toolbar";
    newBmk.parentid = "toolbar";
    collection.insert(newBmk.id, encryptPayload(newBmk.cleartext));

    deepEqual(
      collection.keys().sort(),
      ["places", "readinglist", listBmk.id, newBmk.id].sort(),
      "Should store Places root, reading list items, and new bookmark on server"
    );

    if (engine instanceof BufferedBookmarksEngine) {
      let ping = await sync_engine_and_validate_telem(engine, true);
      if (engine instanceof BufferedBookmarksEngine) {
        // In a real sync, the buffered engine is named `bookmarks-buffered`.
        // However, `sync_engine_and_validate_telem` simulates a sync, where
        // the engine isn't registered with the engine manager, so the recorder
        // doesn't see its `overrideTelemetryName`.
        let engineData = ping.engines.find(e => e.name == "bookmarks");
        ok(
          engineData.validation,
          "Buffered engine should always run validation"
        );
        equal(
          engineData.validation.checked,
          6,
          "Buffered engine should validate all items"
        );
        deepEqual(
          engineData.validation.problems,
          [
            {
              name: "parentChildDisagreements",
              count: 1,
            },
          ],
          "Buffered engine should report parent-child disagreement"
        );
        deepEqual(
          engineData.steps.map(step => step.name),
          [
            "fetchLocalTree",
            "fetchRemoteTree",
            "merge",
            "apply",
            "notifyObservers",
            "fetchLocalChangeRecords",
          ],
          "Buffered engine should report all merge steps"
        );
      }
    } else {
      // The legacy engine doesn't report validation failures for this case,
      // so we disallow error pings.
      await sync_engine_and_validate_telem(engine, false);
    }

    await Assert.rejects(
      PlacesUtils.promiseItemId("readinglist"),
      /no item found for the given GUID/,
      "Should not apply Reading List root"
    );
    await Assert.rejects(
      PlacesUtils.promiseItemId(listBmk.id),
      /no item found for the given GUID/,
      "Should not apply items in Reading List"
    );
    ok(
      (await PlacesUtils.promiseItemId(newBmk.id)) > 0,
      "Should apply new bookmark"
    );

    deepEqual(
      collection.keys().sort(),
      ["menu", "mobile", "toolbar", "unfiled", newBmk.id].sort(),
      "Should remove Places root and reading list items from server; upload local roots"
    );
  } finally {
    await cleanup(engine, server);
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

add_bookmark_test(async function test_processIncoming_error_orderChildren(
  engine
) {
  _(
    "Ensure that _orderChildren() is called even when _processIncoming() throws an error."
  );

  let store = engine._store;
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

    let toolbar_record = await store.createRecord("toolbar");
    collection.insert("toolbar", encryptPayload(toolbar_record.cleartext));

    let bmk1_record = await store.createRecord(bmk1.guid);
    collection.insert(bmk1.guid, encryptPayload(bmk1_record.cleartext));

    let bmk2_record = await store.createRecord(bmk2.guid);
    collection.insert(bmk2.guid, encryptPayload(bmk2_record.cleartext));

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
    bogus_record.modified = new_timestamp() - 60 * 10;
    await engine.setLastSync(new_timestamp() - 60);
    engine.toFetch = new SerializableSet([BOGUS_GUID]);

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
    Assert.deepEqual(
      new_children.sort(),
      [folder1_payload.children[0], folder1_payload.children[1]].sort()
    );

    let localChildIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
      folder1.guid
    );
    Assert.deepEqual(localChildIds.sort(), [bmk2.guid, bmk1.guid].sort());
  } finally {
    await cleanup(engine, server);
  }
});

add_bookmark_test(async function test_restorePromptsReupload(engine) {
  await test_restoreOrImport(engine, { replace: true });
});

add_bookmark_test(async function test_importPromptsReupload(engine) {
  await test_restoreOrImport(engine, { replace: false });
});

// Test a JSON restore or HTML import. Use JSON if `replace` is `true`, or
// HTML otherwise.
async function test_restoreOrImport(engine, { replace }) {
  let verb = replace ? "restore" : "import";
  let verbing = replace ? "restoring" : "importing";
  let bookmarkUtils = replace ? BookmarkJSONUtils : BookmarkHTMLUtils;

  _(`Ensure that ${verbing} from a backup will reupload all records.`);

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  engine._tracker.start(); // We skip usual startup...

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
      OS.Constants.Path.tmpDir,
      `t_b_e_${Date.now()}.json`
    );

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

    _(
      "Verify that there's only one bookmark on the server, and it's Thunderbird."
    );
    // Of course, there's also the Bookmarks Toolbar and Bookmarks Menu...
    let wbos = collection.keys(function(id) {
      return !["menu", "toolbar", "mobile", "unfiled", folder1.guid].includes(
        id
      );
    });
    Assert.equal(wbos.length, 1);
    Assert.equal(wbos[0], bmk2.guid);

    _(`Now ${verb} from a backup.`);
    await bookmarkUtils.importFromFile(backupFilePath, { replace });

    // If `replace` is `true`, we'll wipe the server on the next sync.
    let bookmarksCollection = server.user("foo").collection("bookmarks");
    _("Verify that we didn't wipe the server.");
    Assert.ok(!!bookmarksCollection);

    _("Ensure we have the bookmarks we expect locally.");
    let recordIds = await fetchAllRecordIds();
    _("GUIDs: " + JSON.stringify([...recordIds]));

    let bookmarkRecordIds = new Map();
    let count = 0;
    for (let recordId of recordIds) {
      count++;
      let info = await PlacesUtils.bookmarks.fetch(
        PlacesSyncUtils.bookmarks.recordIdToGuid(recordId)
      );
      // Only one bookmark, so _all_ should be Firefox!
      if (info.type == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
        _(`Found URI ${info.url.href} for record ID ${recordId}`);
        bookmarkRecordIds.set(info.url.href, recordId);
      }
    }
    Assert.ok(bookmarkRecordIds.has("http://getfirefox.com/"));
    if (!replace) {
      Assert.ok(bookmarkRecordIds.has("http://getthunderbird.com/"));
    }

    _("Have the correct number of IDs locally, too.");
    let expectedResults = [
      "menu",
      "toolbar",
      "mobile",
      "unfiled",
      folder1.guid,
      bmk1.guid,
    ];
    if (!replace) {
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
    let payloads = server
      .user("foo")
      .collection("bookmarks")
      .payloads();
    let bookmarkWBOs = payloads.filter(function(wbo) {
      return wbo.type == "bookmark";
    });

    let folderWBOs = payloads.filter(function(wbo) {
      return (
        wbo.type == "folder" &&
        wbo.id != "menu" &&
        wbo.id != "toolbar" &&
        wbo.id != "unfiled" &&
        wbo.id != "mobile" &&
        wbo.parentid != "menu"
      );
    });

    let expectedFX = {
      id: bookmarkRecordIds.get("http://getfirefox.com/"),
      bmkUri: "http://getfirefox.com/",
      title: "Get Firefox!",
    };
    let expectedTB = {
      id: bookmarkRecordIds.get("http://getthunderbird.com/"),
      bmkUri: "http://getthunderbird.com/",
      title: "Get Thunderbird!",
    };

    let expectedBookmarks;
    if (replace) {
      expectedBookmarks = [expectedFX];
    } else {
      expectedBookmarks = [expectedTB, expectedFX];
    }

    doCheckWBOs(bookmarkWBOs, expectedBookmarks);

    _("Our old friend Folder 1 is still in play.");
    let expectedFolder1 = { title: "Folder 1" };

    let expectedFolders;
    if (replace) {
      expectedFolders = [expectedFolder1];
    } else {
      expectedFolders = [expectedFolder1, expectedFolder1];
    }

    doCheckWBOs(folderWBOs, expectedFolders);
  } finally {
    await cleanup(engine, server);
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
  this.cleartextToString = constructor.prototype.cleartextToString;
}

// Bug 632287.
add_task(async function test_mismatched_types() {
  _(
    "Ensure that handling a record that changes type causes deletion " +
      "then re-adding."
  );

  let oldRecord = {
    id: "l1nZZXfB8nC7",
    type: "folder",
    parentName: "Bookmarks Toolbar",
    title: "Innerst i Sneglehode",
    description: null,
    parentid: "toolbar",
  };
  oldRecord.cleartext = oldRecord;

  let newRecord = {
    id: "l1nZZXfB8nC7",
    type: "livemark",
    siteUri: "http://sneglehode.wordpress.com/",
    feedUri: "http://sneglehode.wordpress.com/feed/",
    parentName: "Bookmarks Toolbar",
    title: "Innerst i Sneglehode",
    description: null,
    children: [
      "HCRq40Rnxhrd",
      "YeyWCV1RVsYw",
      "GCceVZMhvMbP",
      "sYi2hevdArlF",
      "vjbZlPlSyGY8",
      "UtjUhVyrpeG6",
      "rVq8WMG2wfZI",
      "Lx0tcy43ZKhZ",
      "oT74WwV8_j4P",
      "IztsItWVSo3-",
    ],
    parentid: "toolbar",
  };
  newRecord.cleartext = newRecord;

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;
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

    await store.applyIncoming(newR);
    await Assert.rejects(
      PlacesUtils.promiseItemId(newR.id),
      /no item found for the given GUID/,
      "Should not apply Livemark"
    );
  } finally {
    await cleanup(engine, server);
    await engine.finalize();
  }
});

add_task(async function test_bookmark_guidMap_fail() {
  _("Ensure that failures building the GUID map cause early death.");

  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let store = engine._store;

  let server = await serverForFoo(engine);
  let coll = server.user("foo").collection("bookmarks");
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

  await engine.setLastSync(1); // So we don't back up.

  // Make building the GUID map fail.

  let pbt = PlacesUtils.promiseBookmarksTree;
  PlacesUtils.promiseBookmarksTree = function() {
    return Promise.reject("Nooo");
  };

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
  Assert.equal(err.code, SyncEngine.prototype.eEngineAbortApplyIncoming);
  Assert.equal(err.cause, "Nooo");

  _("We get an error and abort during processIncoming.");
  err = undefined;
  try {
    await engine._processIncoming();
  } catch (ex) {
    err = ex;
  }
  Assert.equal(err, "Nooo");

  _(
    "Sync the engine and validate that we didn't put the error code in the wrong place"
  );
  let ping;
  try {
    // Clear processIncoming so that we initialize the guid map inside uploadOutgoing
    engine._processIncoming = async function() {};
    await sync_engine_and_validate_telem(engine, true, p => {
      ping = p;
    });
  } catch (e) {}

  deepEqual(ping.engines.find(e => e.name == "bookmarks").failureReason, {
    name: "unexpectederror",
    error: "Nooo",
  });

  PlacesUtils.promiseBookmarksTree = pbt;
  await cleanup(engine, server);
  await engine.finalize();
});

add_task(async function test_bookmark_tag_but_no_uri() {
  _(
    "Ensure that a bookmark record with tags, but no URI, doesn't throw an exception."
  );

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
    parentid: "toolbar",
    id: Utils.makeGUID(),
    description: "",
    tags: ["foo"],
    title: "Taggy tag",
    type: "folder",
  });

  await store.create(record);
  record.tags = ["bar"];
  await store.update(record);

  await cleanupEngine(engine);
  await engine.finalize();
});

add_bookmark_test(async function test_misreconciled_root(engine) {
  _("Ensure that we don't reconcile an arbitrary record with a root.");

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
    PlacesUtils.bookmarks.toolbarGuid
  );
  Assert.notEqual(-1, toolbarIDBefore);

  let parentRecordIDBefore = toolbarBefore.parentid;
  let parentGUIDBefore = PlacesSyncUtils.bookmarks.recordIdToGuid(
    parentRecordIDBefore
  );
  let parentIDBefore = await PlacesUtils.promiseItemId(parentGUIDBefore);
  Assert.equal("string", typeof parentGUIDBefore);

  _("Current parent: " + parentGUIDBefore + " (" + parentIDBefore + ").");

  let to_apply = {
    id: "zzzzzzzzzzzz",
    type: "folder",
    title: "Bookmarks Toolbar",
    description: "Now you're for it.",
    parentName: "",
    parentid: "mobile", // Why not?
    children: [],
  };

  let rec = new FakeRecord(BookmarkFolder, to_apply);

  _("Applying record.");
  store.applyIncomingBatch([rec]);

  // Ensure that afterwards, toolbar is still there.
  // As of 2012-12-05, this only passes because Places doesn't use "toolbar" as
  // the real GUID, instead using a generated one. Sync does the translation.
  let toolbarAfter = await store.createRecord("toolbar", "bookmarks");
  let parentRecordIDAfter = toolbarAfter.parentid;
  let parentGUIDAfter = PlacesSyncUtils.bookmarks.recordIdToGuid(
    parentRecordIDAfter
  );
  let parentIDAfter = await PlacesUtils.promiseItemId(parentGUIDAfter);
  Assert.equal(
    await PlacesUtils.promiseItemGuid(toolbarIDBefore),
    PlacesUtils.bookmarks.toolbarGuid
  );
  Assert.equal(parentGUIDBefore, parentGUIDAfter);
  Assert.equal(parentIDBefore, parentIDAfter);

  await cleanup(engine, server);
});

add_bookmark_test(async function test_sync_dateAdded(engine) {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let store = engine._store;
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  // TODO: Avoid random orange (bug 1374599), this is only necessary
  // intermittently - reset the last sync date so that we'll get all bookmarks.
  await engine.setLastSync(1);

  engine._tracker.start(); // We skip usual startup...

  // Just matters that it's in the past, not how far.
  let now = Date.now();
  let oneYearMS = 365 * 24 * 60 * 60 * 1000;

  try {
    let toolbar = new BookmarkFolder("bookmarks", "toolbar");
    toolbar.title = "toolbar";
    toolbar.parentName = "";
    toolbar.parentid = "places";
    toolbar.children = [
      "abcdefabcdef",
      "aaaaaaaaaaaa",
      "bbbbbbbbbbbb",
      "cccccccccccc",
      "dddddddddddd",
      "eeeeeeeeeeee",
    ];
    collection.insert("toolbar", encryptPayload(toolbar.cleartext));

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
    collection.insert(
      item2GUID,
      encryptPayload(item2.cleartext),
      item2LastModified
    );

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
    collection.insert(
      item4GUID,
      encryptPayload(item4.cleartext),
      item4LastModified
    );

    let item5GUID = "dddddddddddd";
    let item5 = new Bookmark("bookmarks", item5GUID);
    item5.bmkUri = "https://example.com/5";
    item5.title = "asdf5";
    item5.parentName = "Bookmarks Toolbar";
    item5.parentid = "toolbar";
    // no dateAdded, lastModified in (near) future.
    const item5LastModified = (now + 60000) / 1000;
    collection.insert(
      item5GUID,
      encryptPayload(item5.cleartext),
      item5LastModified
    );

    let item6GUID = "eeeeeeeeeeee";
    let item6 = new Bookmark("bookmarks", item6GUID);
    item6.bmkUri = "https://example.com/6";
    item6.title = "asdf6";
    item6.parentName = "Bookmarks Toolbar";
    item6.parentid = "toolbar";
    const item6LastModified = (now - oneYearMS) / 1000;
    collection.insert(
      item6GUID,
      encryptPayload(item6.cleartext),
      item6LastModified
    );

    await sync_engine_and_validate_telem(engine, false);

    let record1 = await store.createRecord(item1GUID);
    let record2 = await store.createRecord(item2GUID);

    equal(
      item1.dateAdded,
      record1.dateAdded,
      "dateAdded in past should be synced"
    );
    equal(
      record2.dateAdded,
      item2LastModified * 1000,
      "dateAdded in future should be ignored in favor of last modified"
    );

    let record3 = await store.createRecord(item3GUID);

    ok(record3.dateAdded);
    // Make sure it's within 24 hours of the right timestamp... This is a little
    // dodgey but we only really care that it's basically accurate and has the
    // right day.
    ok(Math.abs(Date.now() - record3.dateAdded) < 24 * 60 * 60 * 1000);

    let record4 = await store.createRecord(item4GUID);
    equal(
      record4.dateAdded,
      item4LastModified * 1000,
      "If no dateAdded is provided, lastModified should be used"
    );

    let record5 = await store.createRecord(item5GUID);
    equal(
      record5.dateAdded,
      item5LastModified * 1000,
      "If no dateAdded is provided, lastModified should be used (even if it's in the future)"
    );

    // Update item2 and try resyncing it.
    item2.dateAdded = now - 100000;
    collection.insert(
      item2GUID,
      encryptPayload(item2.cleartext),
      now / 1000 - 50
    );

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
    equal(
      newRecord2.dateAdded,
      item2.dateAdded,
      "dateAdded update should work for earlier date"
    );

    let bzWBO = collection.cleartext(bz.guid);
    ok(bzWBO.dateAdded, "Locally added dateAdded lost");

    let localRecord = await store.createRecord(bz.guid);
    equal(
      bzWBO.dateAdded,
      localRecord.dateAdded,
      "dateAdded should not change during upload"
    );

    item2.dateAdded += 10000;
    collection.insert(
      item2GUID,
      encryptPayload(item2.cleartext),
      now / 1000 - 10
    );

    engine.lastModified = null;
    await sync_engine_and_validate_telem(engine, false);

    let newerRecord2 = await store.createRecord(item2GUID);
    equal(
      newerRecord2.dateAdded,
      newRecord2.dateAdded,
      "dateAdded update should be ignored for later date if we know an earlier one "
    );
  } finally {
    await cleanup(engine, server);
  }
});

add_task(async function test_buffer_hasDupe() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BufferedBookmarksEngine(Service);
  await engine.initialize();
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);
  let collection = server.user("foo").collection("bookmarks");
  engine._tracker.start(); // We skip usual startup...
  try {
    let guid1 = Utils.makeGUID();
    let guid2 = Utils.makeGUID();
    await PlacesUtils.bookmarks.insert({
      guid: guid1,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "https://www.example.com",
      title: "example.com",
    });
    await PlacesUtils.bookmarks.insert({
      guid: guid2,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "https://www.example.com",
      title: "example.com",
    });

    await sync_engine_and_validate_telem(engine, false);
    // Make sure we set hasDupe on outgoing records
    Assert.ok(collection.payloads().every(payload => payload.hasDupe));

    await PlacesUtils.bookmarks.remove(guid1);

    // Make sure it works for weakly uploaded records
    engine.addForWeakUpload(guid2);

    await sync_engine_and_validate_telem(engine, false);

    let tombstone = JSON.parse(
      JSON.parse(collection.payload(guid1)).ciphertext
    );
    // We shouldn't set hasDupe on tombstones.
    Assert.ok(tombstone.deleted);
    Assert.ok(!tombstone.hasDupe);

    let record = JSON.parse(JSON.parse(collection.payload(guid2)).ciphertext);
    // We should set hasDupe on weakly uploaded records.
    Assert.ok(!record.deleted);
    Assert.ok(
      record.hasDupe,
      "Buffered bookmark engine should set hasDupe for weakly uploaded records."
    );

    await sync_engine_and_validate_telem(engine, false);
  } finally {
    await cleanup(engine, server);
    await engine.finalize();
  }
});

// Bug 890217.
add_task(async function test_sync_imap_URLs() {
  await Service.recordManager.clearCache();
  await PlacesSyncUtils.bookmarks.reset();
  let engine = new BookmarksEngine(Service);
  await engine.initialize();
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  engine._tracker.start(); // We skip usual startup...

  try {
    collection.insert(
      "menu",
      encryptPayload({
        id: "menu",
        type: "folder",
        parentid: "places",
        title: "Bookmarks Menu",
        children: ["bookmarkAAAA"],
      })
    );
    collection.insert(
      "bookmarkAAAA",
      encryptPayload({
        id: "bookmarkAAAA",
        type: "bookmark",
        parentid: "menu",
        bmkUri:
          "imap://vs@eleven.vs.solnicky.cz:993/fetch%3EUID%3E/" +
          "INBOX%3E56291?part=1.2&type=image/jpeg&filename=" +
          "invalidazPrahy.jpg",
        title:
          "invalidazPrahy.jpg (JPEG Image, 1280x1024 pixels) - Scaled (71%)",
      })
    );

    await PlacesUtils.bookmarks.insert({
      guid: "bookmarkBBBB",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url:
        "imap://eleven.vs.solnicky.cz:993/fetch%3EUID%3E/" +
        "CURRENT%3E2433?part=1.2&type=text/html&filename=TomEdwards.html",
      title: "TomEdwards.html",
    });

    await sync_engine_and_validate_telem(engine, false);

    let aInfo = await PlacesUtils.bookmarks.fetch("bookmarkAAAA");
    equal(
      aInfo.url.href,
      "imap://vs@eleven.vs.solnicky.cz:993/" +
        "fetch%3EUID%3E/INBOX%3E56291?part=1.2&type=image/jpeg&filename=" +
        "invalidazPrahy.jpg",
      "Remote bookmark A with IMAP URL should exist locally"
    );

    let bPayload = collection.cleartext("bookmarkBBBB");
    equal(
      bPayload.bmkUri,
      "imap://eleven.vs.solnicky.cz:993/" +
        "fetch%3EUID%3E/CURRENT%3E2433?part=1.2&type=text/html&filename=" +
        "TomEdwards.html",
      "Local bookmark B with IMAP URL should exist remotely"
    );
  } finally {
    await cleanup(engine, server);
    await engine.finalize();
  }
});

add_task(async function test_resume_buffer() {
  await Service.recordManager.clearCache();
  let engine = new BufferedBookmarksEngine(Service);
  await engine.initialize();
  await engine._store.wipe();
  await engine.resetClient();

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");

  engine._tracker.start(); // We skip usual startup...

  const batchChunkSize = 50;

  engine._store._batchChunkSize = batchChunkSize;
  try {
    let children = [];

    let timestamp = round_timestamp(Date.now());
    // Add two chunks worth of records to the server
    for (let i = 0; i < batchChunkSize * 2; ++i) {
      let cleartext = {
        id: Utils.makeGUID(),
        type: "bookmark",
        parentid: "toolbar",
        title: `Bookmark ${i}`,
        parentName: "Bookmarks Toolbar",
        bmkUri: `https://example.com/${i}`,
      };
      let wbo = collection.insert(
        cleartext.id,
        encryptPayload(cleartext),
        timestamp + 10 * i
      );
      // Something that is effectively random, but deterministic.
      // (This is just to ensure we don't accidentally start using the
      // sortindex again).
      wbo.sortindex = 1000 + Math.round(Math.sin(i / 5) * 100);
      children.push(cleartext.id);
    }

    // Add the parent of those records, and ensure its timestamp is the most recent.
    collection.insert(
      "toolbar",
      encryptPayload({
        id: "toolbar",
        type: "folder",
        parentid: "places",
        title: "Bookmarks Toolbar",
        children,
      }),
      timestamp + 10 * children.length
    );

    // Replace applyIncomingBatch with a custom one that calls the original,
    // but forces it to throw on the 2nd chunk.
    let origApplyIncomingBatch = engine._store.applyIncomingBatch;
    engine._store.applyIncomingBatch = function(records) {
      if (records.length > batchChunkSize) {
        // Hacky way to make reading from the batchChunkSize'th record throw.
        delete records[batchChunkSize];
        Object.defineProperty(records, batchChunkSize, {
          get() {
            throw new Error("D:");
          },
        });
      }
      return origApplyIncomingBatch.call(this, records);
    };

    let caughtError;
    _("We expect this to fail");
    try {
      await sync_engine_and_validate_telem(engine, true);
    } catch (e) {
      caughtError = e;
    }
    Assert.ok(caughtError, "Expected engine.sync to throw");
    Assert.equal(caughtError.message, "D:");

    // The buffer subtracts one second from the actual timestamp.
    let lastSync = (await engine.getLastSync()) + 1;
    // We poisoned the batchChunkSize'th record, so the last successfully
    // applied record will be batchChunkSize - 1.
    let expectedLastSync = timestamp + 10 * (batchChunkSize - 1);
    Assert.equal(expectedLastSync, lastSync);

    engine._store.applyIncomingBatch = origApplyIncomingBatch;

    await sync_engine_and_validate_telem(engine, false);

    // Check that all the children made it onto the correct record.
    let toolbarRecord = await engine._store.createRecord("toolbar");
    Assert.deepEqual(toolbarRecord.children.sort(), children.sort());
  } finally {
    await cleanup(engine, server);
    await engine.finalize();
  }
});

// The buffered engine stores the sync ID and last sync time in three places:
// prefs, Places, and the mirror. We can remove the prefs entirely in bug
// 1443021, and drop the last sync time from Places once we remove the legacy
// engine. This test ensures we keep them in sync (^_^), and handle mismatches
// in case the user copies Places or the mirror between accounts. See
// bug 1199077, comment 84 for the gory details.
add_task(async function test_mirror_syncID() {
  let bufferedEngine = new BufferedBookmarksEngine(Service);
  await bufferedEngine.initialize();
  let buf = await bufferedEngine._store.ensureOpenMirror();

  info("Places and mirror don't have sync IDs");

  let syncID = await bufferedEngine.resetLocalSyncID();

  equal(
    Svc.Prefs.get(`${bufferedEngine.name}.syncID`),
    syncID,
    "Should reset sync ID in prefs"
  );
  strictEqual(
    Svc.Prefs.get(`${bufferedEngine.name}.lastSync`),
    "0",
    "Should reset last sync in prefs"
  );

  equal(
    await PlacesSyncUtils.bookmarks.getSyncId(),
    syncID,
    "Should reset sync ID in Places"
  );
  strictEqual(
    await PlacesSyncUtils.bookmarks.getLastSync(),
    0,
    "Should reset last sync in Places"
  );

  equal(await buf.getSyncId(), syncID, "Should reset sync ID in mirror");
  strictEqual(
    await buf.getCollectionHighWaterMark(),
    0,
    "Should reset high water mark in mirror"
  );

  info("Places and mirror have matching sync ID");

  await bufferedEngine.setLastSync(123.45);
  await bufferedEngine.ensureCurrentSyncID(syncID);

  equal(
    Svc.Prefs.get(`${bufferedEngine.name}.syncID`),
    syncID,
    "Should keep sync ID in prefs if Places and mirror match"
  );
  strictEqual(
    Svc.Prefs.get(`${bufferedEngine.name}.lastSync`),
    "123.45",
    "Should keep last sync in prefs if Places and mirror match"
  );

  equal(
    await PlacesSyncUtils.bookmarks.getSyncId(),
    syncID,
    "Should keep sync ID in Places if Places and mirror match"
  );
  strictEqual(
    await PlacesSyncUtils.bookmarks.getLastSync(),
    123.45,
    "Should keep last sync in Places if Places and mirror match"
  );

  equal(await buf.getSyncId(), syncID, "Should keep sync ID in mirror");
  equal(
    await buf.getCollectionHighWaterMark(),
    123.45,
    "Should keep high water mark in mirror"
  );

  info("Places and mirror have different sync IDs");

  // Directly update the sync ID in the mirror, without resetting.
  await buf.db.execute(`UPDATE meta SET value = :value WHERE key = :key`, {
    key: SyncedBookmarksMirror.META_KEY.SYNC_ID,
    value: "syncIdAAAAAA",
  });
  await bufferedEngine.ensureCurrentSyncID(syncID);

  equal(
    Svc.Prefs.get(`${bufferedEngine.name}.syncID`),
    syncID,
    "Should keep sync ID in prefs if Places and mirror don't match"
  );
  strictEqual(
    Svc.Prefs.get(`${bufferedEngine.name}.lastSync`),
    "123.45",
    "Should keep last sync in prefs if Places and mirror don't match"
  );

  equal(
    await PlacesSyncUtils.bookmarks.getSyncId(),
    syncID,
    "Should keep existing sync ID in Places on mirror sync ID mismatch"
  );
  strictEqual(
    await PlacesSyncUtils.bookmarks.getLastSync(),
    123.45,
    "Should keep existing last sync in Places on mirror sync ID mismatch"
  );

  equal(
    await buf.getSyncId(),
    syncID,
    "Should reset mismatched sync ID in mirror"
  );
  strictEqual(
    await buf.getCollectionHighWaterMark(),
    0,
    "Should reset high water mark on mirror sync ID mismatch"
  );

  info("Places has sync ID; mirror missing sync ID");
  await buf.reset();

  equal(
    await bufferedEngine.ensureCurrentSyncID(syncID),
    syncID,
    "Should not assign new sync ID if Places has sync ID; mirror missing"
  );
  equal(
    await buf.getSyncId(),
    syncID,
    "Should set sync ID in mirror to match Places"
  );

  info("Places missing sync ID; mirror has sync ID");

  await buf.setCollectionLastModified(123.45);
  await PlacesSyncUtils.bookmarks.reset();
  let newSyncID = await bufferedEngine.ensureCurrentSyncID("syncIdBBBBBB");

  equal(
    Svc.Prefs.get(`${bufferedEngine.name}.syncID`),
    newSyncID,
    "Should set new sync ID in prefs"
  );
  strictEqual(
    Svc.Prefs.get(`${bufferedEngine.name}.lastSync`),
    "0",
    "Should reset last sync in prefs on sync ID change"
  );

  equal(
    await PlacesSyncUtils.bookmarks.getSyncId(),
    newSyncID,
    "Should set new sync ID in Places"
  );
  equal(
    await buf.getSyncId(),
    newSyncID,
    "Should update new sync ID in mirror"
  );

  strictEqual(
    await buf.getCollectionHighWaterMark(),
    0,
    "Should reset high water mark on sync ID change in Places"
  );

  await cleanupEngine(bufferedEngine);
  await bufferedEngine.finalize();
});

add_bookmark_test(async function test_livemarks(engine) {
  _("Ensure we replace new and existing livemarks with tombstones");

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("bookmarks");
  let now = Date.now();

  try {
    _("Insert existing livemark");
    let modifiedForA = now - 5 * 60 * 1000;
    await PlacesUtils.bookmarks.insert({
      guid: "livemarkAAAA",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      title: "A",
      lastModified: new Date(modifiedForA),
      dateAdded: new Date(modifiedForA),
      source: PlacesUtils.bookmarks.SOURCE_SYNC,
    });
    collection.insert(
      "menu",
      encryptPayload({
        id: "menu",
        type: "folder",
        parentName: "",
        title: "menu",
        children: ["livemarkAAAA"],
        parentid: "places",
      }),
      round_timestamp(modifiedForA)
    );
    collection.insert(
      "livemarkAAAA",
      encryptPayload({
        id: "livemarkAAAA",
        type: "livemark",
        feedUri: "http://example.com/a",
        parentName: "menu",
        title: "A",
        parentid: "menu",
      }),
      round_timestamp(modifiedForA)
    );

    _("Insert remotely updated livemark");
    await PlacesUtils.bookmarks.insert({
      guid: "livemarkBBBB",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      title: "B",
      lastModified: new Date(now),
      dateAdded: new Date(now),
    });
    collection.insert(
      "toolbar",
      encryptPayload({
        id: "toolbar",
        type: "folder",
        parentName: "",
        title: "toolbar",
        children: ["livemarkBBBB"],
        parentid: "places",
      }),
      round_timestamp(now)
    );
    collection.insert(
      "livemarkBBBB",
      encryptPayload({
        id: "livemarkBBBB",
        type: "livemark",
        feedUri: "http://example.com/b",
        parentName: "toolbar",
        title: "B",
        parentid: "toolbar",
      }),
      round_timestamp(now)
    );

    _("Insert new remote livemark");
    collection.insert(
      "unfiled",
      encryptPayload({
        id: "unfiled",
        type: "folder",
        parentName: "",
        title: "unfiled",
        children: ["livemarkCCCC"],
        parentid: "places",
      }),
      round_timestamp(now)
    );
    collection.insert(
      "livemarkCCCC",
      encryptPayload({
        id: "livemarkCCCC",
        type: "livemark",
        feedUri: "http://example.com/c",
        parentName: "unfiled",
        title: "C",
        parentid: "unfiled",
      }),
      round_timestamp(now)
    );

    _("Bump last sync time to ignore A");
    await engine.setLastSync(round_timestamp(now) - 60);

    _("Sync");
    await sync_engine_and_validate_telem(engine, false);

    deepEqual(
      collection.keys().sort(),
      [
        "livemarkAAAA",
        "livemarkBBBB",
        "livemarkCCCC",
        "menu",
        "mobile",
        "toolbar",
        "unfiled",
      ],
      "Should store original livemark A and tombstones for B and C on server"
    );

    let payloads = collection.payloads();

    deepEqual(
      payloads.find(payload => payload.id == "menu").children,
      ["livemarkAAAA"],
      "Should keep A in menu"
    );
    ok(
      !payloads.find(payload => payload.id == "livemarkAAAA").deleted,
      "Should not upload tombstone for A"
    );

    deepEqual(
      payloads.find(payload => payload.id == "toolbar").children,
      [],
      "Should remove B from toolbar"
    );
    ok(
      payloads.find(payload => payload.id == "livemarkBBBB").deleted,
      "Should upload tombstone for B"
    );

    deepEqual(
      payloads.find(payload => payload.id == "unfiled").children,
      [],
      "Should remove C from unfiled"
    );
    ok(
      payloads.find(payload => payload.id == "livemarkCCCC").deleted,
      "Should replace C with tombstone"
    );

    await assertBookmarksTreeMatches(
      "",
      [
        {
          guid: PlacesUtils.bookmarks.menuGuid,
          index: 0,
          children: [
            {
              guid: "livemarkAAAA",
              index: 0,
            },
          ],
        },
        {
          guid: PlacesUtils.bookmarks.toolbarGuid,
          index: 1,
        },
        {
          guid: PlacesUtils.bookmarks.unfiledGuid,
          index: 3,
        },
        {
          guid: PlacesUtils.bookmarks.mobileGuid,
          index: 4,
        },
      ],
      "Should keep A and remove B locally"
    );
  } finally {
    await cleanup(engine, server);
  }
});
