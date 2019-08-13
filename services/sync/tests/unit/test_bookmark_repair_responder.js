/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://services-sync/engines/bookmarks.js");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");
const { BookmarkRepairResponder } = ChromeUtils.import(
  "resource://services-sync/bookmark_repair.js"
);

// Disable validation so that we don't try to automatically repair the server
// when we sync.
Svc.Prefs.set("engine.bookmarks.validation.enabled", false);

// stub telemetry so we can easily check the right things are recorded.
var recordedEvents = [];

function checkRecordedEvents(expected) {
  // Ignore event telemetry from the merger and Places maintenance.
  let repairEvents = recordedEvents.filter(
    event => !["mirror", "maintenance"].includes(event.object)
  );
  deepEqual(repairEvents, expected);
  // and clear the list so future checks are easier to write.
  recordedEvents = [];
}

function getServerBookmarks(server) {
  return server.user("foo").collection("bookmarks");
}

async function makeServer() {
  let server = await serverForFoo(bookmarksEngine);
  await SyncTestingInfrastructure(server);
  return server;
}

async function cleanup(server) {
  await promiseStopServer(server);
  await PlacesSyncUtils.bookmarks.wipe();
  // clear keys so when each test finds a different server it accepts its keys.
  Service.collectionKeys.clear();
}

let bookmarksEngine;

add_task(async function setup() {
  bookmarksEngine = Service.engineManager.get("bookmarks");

  Service.recordTelemetryEvent = (object, method, value, extra = undefined) => {
    recordedEvents.push({ object, method, value, extra });
  };
});

add_task(async function test_responder_error() {
  let server = await makeServer();

  // sync so the collection is created.
  await Service.sync();

  let request = {
    request: "upload",
    ids: [Utils.makeGUID()],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  // mock the responder to simulate an error.
  responder._fetchItemsToUpload = async function() {
    throw new Error("oh no!");
  };
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "failed",
      value: undefined,
      extra: {
        flowID: request.flowID,
        numIDs: "0",
        failureReason: '{"name":"unexpectederror","error":"Error: oh no!"}',
      },
    },
  ]);

  await cleanup(server);
});

add_task(async function test_responder_no_items() {
  let server = await makeServer();

  // sync so the collection is created.
  await Service.sync();

  let request = {
    request: "upload",
    ids: [Utils.makeGUID()],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "0" },
    },
  ]);

  await cleanup(server);
});

// One item requested and we have it locally, but it's not yet on the server.
add_task(async function test_responder_upload() {
  let server = await makeServer();

  await Service.sync();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    ["menu", "mobile", "toolbar", "unfiled"],
    "Should only upload roots on first sync"
  );

  // Pretend we've already synced this bookmark, so that we can ensure it's
  // uploaded in response to our repair request.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Firefox",
    url: "http://getfirefox.com/",
    source: PlacesUtils.bookmarks.SOURCES.SYNC,
  });

  let request = {
    request: "upload",
    ids: [bm.guid],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  await Service.sync();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    ["menu", "mobile", "toolbar", "unfiled", bm.guid].sort(),
    "Should upload requested bookmark on second sync"
  );

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  await cleanup(server);
});

// One item requested and we have it locally and it's already on the server.
// As it was explicitly requested, we should upload it.
add_task(async function test_responder_item_exists_locally() {
  let server = await makeServer();

  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Firefox",
    url: "http://getfirefox.com/",
  });
  // first sync to get the item on the server.
  _("Syncing to get item on the server");
  await Service.sync();

  // issue a repair request for it.
  let request = {
    request: "upload",
    ids: [bm.guid],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  // We still re-upload the item.
  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  _("Syncing to do the upload.");
  await Service.sync();

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);
  await cleanup(server);
});

add_task(async function test_responder_tombstone() {
  let server = await makeServer();

  // TODO: Request an item for which we have a tombstone locally. Decide if
  // we want to store tombstones permanently for this. In the integration
  // test, we can also try requesting a deleted child or ancestor.

  // For now, we'll handle this identically to `test_responder_missing_items`.
  // Bug 1343103 is a follow-up to better handle this.
  await cleanup(server);
});

add_task(async function test_responder_missing_items() {
  let server = await makeServer();

  let fxBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Firefox",
    url: "http://getfirefox.com/",
  });

  await Service.sync();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    ["menu", "mobile", "toolbar", "unfiled", fxBmk.guid].sort(),
    "Should upload roots and Firefox on first sync"
  );

  let tbBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Thunderbird",
    url: "http://getthunderbird.com/",
    // Pretend we've already synced Thunderbird.
    source: PlacesUtils.bookmarks.SOURCES.SYNC,
  });

  _("Request Firefox, Thunderbird, and nonexistent GUID");
  let request = {
    request: "upload",
    ids: [fxBmk.guid, tbBmk.guid, Utils.makeGUID()],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "2" },
    },
  ]);

  _("Sync after requesting IDs");
  await Service.sync();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    ["menu", "mobile", "toolbar", "unfiled", fxBmk.guid, tbBmk.guid].sort(),
    "Second sync should upload Thunderbird; skip nonexistent"
  );

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "2" },
    },
  ]);

  await cleanup(server);
});

