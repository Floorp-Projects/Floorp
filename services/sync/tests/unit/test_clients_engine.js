/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const MORE_THAN_CLIENTS_TTL_REFRESH = 691200; // 8 days
const LESS_THAN_CLIENTS_TTL_REFRESH = 86400;  // 1 day

let engine;

/**
 * Unpack the record with this ID, and verify that it has the same version that
 * we should be putting into records.
 */
async function check_record_version(user, id) {
    let payload = JSON.parse(user.collection("clients").wbo(id).payload);

    let rec = new CryptoWrapper();
    rec.id = id;
    rec.collection = "clients";
    rec.ciphertext = payload.ciphertext;
    rec.hmac = payload.hmac;
    rec.IV = payload.IV;

    let cleartext = await rec.decrypt(Service.collectionKeys.keyForCollection("clients"));

    _("Payload is " + JSON.stringify(cleartext));
    equal(Services.appinfo.version, cleartext.version);
    equal(1, cleartext.protocols.length);
    equal("1.5", cleartext.protocols[0]);
}

// compare 2 different command arrays, taking into account that a flowID
// attribute must exist, be unique in the commands, but isn't specified in
// "expected" as the value isn't known.
function compareCommands(actual, expected, description) {
  let tweakedActual = JSON.parse(JSON.stringify(actual));
  tweakedActual.map(elt => delete elt.flowID);
  deepEqual(tweakedActual, expected, description);
  // each item must have a unique flowID.
  let allIDs = new Set(actual.map(elt => elt.flowID).filter(fid => !!fid));
  equal(allIDs.size, actual.length, "all items have unique IDs");
}

async function syncClientsEngine(server) {
  engine.lastModified = server.getCollection("foo", "clients").timestamp;
  await engine._sync();
}

add_task(async function setup() {
  engine = Service.clientsEngine;
});

async function cleanup() {
  Svc.Prefs.resetBranch("");
  engine._tracker.clearChangedIDs();
  await engine._resetClient();
  // We don't finalize storage at cleanup, since we use the same clients engine
  // instance across all tests.
}

add_task(async function test_bad_hmac() {
  _("Ensure that Clients engine deletes corrupt records.");
  let deletedCollections = [];
  let deletedItems       = [];
  let callback = {
    __proto__: SyncServerCallback,
    onItemDeleted(username, coll, wboID) {
      deletedItems.push(coll + "/" + wboID);
    },
    onCollectionDeleted(username, coll) {
      deletedCollections.push(coll);
    }
  };
  let server = await serverForFoo(engine, callback);
  let user   = server.user("foo");

  function check_clients_count(expectedCount) {
    let stack = Components.stack.caller;
    let coll  = user.collection("clients");

    // Treat a non-existent collection as empty.
    equal(expectedCount, coll ? coll.count() : 0, stack);
  }

  function check_client_deleted(id) {
    let coll = user.collection("clients");
    let wbo  = coll.wbo(id);
    return !wbo || !wbo.payload;
  }

  async function uploadNewKeys() {
    await generateNewKeys(Service.collectionKeys);
    let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
    await serverKeys.encrypt(Service.identity.syncKeyBundle);
    ok((await serverKeys.upload(Service.resource(Service.cryptoKeysURL))).success);
  }

  try {
    await configureIdentity({username: "foo"}, server);
    await Service.login();

    await generateNewKeys(Service.collectionKeys);

    _("First sync, client record is uploaded");
    equal(engine.lastRecordUpload, 0);
    check_clients_count(0);
    await syncClientsEngine(server);
    check_clients_count(1);
    ok(engine.lastRecordUpload > 0);

    // Our uploaded record has a version.
    await check_record_version(user, engine.localID);

    // Initial setup can wipe the server, so clean up.
    deletedCollections = [];
    deletedItems       = [];

    _("Change our keys and our client ID, reupload keys.");
    let oldLocalID  = engine.localID;     // Preserve to test for deletion!
    engine.localID = Utils.makeGUID();
    await engine.resetClient();
    await generateNewKeys(Service.collectionKeys);
    let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
    await serverKeys.encrypt(Service.identity.syncKeyBundle);
    ok((await serverKeys.upload(Service.resource(Service.cryptoKeysURL))).success);

    _("Sync.");
    await syncClientsEngine(server);

    _("Old record " + oldLocalID + " was deleted, new one uploaded.");
    check_clients_count(1);
    check_client_deleted(oldLocalID);

    _("Now change our keys but don't upload them. " +
      "That means we get an HMAC error but redownload keys.");
    Service.lastHMACEvent = 0;
    engine.localID = Utils.makeGUID();
    await engine.resetClient();
    await generateNewKeys(Service.collectionKeys);
    deletedCollections = [];
    deletedItems       = [];
    check_clients_count(1);
    await syncClientsEngine(server);

    _("Old record was not deleted, new one uploaded.");
    equal(deletedCollections.length, 0);
    equal(deletedItems.length, 0);
    check_clients_count(2);

    _("Now try the scenario where our keys are wrong *and* there's a bad record.");
    // Clean up and start fresh.
    user.collection("clients")._wbos = {};
    Service.lastHMACEvent = 0;
    engine.localID = Utils.makeGUID();
    await engine.resetClient();
    deletedCollections = [];
    deletedItems       = [];
    check_clients_count(0);

    await uploadNewKeys();

    // Sync once to upload a record.
    await syncClientsEngine(server);
    check_clients_count(1);

    // Generate and upload new keys, so the old client record is wrong.
    await uploadNewKeys();

    // Create a new client record and new keys. Now our keys are wrong, as well
    // as the object on the server. We'll download the new keys and also delete
    // the bad client record.
    oldLocalID  = engine.localID;         // Preserve to test for deletion!
    engine.localID = Utils.makeGUID();
    await engine.resetClient();
    await generateNewKeys(Service.collectionKeys);
    let oldKey = Service.collectionKeys.keyForCollection();

    equal(deletedCollections.length, 0);
    equal(deletedItems.length, 0);
    await syncClientsEngine(server);
    equal(deletedItems.length, 1);
    check_client_deleted(oldLocalID);
    check_clients_count(1);
    let newKey = Service.collectionKeys.keyForCollection();
    ok(!oldKey.equals(newKey));

  } finally {
    await cleanup();
    await promiseStopServer(server);
  }
});

