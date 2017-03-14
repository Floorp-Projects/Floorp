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

initTestLogging("Trace");

Service.engineManager.register(BookmarksEngine);

function serverForFoo(engine) {
  // The bookmarks engine *always* tracks changes, meaning we might try
  // and sync due to the bookmarks we ourselves create! Worse, because we
  // do an engine sync only, there's no locking - so we end up with multiple
  // syncs running. Neuter that by making the threshold very large.
  Service.scheduler.syncThreshold = 10000000;
  let clientsEngine = Service.clientsEngine;
  return serverForUsers({"foo": "password"}, {
    meta: {
      global: {
        syncID: Service.syncID,
        storageVersion: STORAGE_VERSION,
        engines: {
          clients: {
            version: clientsEngine.version,
            syncID: clientsEngine.syncID,
          },
          bookmarks: {
            version: engine.version,
            syncID: engine.syncID,
          },
        },
      },
    },
    crypto: {
      keys: encryptPayload({
        id: "keys",
        // Generate a fake default key bundle to avoid resetting the client
        // before the first sync.
        default: [
          Svc.Crypto.generateRandomKey(),
          Svc.Crypto.generateRandomKey(),
        ],
      }),
    },
    bookmarks: {}
  });
}

// A stored reference to the collection won't be valid after disabling.
function getBookmarkWBO(server, guid) {
  let coll = server.user("foo").collection("bookmarks");
  if (!coll) {
    return null;
  }
  return coll.wbo(guid);
}

add_task(async function test_decline_undecline() {
  let engine = Service.engineManager.get("bookmarks");
  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    let { guid: bzGuid } = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: "https://bugzilla.mozilla.org",
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      title: "bugzilla",
    });

    ok(!getBookmarkWBO(server, bzGuid), "Shouldn't have been uploaded yet");
    Service.sync();
    ok(getBookmarkWBO(server, bzGuid), "Should be present on server");

    engine.enabled = false;
    Service.sync();
    ok(!getBookmarkWBO(server, bzGuid), "Shouldn't be present on server anymore");

    engine.enabled = true;
    Service.sync();
    ok(getBookmarkWBO(server, bzGuid), "Should be present on server again");

  } finally {
    await PlacesSyncUtils.bookmarks.reset();
    await promiseStopServer(server);
  }
});

function run_test() {
  initTestLogging("Trace");
  generateNewKeys(Service.collectionKeys);
  run_next_test();
}