add_task(async function test_folder_descendants() {
  let server = await makeServer();

  let parentFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "Parent folder",
  });
  let childFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: parentFolder.guid,
    title: "Child folder",
  });
  // This item is in parentFolder and *should not* be uploaded as part of
  // the repair even though we explicitly request its parent.
  let existingChildBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: parentFolder.guid,
    title: "Get Firefox",
    url: "http://firefox.com",
  });
  // This item is in parentFolder and *should* be uploaded as part of
  // the repair because we explicitly request its ID.
  let childSiblingBmk = await PlacesUtils.bookmarks.insert({
    parentGuid: parentFolder.guid,
    title: "Get Thunderbird",
    url: "http://getthunderbird.com",
  });

  _("Initial sync to upload roots and parent folder");
  await Service.sync();

  let initialRecordIds = [
    "menu",
    "mobile",
    "toolbar",
    "unfiled",
    parentFolder.guid,
    existingChildBmk.guid,
    childFolder.guid,
    childSiblingBmk.guid,
  ].sort();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    initialRecordIds,
    "Should upload roots and partial folder contents on first sync"
  );

  _("Insert missing bookmarks locally to request later");
  let childBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: Utils.makeGUID(),
    parentRecordId: parentFolder.guid,
    title: "Get Firefox",
    url: "http://getfirefox.com",
  });
  let grandChildBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: Utils.makeGUID(),
    parentRecordId: childFolder.guid,
    title: "Bugzilla",
    url: "https://bugzilla.mozilla.org",
  });
  let grandChildSiblingBmk = await PlacesSyncUtils.bookmarks.insert({
    kind: "bookmark",
    recordId: Utils.makeGUID(),
    parentRecordId: childFolder.guid,
    title: "Mozilla",
    url: "https://mozilla.org",
  });

  _("Sync again");
  await Service.sync();
  {
    // The buffered engine will upload the missing records, since it does a full
    // tree merge. The old engine won't, since it relies on the Sync status
    // flag, and we used `PSU.b.i` to pretend that Sync added the bookmarks.
    let collection = getServerBookmarks(server);
    collection.remove(childBmk.recordId);
    collection.remove(grandChildBmk.recordId);
    collection.remove(grandChildSiblingBmk.recordId);
    deepEqual(
      collection.keys().sort(),
      initialRecordIds,
      "Second sync should not upload missing bookmarks"
    );
  }

  // This assumes the parent record on the server is correct, and the server
  // is just missing the children. This isn't a correct assumption if the
  // parent's `children` array is wrong, or if the parent and children disagree.
  _("Request missing bookmarks");
  let request = {
    request: "upload",
    ids: [
      // Already on server (but still uploaded as they are explicitly requested)
      parentFolder.guid,
      childSiblingBmk.guid,
      // Explicitly upload these. We should also upload `grandChildBmk`,
      // since it's a descendant of `parentFolder` and we requested its
      // ancestor.
      childBmk.recordId,
      grandChildSiblingBmk.recordId,
    ],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "5" },
    },
  ]);

  _("Sync after requesting repair; should upload missing records");
  await Service.sync();
  deepEqual(
    getServerBookmarks(server)
      .keys()
      .sort(),
    [
      ...initialRecordIds,
      childBmk.recordId,
      grandChildBmk.recordId,
      grandChildSiblingBmk.recordId,
    ].sort(),
    "Third sync should upload requested items"
  );

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "5" },
    },
  ]);

  await cleanup(server);
});

// Error handling.
add_task(async function test_aborts_unknown_request() {
  let server = await makeServer();

  let request = {
    request: "not-upload",
    ids: [],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "aborted",
      value: undefined,
      extra: {
        flowID: request.flowID,
        reason: "Don't understand request type 'not-upload'",
      },
    },
  ]);
  await cleanup(server);
});

add_task(async function test_upload_fail() {
  let server = await makeServer();

  // Pretend we've already synced this bookmark, so that we can ensure it's
  // uploaded in response to our repair request.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Firefox",
    url: "http://getfirefox.com/",
    source: PlacesUtils.bookmarks.SOURCES.SYNC,
  });

  await Service.sync();
  let request = {
    request: "upload",
    ids: [bm.guid],
    flowID: Utils.makeGUID(),
  };
  let responder = new BookmarkRepairResponder();
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  // This sync would normally upload the item - arrange for it to fail.
  let engine = Service.engineManager.get("bookmarks");
  let oldCreateRecord = engine._createRecord;
  engine._createRecord = async function(id) {
    return "anything"; // doesn't have an "encrypt"
  };

  let numFailures = 0;
  let numSuccesses = 0;
  function onUploaded(subject, data) {
    if (data != "bookmarks") {
      return;
    }
    if (subject.failed) {
      numFailures += 1;
    } else {
      numSuccesses += 1;
    }
  }
  Svc.Obs.add("weave:engine:sync:uploaded", onUploaded, this);

  await Service.sync();

  equal(numFailures, 1);
  equal(numSuccesses, 0);

  // should be no recorded events
  checkRecordedEvents([]);

  // restore the error injection so next sync succeeds - the repair should
  // restart
  engine._createRecord = oldCreateRecord;
  await responder.repair(request, null);

  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "uploading",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  await Service.sync();
  checkRecordedEvents([
    {
      object: "repairResponse",
      method: "finished",
      value: undefined,
      extra: { flowID: request.flowID, numIDs: "1" },
    },
  ]);

  equal(numFailures, 1);
  equal(numSuccesses, 1);

  Svc.Obs.remove("weave:engine:sync:uploaded", onUploaded, this);
  await cleanup(server);
});

add_task(async function teardown() {
  Svc.Prefs.reset("engine.bookmarks.validation.enabled");
});