add_task(async function test_properties() {
  _("Test lastRecordUpload property");
  try {
    equal(Svc.Prefs.get("clients.lastRecordUpload"), undefined);
    equal(engine.lastRecordUpload, 0);

    let now = Date.now();
    engine.lastRecordUpload = now / 1000;
    equal(engine.lastRecordUpload, Math.floor(now / 1000));
  } finally {
    await cleanup();
  }
});

add_task(async function test_full_sync() {
  _("Ensure that Clients engine fetches all records for each sync.");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);
  let user   = server.user("foo");

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let activeID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(activeID, encryptPayload({
    id: activeID,
    name: "Active client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  let deletedID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(deletedID, encryptPayload({
    id: deletedID,
    name: "Client to delete",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    let store = engine._store;

    _("First sync. 2 records downloaded; our record uploaded.");
    strictEqual(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);
    ok(engine.lastRecordUpload > 0);
    deepEqual(user.collection("clients").keys().sort(),
              [activeID, deletedID, engine.localID].sort(),
              "Our record should be uploaded on first sync");
    let ids = await store.getAllIDs();
    deepEqual(Object.keys(ids).sort(),
              [activeID, deletedID, engine.localID].sort(),
              "Other clients should be downloaded on first sync");

    _("Delete a record, then sync again");
    let collection = server.getCollection("foo", "clients");
    collection.remove(deletedID);
    // Simulate a timestamp update in info/collections.
    await syncClientsEngine(server);

    _("Record should be updated");
    ids = await store.getAllIDs();
    deepEqual(Object.keys(ids).sort(),
              [activeID, engine.localID].sort(),
              "Deleted client should be removed on next sync");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_sync() {
  _("Ensure that Clients engine uploads a new client record once a week.");

  let server = await serverForFoo(engine);
  let user   = server.user("foo");

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  function clientWBO() {
    return user.collection("clients").wbo(engine.localID);
  }

  try {

    _("First sync. Client record is uploaded.");
    equal(clientWBO(), undefined);
    equal(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);
    ok(!!clientWBO().payload);
    ok(engine.lastRecordUpload > 0);

    _("Let's time travel more than a week back, new record should've been uploaded.");
    engine.lastRecordUpload -= MORE_THAN_CLIENTS_TTL_REFRESH;
    let lastweek = engine.lastRecordUpload;
    clientWBO().payload = undefined;
    await syncClientsEngine(server);
    ok(!!clientWBO().payload);
    ok(engine.lastRecordUpload > lastweek);

    _("Remove client record.");
    await engine.removeClientData();
    equal(clientWBO().payload, undefined);

    _("Time travel one day back, no record uploaded.");
    engine.lastRecordUpload -= LESS_THAN_CLIENTS_TTL_REFRESH;
    let yesterday = engine.lastRecordUpload;
    await syncClientsEngine(server);
    equal(clientWBO().payload, undefined);
    equal(engine.lastRecordUpload, yesterday);

  } finally {
    await cleanup();
    await promiseStopServer(server);
  }
});

add_task(async function test_client_name_change() {
  _("Ensure client name change incurs a client record update.");

  let tracker = engine._tracker;

  engine.localID; // Needed to increase the tracker changedIDs count.
  let initialName = engine.localName;

  Svc.Obs.notify("weave:engine:start-tracking");
  _("initial name: " + initialName);

  // Tracker already has data, so clear it.
  tracker.clearChangedIDs();

  let initialScore = tracker.score;

  equal(Object.keys(tracker.changedIDs).length, 0);

  Svc.Prefs.set("client.name", "new name");

  _("new name: " + engine.localName);
  notEqual(initialName, engine.localName);
  equal(Object.keys(tracker.changedIDs).length, 1);
  ok(engine.localID in tracker.changedIDs);
  ok(tracker.score > initialScore);
  ok(tracker.score >= SCORE_INCREMENT_XLARGE);

  Svc.Obs.notify("weave:engine:stop-tracking");

  await cleanup();
});

add_task(async function test_last_modified() {
  _("Ensure that remote records have a sane serverLastModified attribute.");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);
  let user   = server.user("foo");

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let activeID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(activeID, encryptPayload({
    id: activeID,
    name: "Active client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    let collection = user.collection("clients");

    _("Sync to download the record");
    await syncClientsEngine(server);

    equal(engine._store._remoteClients[activeID].serverLastModified, now - 10,
          "last modified in the local record is correctly the server last-modified");

    _("Modify the record and re-upload it");
    // set a new name to make sure we really did upload.
    engine._store._remoteClients[activeID].name = "New name";
    engine._modified.set(activeID, 0);
    // The sync above also did a POST, so adjust our lastModified.
    engine.lastModified = server.getCollection("foo", "clients").timestamp;
    await engine._uploadOutgoing();

    _("Local record should have updated timestamp");
    ok(engine._store._remoteClients[activeID].serverLastModified >= now);

    _("Record on the server should have new name but not serverLastModified");
    let payload = JSON.parse(JSON.parse(collection.payload(activeID)).ciphertext);
    equal(payload.name, "New name");
    equal(payload.serverLastModified, undefined);

  } finally {
    await cleanup();
    server.deleteCollections("foo");
    await promiseStopServer(server);
  }
});

add_task(async function test_send_command() {
  _("Verifies _sendCommandToClient puts commands in the outbound queue.");

  let store = engine._store;
  let tracker = engine._tracker;
  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);

  await store.create(rec);
  await store.createRecord(remoteId, "clients");

  let action = "testCommand";
  let args = ["foo", "bar"];
  let extra = { flowID: "flowy" };

  await engine._sendCommandToClient(action, args, remoteId, extra);

  let newRecord = store._remoteClients[remoteId];
  let clientCommands = (await engine._readCommands())[remoteId];
  notEqual(newRecord, undefined);
  equal(clientCommands.length, 1);

  let command = clientCommands[0];
  equal(command.command, action);
  equal(command.args.length, 2);
  deepEqual(command.args, args);
  ok(command.flowID);

  notEqual(tracker.changedIDs[remoteId], undefined);

  await cleanup();
});

// The browser UI might call _addClientCommand indirectly without awaiting on the returned promise.
// We need to make sure this doesn't result on commands not being saved.
add_task(async function test_add_client_command_race() {
  let promises = [];
  for (let i = 0; i < 100; i++) {
    promises.push(engine._addClientCommand(`client-${i}`, { command: "cmd", args: []}));
  }
  await Promise.all(promises);

  let localCommands = await engine._readCommands();
  for (let i = 0; i < 100; i++) {
    equal(localCommands[`client-${i}`].length, 1);
  }
});

add_task(async function test_command_validation() {
  _("Verifies that command validation works properly.");

  let store = engine._store;

  let testCommands = [
    ["resetAll",    [],       true ],
    ["resetAll",    ["foo"],  false],
    ["resetEngine", ["tabs"], true ],
    ["resetEngine", [],       false],
    ["wipeAll",     [],       true ],
    ["wipeAll",     ["foo"],  false],
    ["wipeEngine",  ["tabs"], true ],
    ["wipeEngine",  [],       false],
    ["logout",      [],       true ],
    ["logout",      ["foo"],  false],
    ["__UNKNOWN__", [],       false]
  ];

  for (let [action, args, expectedResult] of testCommands) {
    let remoteId = Utils.makeGUID();
    let rec = new ClientsRec("clients", remoteId);

    await store.create(rec);
    await store.createRecord(remoteId, "clients");

    await engine.sendCommand(action, args, remoteId);

    let newRecord = store._remoteClients[remoteId];
    notEqual(newRecord, undefined);

    let clientCommands = (await engine._readCommands())[remoteId];

    if (expectedResult) {
      _("Ensuring command is sent: " + action);
      equal(clientCommands.length, 1);

      let command = clientCommands[0];
      equal(command.command, action);
      deepEqual(command.args, args);

      notEqual(engine._tracker, undefined);
      notEqual(engine._tracker.changedIDs[remoteId], undefined);
    } else {
      _("Ensuring command is scrubbed: " + action);
      equal(clientCommands, undefined);

      if (store._tracker) {
        equal(engine._tracker[remoteId], undefined);
      }
    }

  }
  await cleanup();
});

add_task(async function test_command_duplication() {
  _("Ensures duplicate commands are detected and not added");

  let store = engine._store;
  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);
  await store.create(rec);
  await store.createRecord(remoteId, "clients");

  let action = "resetAll";
  let args = [];

  await engine.sendCommand(action, args, remoteId);
  await engine.sendCommand(action, args, remoteId);

  let clientCommands = (await engine._readCommands())[remoteId];
  equal(clientCommands.length, 1);

  _("Check variant args length");
  await engine._saveCommands({});

  action = "resetEngine";
  await engine.sendCommand(action, [{ x: "foo" }], remoteId);
  await engine.sendCommand(action, [{ x: "bar" }], remoteId);

  _("Make sure we spot a real dupe argument.");
  await engine.sendCommand(action, [{ x: "bar" }], remoteId);

  clientCommands = (await engine._readCommands())[remoteId];
  equal(clientCommands.length, 2);

  await cleanup();
});

add_task(async function test_command_invalid_client() {
  _("Ensures invalid client IDs are caught");

  let id = Utils.makeGUID();
  let error;

  try {
    await engine.sendCommand("wipeAll", [], id);
  } catch (ex) {
    error = ex;
  }

  equal(error.message.indexOf("Unknown remote client ID: "), 0);

  await cleanup();
});

add_task(async function test_process_incoming_commands() {
  _("Ensures local commands are executed");

  engine.localCommands = [{ command: "logout", args: [] }];

  let ev = "weave:service:logout:finish";

  let logoutPromise = new Promise(resolve => {
    var handler = function() {
      Svc.Obs.remove(ev, handler);

      resolve();
    };

    Svc.Obs.add(ev, handler);
  });

  // logout command causes processIncomingCommands to return explicit false.
  ok(!(await engine.processIncomingCommands()));

  await logoutPromise;

  await cleanup();
});

add_task(async function test_filter_duplicate_names() {
  _("Ensure that we exclude clients with identical names that haven't synced in a week.");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);
  let user   = server.user("foo");

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  // Synced recently.
  let recentID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(recentID, encryptPayload({
    id: recentID,
    name: "My Phone",
    type: "mobile",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  // Dupe of our client, synced more than 1 week ago.
  let dupeID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(dupeID, encryptPayload({
    id: dupeID,
    name: engine.localName,
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 604810));

  // Synced more than 1 week ago, but not a dupe.
  let oldID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(oldID, encryptPayload({
    id: oldID,
    name: "My old desktop",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 604820));

  try {
    let store = engine._store;

    _("First sync");
    strictEqual(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);
    ok(engine.lastRecordUpload > 0);
    deepEqual(user.collection("clients").keys().sort(),
              [recentID, dupeID, oldID, engine.localID].sort(),
              "Our record should be uploaded on first sync");

    let ids = await store.getAllIDs();
    deepEqual(Object.keys(ids).sort(),
              [recentID, dupeID, oldID, engine.localID].sort(),
              "Duplicate ID should remain in getAllIDs");
    ok((await engine._store.itemExists(dupeID)), "Dupe ID should be considered as existing for Sync methods.");
    ok(!engine.remoteClientExists(dupeID), "Dupe ID should not be considered as existing for external methods.");

    // dupe desktop should not appear in .deviceTypes.
    equal(engine.deviceTypes.get("desktop"), 2);
    equal(engine.deviceTypes.get("mobile"), 1);

    // dupe desktop should not appear in stats
    deepEqual(engine.stats, {
      hasMobile: 1,
      names: [engine.localName, "My Phone", "My old desktop"],
      numClients: 3,
    });

    ok(engine.remoteClientExists(oldID), "non-dupe ID should exist.");
    ok(!engine.remoteClientExists(dupeID), "dupe ID should not exist");
    equal(engine.remoteClients.length, 2, "dupe should not be in remoteClients");

    // Check that a subsequent Sync doesn't report anything as being processed.
    let counts;
    Svc.Obs.add("weave:engine:sync:applied", function observe(subject, data) {
      Svc.Obs.remove("weave:engine:sync:applied", observe);
      counts = subject;
    });

    await syncClientsEngine(server);
    equal(counts.applied, 0); // We didn't report applying any records.
    equal(counts.reconciled, 4); // We reported reconcilliation for all records
    equal(counts.succeeded, 0);
    equal(counts.failed, 0);
    equal(counts.newFailed, 0);

    _("Broadcast logout to all clients");
    await engine.sendCommand("logout", []);
    await syncClientsEngine(server);

    let collection = server.getCollection("foo", "clients");
    let recentPayload = JSON.parse(JSON.parse(collection.payload(recentID)).ciphertext);
    compareCommands(recentPayload.commands, [{ command: "logout", args: [] }],
                    "Should send commands to the recent client");

    let oldPayload = JSON.parse(JSON.parse(collection.payload(oldID)).ciphertext);
    compareCommands(oldPayload.commands, [{ command: "logout", args: [] }],
                    "Should send commands to the week-old client");

    let dupePayload = JSON.parse(JSON.parse(collection.payload(dupeID)).ciphertext);
    deepEqual(dupePayload.commands, [],
              "Should not send commands to the dupe client");

    _("Update the dupe client's modified time");
    server.insertWBO("foo", "clients", new ServerWBO(dupeID, encryptPayload({
      id: dupeID,
      name: engine.localName,
      type: "desktop",
      commands: [],
      version: "48",
      protocols: ["1.5"],
    }), now - 10));

    _("Second sync.");
    await syncClientsEngine(server);

    ids = await store.getAllIDs();
    deepEqual(Object.keys(ids).sort(),
              [recentID, oldID, dupeID, engine.localID].sort(),
              "Stale client synced, so it should no longer be marked as a dupe");

    ok(engine.remoteClientExists(dupeID), "Dupe ID should appear as it synced.");

    // Recently synced dupe desktop should appear in .deviceTypes.
    equal(engine.deviceTypes.get("desktop"), 3);

    // Recently synced dupe desktop should now appear in stats
    deepEqual(engine.stats, {
      hasMobile: 1,
      names: [engine.localName, "My Phone", engine.localName, "My old desktop"],
      numClients: 4,
    });

    ok(engine.remoteClientExists(dupeID), "recently synced dupe ID should now exist");
    equal(engine.remoteClients.length, 3, "recently synced dupe should now be in remoteClients");

  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_command_sync() {
  _("Ensure that commands are synced across clients.");

  await engine._store.wipe();
  await generateNewKeys(Service.collectionKeys);

  let server   = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let user     = server.user("foo");
  let remoteId = Utils.makeGUID();

  function clientWBO(id) {
    return user.collection("clients").wbo(id);
  }

  _("Create remote client record");
  server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
    id: remoteId,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), Date.now() / 1000));

  try {
    _("Syncing.");
    await syncClientsEngine(server);

    _("Checking remote record was downloaded.");
    let clientRecord = engine._store._remoteClients[remoteId];
    notEqual(clientRecord, undefined);
    equal(clientRecord.commands.length, 0);

    _("Send a command to the remote client.");
    await engine.sendCommand("wipeAll", []);
    let clientCommands = (await engine._readCommands())[remoteId];
    equal(clientCommands.length, 1);
    await syncClientsEngine(server);

    _("Checking record was uploaded.");
    notEqual(clientWBO(engine.localID).payload, undefined);
    ok(engine.lastRecordUpload > 0);

    notEqual(clientWBO(remoteId).payload, undefined);

    Svc.Prefs.set("client.GUID", remoteId);
    engine._resetClient();
    equal(engine.localID, remoteId);
    _("Performing sync on resetted client.");
    await syncClientsEngine(server);
    notEqual(engine.localCommands, undefined);
    equal(engine.localCommands.length, 1);

    let command = engine.localCommands[0];
    equal(command.command, "wipeAll");
    equal(command.args.length, 0);

  } finally {
    await cleanup();

    try {
      let collection = server.getCollection("foo", "clients");
      collection.remove(remoteId);
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_clients_not_in_fxa_list() {
  _("Ensure that clients not in the FxA devices list are marked as stale.");

  await engine._store.wipe();
  await generateNewKeys(Service.collectionKeys);

  let server   = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let remoteId = Utils.makeGUID();
  let remoteId2 = Utils.makeGUID();

  _("Create remote client records");
  server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
    id: remoteId,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteId,
    protocols: ["1.5"],
  }), Date.now() / 1000));
  server.insertWBO("foo", "clients", new ServerWBO(remoteId2, encryptPayload({
    id: remoteId2,
    name: "Remote client 2",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteId2,
    protocols: ["1.5"],
  }), Date.now() / 1000));

  let fxAccounts = engine.fxAccounts;
  engine.fxAccounts = {
    notifyDevices() { return Promise.resolve(true); },
    getDeviceId() { return fxAccounts.getDeviceId(); },
    getDeviceList() { return Promise.resolve([{ id: remoteId }]); }
  };

  try {
    _("Syncing.");
    await syncClientsEngine(server);

    ok(!engine._store._remoteClients[remoteId].stale);
    ok(engine._store._remoteClients[remoteId2].stale);

  } finally {
    engine.fxAccounts = fxAccounts;
    await cleanup();

    try {
      let collection = server.getCollection("foo", "clients");
      collection.remove(remoteId);
    } finally {
      await promiseStopServer(server);
    }
  }
});


add_task(async function test_dupe_device_ids() {
  _("Ensure that we mark devices with duplicate fxaDeviceIds but older lastModified as stale.");

  await engine._store.wipe();
  await generateNewKeys(Service.collectionKeys);

  let server   = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let remoteId = Utils.makeGUID();
  let remoteId2 = Utils.makeGUID();
  let remoteDeviceId = Utils.makeGUID();

  _("Create remote client records");
  server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
    id: remoteId,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteDeviceId,
    protocols: ["1.5"],
  }), Date.now() / 1000 - 30000));
  server.insertWBO("foo", "clients", new ServerWBO(remoteId2, encryptPayload({
    id: remoteId2,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteDeviceId,
    protocols: ["1.5"],
  }), Date.now() / 1000));

  let fxAccounts = engine.fxAccounts;
  engine.fxAccounts = {
    notifyDevices() { return Promise.resolve(true); },
    getDeviceId() { return fxAccounts.getDeviceId(); },
    getDeviceList() { return Promise.resolve([{ id: remoteDeviceId }]); }
  };

  try {
    _("Syncing.");
    await syncClientsEngine(server);

    ok(engine._store._remoteClients[remoteId].stale);
    ok(!engine._store._remoteClients[remoteId2].stale);

  } finally {
    engine.fxAccounts = fxAccounts;
    await cleanup();

    try {
      let collection = server.getCollection("foo", "clients");
      collection.remove(remoteId);
    } finally {
      await promiseStopServer(server);
    }
  }
});


