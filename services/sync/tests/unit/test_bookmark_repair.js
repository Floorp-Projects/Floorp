/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the bookmark repair requestor and responder end-to-end (ie, without
// many mocks)
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://testing-common/services/sync/utils.js");

initTestLogging("Trace");
Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace
Log.repository.getLogger("Sync.Engine.Clients").level = Log.Level.Trace
Log.repository.getLogger("Sqlite").level = Log.Level.Info; // less noisy

const bms = PlacesUtils.bookmarks;

//Service.engineManager.register(BookmarksEngine);
let clientsEngine = Service.clientsEngine;
let bookmarksEngine = Service.engineManager.get("bookmarks");

generateNewKeys(Service.collectionKeys);

function createFolder(parentId, title) {
  let id = bms.createFolder(parentId, title, 0);
  let guid = bookmarksEngine._store.GUIDForId(id);
  return { id, guid };
}

function createBookmark(parentId, url, title, index = bms.DEFAULT_INDEX) {
  let uri = Utils.makeURI(url);
  let id = bms.insertBookmark(parentId, uri, index, title)
  let guid = bookmarksEngine._store.GUIDForId(id);
  return { id, guid };
}

async function promiseValidationDone(expected) {
  // wait for a validation to complete.
  let obs = promiseOneObserver("weave:engine:validate:finish");
  let { subject: validationResult } = await obs;
  // check the results - anything non-zero is checked against |expected|
  let summary = validationResult.problems.getSummary();
  let actual = summary.filter(({name, count}) => count);
  actual.sort((a, b) => String(a.name).localeCompare(b.name));
  expected.sort((a, b) => String(a.name).localeCompare(b.name));
  deepEqual(actual, expected);
};

async function cleanup(server) {
  bookmarksEngine._store.wipe();
  clientsEngine._store.wipe();
  Svc.Prefs.resetBranch("");
  Service.recordManager.clearCache();
  await promiseStopServer(server);
}

add_task(async function test_something() {
  _("Ensure that a validation error triggers a repair request.");

  let contents = {
    meta: {
      global: {
        engines: {
          clients: {
            version: clientsEngine.version,
            syncID: clientsEngine.syncID,
          },
          bookmarks: {
            version: bookmarksEngine.version,
            syncID: bookmarksEngine.syncID,
          },
        }
      }
    },
    clients: {},
    bookmarks: {},
    crypto: {},
  };
  let server = serverForUsers({"foo": "password"}, contents);
  await SyncTestingInfrastructure(server);

  let user = server.user("foo");

  let initialID = Service.clientsEngine.localID;
  let remoteID = Utils.makeGUID();
  try {

    _("Syncing to initialize crypto etc.");
    Service.sync();

    _("Create remote client record");
    server.insertWBO("foo", "clients", new ServerWBO(remoteID, encryptPayload({
      id: remoteID,
      name: "Remote client",
      type: "desktop",
      commands: [],
      version: "54",
      protocols: ["1.5"],
    }), Date.now() / 1000));

    // Create a couple of bookmarks.
    let folderInfo = createFolder(bms.toolbarFolder, "Folder 1");
    let bookmarkInfo = createBookmark(folderInfo.id, "http://getfirefox.com/", "Get Firefox!");

    let validationPromise = promiseValidationDone([]);
    _("Syncing.");
    Service.sync();
    // should have 2 clients
    equal(clientsEngine.stats.numClients, 2)
    await validationPromise;

    // Now we will reach into the server and hard-delete the bookmark
    user.collection("bookmarks").wbo(bookmarkInfo.guid).delete();
    // And delete the bookmark, but cheat by telling places that Sync did
    // it, so we don't end up with an orphan.
    // and use SQL to hard-delete the bookmark from the store.
    await bms.remove(bookmarkInfo.guid, {
      source: bms.SOURCE_SYNC,
    });
    // sanity check we aren't going to sync this removal.
    do_check_empty(bookmarksEngine.pullNewChanges());

    // sync again - we should have a few problems...
    _("Syncing again.");
    validationPromise = promiseValidationDone([
      {"name":"missingChildren","count":1},
      {"name":"structuralDifferences","count":1},
    ]);
    Service.sync();
    await validationPromise;

    // We should have started a repair with our second client.
    equal(clientsEngine.getClientCommands(remoteID).length, 1);
    _("Syncing so the outgoing client command is sent.");
    Service.sync();
    equal(clientsEngine.getClientCommands(remoteID).length, 0);

    // so now let's take over the role of that other client!
    let remoteClientsEngine = Service.clientsEngine = new ClientEngine(Service);
    remoteClientsEngine.localID = remoteID;
    _("what could possibly go wrong?");
    Service.sync();

    // TODO - make the rest of this work!
  } finally {
    await cleanup(server);
  }
});