add_task(async function test_send_uri_to_client_for_display() {
  _("Ensure sendURIToClientForDisplay() sends command properly.");

  let tracker = engine._tracker;
  let store = engine._store;

  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);
  rec.name = "remote";
  await store.create(rec);
  await store.createRecord(remoteId, "clients");

  tracker.clearChangedIDs();
  let initialScore = tracker.score;

  let uri = "http://www.mozilla.org/";
  let title = "Title of the Page";
  await engine.sendURIToClientForDisplay(uri, remoteId, title);

  let newRecord = store._remoteClients[remoteId];

  notEqual(newRecord, undefined);
  let clientCommands = (await engine._readCommands())[remoteId];
  equal(clientCommands.length, 1);

  let command = clientCommands[0];
  equal(command.command, "displayURI");
  equal(command.args.length, 3);
  equal(command.args[0], uri);
  equal(command.args[1], engine.localID);
  equal(command.args[2], title);

  ok(tracker.score > initialScore);
  ok(tracker.score - initialScore >= SCORE_INCREMENT_XLARGE);

  _("Ensure unknown client IDs result in exception.");
  let unknownId = Utils.makeGUID();
  let error;

  try {
    await engine.sendURIToClientForDisplay(uri, unknownId);
  } catch (ex) {
    error = ex;
  }

  equal(error.message.indexOf("Unknown remote client ID: "), 0);

  await cleanup();
});

add_task(async function test_receive_display_uri() {
  _("Ensure processing of received 'displayURI' commands works.");

  // We don't set up WBOs and perform syncing because other tests verify
  // the command API works as advertised. This saves us a little work.

  let uri = "http://www.mozilla.org/";
  let remoteId = Utils.makeGUID();
  let title = "Page Title!";

  let command = {
    command: "displayURI",
    args: [uri, remoteId, title],
  };

  engine.localCommands = [command];

  // Received 'displayURI' command should result in the topic defined below
  // being called.
  let ev = "weave:engine:clients:display-uris";

  let promiseDisplayURI = new Promise(resolve => {
    let handler = function(subject, data) {
      Svc.Obs.remove(ev, handler);

      resolve({ subject, data });
    };

    Svc.Obs.add(ev, handler);
  });

  ok((await engine.processIncomingCommands()));

  let { subject, data } = await promiseDisplayURI;

  equal(subject[0].uri, uri);
  equal(subject[0].clientId, remoteId);
  equal(subject[0].title, title);
  equal(data, null);

  await cleanup();
});

add_task(async function test_optional_client_fields() {
  _("Ensure that we produce records with the fields added in Bug 1097222.");

  const SUPPORTED_PROTOCOL_VERSIONS = ["1.5"];
  let local = await engine._store.createRecord(engine.localID, "clients");
  equal(local.name, engine.localName);
  equal(local.type, engine.localType);
  equal(local.version, Services.appinfo.version);
  deepEqual(local.protocols, SUPPORTED_PROTOCOL_VERSIONS);

  // Optional fields.
  // Make sure they're what they ought to be...
  equal(local.os, Services.appinfo.OS);
  equal(local.appPackage, Services.appinfo.ID);

  // ... and also that they're non-empty.
  ok(!!local.os);
  ok(!!local.appPackage);
  ok(!!local.application);

  // We don't currently populate device or formfactor.
  // See Bug 1100722, Bug 1100723.

  await cleanup();
});

add_task(async function test_merge_commands() {
  _("Verifies local commands for remote clients are merged with the server's");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let desktopID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(desktopID, encryptPayload({
    id: desktopID,
    name: "Desktop client",
    type: "desktop",
    commands: [{
      command: "displayURI",
      args: ["https://example.com", engine.localID, "Yak Herders Anonymous"],
      flowID: Utils.makeGUID(),
    }],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  let mobileID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(mobileID, encryptPayload({
    id: mobileID,
    name: "Mobile client",
    type: "mobile",
    commands: [{
      command: "logout",
      args: [],
      flowID: Utils.makeGUID(),
    }],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    _("First sync. 2 records downloaded.");
    strictEqual(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);

    _("Broadcast logout to all clients");
    await engine.sendCommand("logout", []);
    await syncClientsEngine(server);

    let collection = server.getCollection("foo", "clients");
    let desktopPayload = JSON.parse(JSON.parse(collection.payload(desktopID)).ciphertext);
    compareCommands(desktopPayload.commands, [{
      command: "displayURI",
      args: ["https://example.com", engine.localID, "Yak Herders Anonymous"],
    }, {
      command: "logout",
      args: [],
    }], "Should send the logout command to the desktop client");

    let mobilePayload = JSON.parse(JSON.parse(collection.payload(mobileID)).ciphertext);
    compareCommands(mobilePayload.commands, [{ command: "logout", args: [] }],
                    "Should not send a duplicate logout to the mobile client");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_duplicate_remote_commands() {
  _("Verifies local commands for remote clients are sent only once (bug 1289287)");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let desktopID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(desktopID, encryptPayload({
    id: desktopID,
    name: "Desktop client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    _("First sync. 1 record downloaded.");
    strictEqual(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);

    _("Send tab to client");
    await engine.sendCommand("displayURI", ["https://example.com", engine.localID, "Yak Herders Anonymous"]);
    await syncClientsEngine(server);

    _("Simulate the desktop client consuming the command and syncing to the server");
    server.insertWBO("foo", "clients", new ServerWBO(desktopID, encryptPayload({
      id: desktopID,
      name: "Desktop client",
      type: "desktop",
      commands: [],
      version: "48",
      protocols: ["1.5"],
    }), now - 10));

    _("Send another tab to the desktop client");
    await engine.sendCommand("displayURI", ["https://foobar.com", engine.localID, "Foo bar!"], desktopID);
    await syncClientsEngine(server);

    let collection = server.getCollection("foo", "clients");
    let desktopPayload = JSON.parse(JSON.parse(collection.payload(desktopID)).ciphertext);
    compareCommands(desktopPayload.commands, [{
      command: "displayURI",
      args: ["https://foobar.com", engine.localID, "Foo bar!"],
    }], "Should only send the second command to the desktop client");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_upload_after_reboot() {
  _("Multiple downloads, reboot, then upload (bug 1289287)");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let deviceBID = Utils.makeGUID();
  let deviceCID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(deviceBID, encryptPayload({
    id: deviceBID,
    name: "Device B",
    type: "desktop",
    commands: [{
      command: "displayURI",
      args: ["https://deviceclink.com", deviceCID, "Device C link"],
      flowID: Utils.makeGUID(),
    }],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));
  server.insertWBO("foo", "clients", new ServerWBO(deviceCID, encryptPayload({
    id: deviceCID,
    name: "Device C",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    _("First sync. 2 records downloaded.");
    strictEqual(engine.lastRecordUpload, 0);
    await syncClientsEngine(server);

    _("Send tab to client");
    await engine.sendCommand("displayURI", ["https://example.com", engine.localID, "Yak Herders Anonymous"], deviceBID);

    const oldUploadOutgoing = SyncEngine.prototype._uploadOutgoing;
    SyncEngine.prototype._uploadOutgoing = async () => engine._onRecordsWritten([], [deviceBID]);
    await syncClientsEngine(server);

    let collection = server.getCollection("foo", "clients");
    let deviceBPayload = JSON.parse(JSON.parse(collection.payload(deviceBID)).ciphertext);
    compareCommands(deviceBPayload.commands, [{
      command: "displayURI", args: ["https://deviceclink.com", deviceCID, "Device C link"]
    }], "Should be the same because the upload failed");

    _("Simulate the client B consuming the command and syncing to the server");
    server.insertWBO("foo", "clients", new ServerWBO(deviceBID, encryptPayload({
      id: deviceBID,
      name: "Device B",
      type: "desktop",
      commands: [],
      version: "48",
      protocols: ["1.5"],
    }), now - 10));

    // Simulate reboot
    SyncEngine.prototype._uploadOutgoing = oldUploadOutgoing;
    engine = Service.clientsEngine = new ClientEngine(Service);
    await engine.initialize();

    await syncClientsEngine(server);

    deviceBPayload = JSON.parse(JSON.parse(collection.payload(deviceBID)).ciphertext);
    compareCommands(deviceBPayload.commands, [{
      command: "displayURI",
      args: ["https://example.com", engine.localID, "Yak Herders Anonymous"],
    }], "Should only had written our outgoing command");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_keep_cleared_commands_after_reboot() {
  _("Download commands, fail upload, reboot, then apply new commands (bug 1289287)");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let deviceBID = Utils.makeGUID();
  let deviceCID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(engine.localID, encryptPayload({
    id: engine.localID,
    name: "Device A",
    type: "desktop",
    commands: [{
      command: "displayURI",
      args: ["https://deviceblink.com", deviceBID, "Device B link"],
      flowID: Utils.makeGUID(),
    },
    {
      command: "displayURI",
      args: ["https://deviceclink.com", deviceCID, "Device C link"],
      flowID: Utils.makeGUID(),
    }],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));
  server.insertWBO("foo", "clients", new ServerWBO(deviceBID, encryptPayload({
    id: deviceBID,
    name: "Device B",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));
  server.insertWBO("foo", "clients", new ServerWBO(deviceCID, encryptPayload({
    id: deviceCID,
    name: "Device C",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    _("First sync. Download remote and our record.");
    strictEqual(engine.lastRecordUpload, 0);

    let collection = server.getCollection("foo", "clients");
    const oldUploadOutgoing = SyncEngine.prototype._uploadOutgoing;
    SyncEngine.prototype._uploadOutgoing = async () => engine._onRecordsWritten([], [deviceBID]);
    let commandsProcessed = 0;
    engine._handleDisplayURIs = (uris) => { commandsProcessed = uris.length; };

    await syncClientsEngine(server);
    await engine.processIncomingCommands(); // Not called by the engine.sync(), gotta call it ourselves
    equal(commandsProcessed, 2, "We processed 2 commands");

    let localRemoteRecord = JSON.parse(JSON.parse(collection.payload(engine.localID)).ciphertext);
    compareCommands(localRemoteRecord.commands, [{
      command: "displayURI", args: ["https://deviceblink.com", deviceBID, "Device B link"]
    },
    {
      command: "displayURI", args: ["https://deviceclink.com", deviceCID, "Device C link"]
    }], "Should be the same because the upload failed");

    // Another client sends another link
    server.insertWBO("foo", "clients", new ServerWBO(engine.localID, encryptPayload({
      id: engine.localID,
      name: "Device A",
      type: "desktop",
      commands: [{
        command: "displayURI",
        args: ["https://deviceblink.com", deviceBID, "Device B link"],
        flowID: Utils.makeGUID(),
      },
      {
        command: "displayURI",
        args: ["https://deviceclink.com", deviceCID, "Device C link"],
        flowID: Utils.makeGUID(),
      },
      {
        command: "displayURI",
        args: ["https://deviceclink2.com", deviceCID, "Device C link 2"],
        flowID: Utils.makeGUID(),
      }],
      version: "48",
      protocols: ["1.5"],
    }), now - 5));

    // Simulate reboot
    SyncEngine.prototype._uploadOutgoing = oldUploadOutgoing;
    engine = Service.clientsEngine = new ClientEngine(Service);
    await engine.initialize();

    commandsProcessed = 0;
    engine._handleDisplayURIs = (uris) => { commandsProcessed = uris.length; };
    await syncClientsEngine(server);
    await engine.processIncomingCommands();
    equal(commandsProcessed, 1, "We processed one command (the other were cleared)");

    localRemoteRecord = JSON.parse(JSON.parse(collection.payload(deviceBID)).ciphertext);
    deepEqual(localRemoteRecord.commands, [], "Should be empty");
  } finally {
    await cleanup();

    // Reset service (remove mocks)
    engine = Service.clientsEngine = new ClientEngine(Service);
    await engine.initialize();
    engine._resetClient();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_deleted_commands() {
  _("Verifies commands for a deleted client are discarded");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let activeID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(activeID, encryptPayload({
    id: activeID,
    name: "Active client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  let deletedID = Utils.makeGUID();
  server.insertWBO("foo", "clients", new ServerWBO(deletedID, encryptPayload({
    id: deletedID,
    name: "Client to delete",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"],
  }), now - 10));

  try {
    _("First sync. 2 records downloaded.");
    await syncClientsEngine(server);

    _("Delete a record on the server.");
    let collection = server.getCollection("foo", "clients");
    collection.remove(deletedID);

    _("Broadcast a command to all clients");
    await engine.sendCommand("logout", []);
    await syncClientsEngine(server);

    deepEqual(collection.keys().sort(), [activeID, engine.localID].sort(),
      "Should not reupload deleted clients");

    let activePayload = JSON.parse(JSON.parse(collection.payload(activeID)).ciphertext);
    compareCommands(activePayload.commands, [{ command: "logout", args: [] }],
      "Should send the command to the active client");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_send_uri_ack() {
  _("Ensure a sent URI is deleted when the client syncs");

  let now = Date.now() / 1000;
  let server = await serverForFoo(engine);

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  try {
    let fakeSenderID = Utils.makeGUID();

    _("Initial sync for empty clients collection");
    await syncClientsEngine(server);
    let collection = server.getCollection("foo", "clients");
    let ourPayload = JSON.parse(JSON.parse(collection.payload(engine.localID)).ciphertext);
    ok(ourPayload, "Should upload our client record");

    _("Send a URL to the device on the server");
    ourPayload.commands = [{
      command: "displayURI",
      args: ["https://example.com", fakeSenderID, "Yak Herders Anonymous"],
      flowID: Utils.makeGUID(),
    }];
    server.insertWBO("foo", "clients", new ServerWBO(engine.localID, encryptPayload(ourPayload), now));

    _("Sync again");
    await syncClientsEngine(server);
    compareCommands(engine.localCommands, [{
      command: "displayURI",
      args: ["https://example.com", fakeSenderID, "Yak Herders Anonymous"],
    }], "Should receive incoming URI");
    ok((await engine.processIncomingCommands()), "Should process incoming commands");
    const clearedCommands = (await engine._readCommands())[engine.localID];
    compareCommands(clearedCommands, [{
      command: "displayURI",
      args: ["https://example.com", fakeSenderID, "Yak Herders Anonymous"],
    }], "Should mark the commands as cleared after processing");

    _("Check that the command was removed on the server");
    await syncClientsEngine(server);
    ourPayload = JSON.parse(JSON.parse(collection.payload(engine.localID)).ciphertext);
    ok(ourPayload, "Should upload the synced client record");
    deepEqual(ourPayload.commands, [], "Should not reupload cleared commands");
  } finally {
    await cleanup();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function test_command_sync() {
  _("Notify other clients when writing their record.");

  await engine._store.wipe();
  await generateNewKeys(Service.collectionKeys);

  let server    = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let collection = server.getCollection("foo", "clients");
  let remoteId   = Utils.makeGUID();
  let remoteId2  = Utils.makeGUID();

  _("Create remote client record 1");
  server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
    id: remoteId,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    protocols: ["1.5"]
  }), Date.now() / 1000));

  _("Create remote client record 2");
  server.insertWBO("foo", "clients", new ServerWBO(remoteId2, encryptPayload({
    id: remoteId2,
    name: "Remote client 2",
    type: "mobile",
    commands: [],
    version: "48",
    protocols: ["1.5"]
  }), Date.now() / 1000));

  try {
    equal(collection.count(), 2, "2 remote records written");
    await syncClientsEngine(server);
    equal(collection.count(), 3, "3 remote records written (+1 for the synced local record)");

    await engine.sendCommand("wipeAll", []);
    engine._tracker.addChangedID(engine.localID);
    const getClientFxaDeviceId = sinon.stub(engine, "getClientFxaDeviceId", (id) => "fxa-" + id);
    const engineMock = sinon.mock(engine);
    let _notifyCollectionChanged = engineMock.expects("_notifyCollectionChanged")
                                             .withArgs(["fxa-" + remoteId, "fxa-" + remoteId2]);
    _("Syncing.");
    await syncClientsEngine(server);
    _notifyCollectionChanged.verify();

    engineMock.restore();
    getClientFxaDeviceId.restore();
  } finally {
    await cleanup();
    engine._tracker.clearChangedIDs();

    try {
      server.deleteCollections("foo");
    } finally {
      await promiseStopServer(server);
    }
  }
});

add_task(async function ensureSameFlowIDs() {
  let events = [];
  let origRecordTelemetryEvent = Service.recordTelemetryEvent;
  Service.recordTelemetryEvent = (object, method, value, extra) => {
    events.push({ object, method, value, extra });
  };

  let server = await serverForFoo(engine);
  try {
    // Setup 2 clients, send them a command, and ensure we get to events
    // written, both with the same flowID.
    await SyncTestingInfrastructure(server);

    let remoteId   = Utils.makeGUID();
    let remoteId2  = Utils.makeGUID();

    _("Create remote client record 1");
    server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
      id: remoteId,
      name: "Remote client",
      type: "desktop",
      commands: [],
      version: "48",
      protocols: ["1.5"]
    }), Date.now() / 1000));

    _("Create remote client record 2");
    server.insertWBO("foo", "clients", new ServerWBO(remoteId2, encryptPayload({
      id: remoteId2,
      name: "Remote client 2",
      type: "mobile",
      commands: [],
      version: "48",
      protocols: ["1.5"]
    }), Date.now() / 1000));

    await syncClientsEngine(server);
    await engine.sendCommand("wipeAll", []);
    await syncClientsEngine(server);
    equal(events.length, 2);
    // we don't know what the flowID is, but do know it should be the same.
    equal(events[0].extra.flowID, events[1].extra.flowID);
    // Wipe remote clients to ensure deduping doesn't prevent us from adding the command.
    for (let client of Object.values(engine._store._remoteClients)) {
      client.commands = [];
    }
    // check it's correctly used when we specify a flow ID
    events.length = 0;
    let flowID = Utils.makeGUID();
    await engine.sendCommand("wipeAll", [], null, { flowID });
    await syncClientsEngine(server);
    equal(events.length, 2);
    equal(events[0].extra.flowID, flowID);
    equal(events[1].extra.flowID, flowID);

    // Wipe remote clients to ensure deduping doesn't prevent us from adding the command.
    for (let client of Object.values(engine._store._remoteClients)) {
      client.commands = [];
    }

    // and that it works when something else is in "extra"
    events.length = 0;
    await engine.sendCommand("wipeAll", [], null, { reason: "testing" });
    await syncClientsEngine(server);
    equal(events.length, 2);
    equal(events[0].extra.flowID, events[1].extra.flowID);
    equal(events[0].extra.reason, "testing");
    equal(events[1].extra.reason, "testing");
    // Wipe remote clients to ensure deduping doesn't prevent us from adding the command.
    for (let client of Object.values(engine._store._remoteClients)) {
      client.commands = [];
    }

    // and when both are specified.
    events.length = 0;
    await engine.sendCommand("wipeAll", [], null, { reason: "testing", flowID });
    await syncClientsEngine(server);
    equal(events.length, 2);
    equal(events[0].extra.flowID, flowID);
    equal(events[1].extra.flowID, flowID);
    equal(events[0].extra.reason, "testing");
    equal(events[1].extra.reason, "testing");
    // Wipe remote clients to ensure deduping doesn't prevent us from adding the command.
    for (let client of Object.values(engine._store._remoteClients)) {
      client.commands = [];
    }

  } finally {
    Service.recordTelemetryEvent = origRecordTelemetryEvent;
    cleanup();
    await promiseStopServer(server);
  }
});

add_task(async function test_duplicate_commands_telemetry() {
  let events = [];
  let origRecordTelemetryEvent = Service.recordTelemetryEvent;
  Service.recordTelemetryEvent = (object, method, value, extra) => {
    events.push({ object, method, value, extra });
  };

  let server = await serverForFoo(engine);
  try {
    await SyncTestingInfrastructure(server);

    let remoteId   = Utils.makeGUID();
    let remoteId2  = Utils.makeGUID();

    _("Create remote client record 1");
    server.insertWBO("foo", "clients", new ServerWBO(remoteId, encryptPayload({
      id: remoteId,
      name: "Remote client",
      type: "desktop",
      commands: [],
      version: "48",
      protocols: ["1.5"]
    }), Date.now() / 1000));

    _("Create remote client record 2");
    server.insertWBO("foo", "clients", new ServerWBO(remoteId2, encryptPayload({
      id: remoteId2,
      name: "Remote client 2",
      type: "mobile",
      commands: [],
      version: "48",
      protocols: ["1.5"]
    }), Date.now() / 1000));

    await syncClientsEngine(server);
    // Make sure deduping works before syncing
    await engine.sendURIToClientForDisplay("https://example.com", remoteId, "Example");
    await engine.sendURIToClientForDisplay("https://example.com", remoteId, "Example");
    equal(events.length, 1);
    await syncClientsEngine(server);
    // And after syncing.
    await engine.sendURIToClientForDisplay("https://example.com", remoteId, "Example");
    equal(events.length, 1);
    // Ensure we aren't deduping commands to different clients
    await engine.sendURIToClientForDisplay("https://example.com", remoteId2, "Example");
    equal(events.length, 2);
  } finally {
    Service.recordTelemetryEvent = origRecordTelemetryEvent;
    cleanup();
    await promiseStopServer(server);
  }
});

add_task(async function test_other_clients_notified_on_first_sync() {
  _("Ensure that other clients are notified when we upload our client record for the first time.");

  engine.resetLastSync();
  engine._store.wipe();
  await generateNewKeys(Service.collectionKeys);

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  const fxAccounts = engine.fxAccounts;
  let calls = 0;
  engine.fxAccounts = {
    getDeviceId() { return fxAccounts.getDeviceId(); },
    notifyDevices() {
      calls++;
      return Promise.resolve(true);
    }
  };

  try {
    engine.lastRecordUpload = 0;
    _("First sync, should notify other clients");
    await syncClientsEngine(server);
    equal(calls, 1);

    _("Second sync, should not notify other clients");
    await syncClientsEngine(server);
    equal(calls, 1);
  } finally {
    engine.fxAccounts = fxAccounts;
    cleanup();
    await promiseStopServer(server);
  }
});

add_task(async function device_disconnected_notification_updates_known_stale_clients() {
  const spyUpdate = sinon.spy(engine, "updateKnownStaleClients");
  const makeFakeClient = (id) => ({ id, fxaDeviceId: `fxa-${id}` });
  const clients = [makeFakeClient("one"), makeFakeClient("two"), makeFakeClient("three")];
  const stubRemoteClients = sinon.stub(engine._store, "_remoteClients").get(() => {
    return clients;
  });
  const stubRefresh = sinon.stub(engine, "_refreshKnownStaleClients", () => {
    engine._knownStaleFxADeviceIds = ["fxa-one", "fxa-two"];
  });

  engine._knownStaleFxADeviceIds = null;
  Services.obs.notifyObservers(null, "fxaccounts:device_disconnected",
                               JSON.stringify({ isLocalDevice: false }));
  ok(spyUpdate.calledOnce, "updateKnownStaleClients should be called");
  ok(clients[0].stale);
  ok(clients[1].stale);
  ok(!clients[2].stale);
  spyUpdate.reset();

  ok(engine._knownStaleFxADeviceIds);
  Services.obs.notifyObservers(null, "fxaccounts:device_disconnected",
                               JSON.stringify({ isLocalDevice: false }));
  ok(spyUpdate.calledOnce, "updateKnownStaleClients should be called");
  spyUpdate.reset();

  Services.obs.notifyObservers(null, "fxaccounts:device_disconnected",
                               JSON.stringify({ isLocalDevice: true }));
  ok(spyUpdate.notCalled, "updateKnownStaleClients should not be called");

  stubRemoteClients.restore();
  spyUpdate.restore();
  stubRefresh.restore();
});

add_task(async function process_incoming_refreshes_known_stale_clients() {
  const stubProcessIncoming = sinon.stub(SyncEngine.prototype, "_processIncoming");
  const stubRefresh = sinon.stub(engine, "_refreshKnownStaleClients", () => {
    engine._knownStaleFxADeviceIds = ["one", "two"];
  });

  engine._knownStaleFxADeviceIds = null;
  await engine._processIncoming();
  ok(stubRefresh.calledOnce, "Should refresh the known stale clients");
  stubRefresh.reset();

  await engine._processIncoming();
  ok(stubRefresh.notCalled, "Should not refresh the known stale clients since it's already populated");

  stubProcessIncoming.restore();
  stubRefresh.restore();
});

add_task(async function process_incoming_refreshes_known_stale_clients() {
  Services.prefs.clearUserPref("services.sync.clients.lastModifiedOnProcessCommands");
  engine._localClientLastModified = Math.round(Date.now() / 1000);

  const stubRemoveLocalCommand = sinon.stub(engine, "removeLocalCommand");
  const tabProcessedSpy = sinon.spy(engine, "_handleDisplayURIs");
  engine.localCommands = [{ command: "displayURI", args: ["https://foo.bar", "fxaid1", "foo"] }];

  await engine.processIncomingCommands();
  ok(tabProcessedSpy.calledOnce);
  // Let's say we failed to upload and we end up calling processIncomingCommands again
  await engine.processIncomingCommands();
  ok(tabProcessedSpy.calledOnce);

  tabProcessedSpy.restore();
  stubRemoveLocalCommand.restore();
});

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Clients").level = Log.Level.Trace;
  run_next_test();
}
