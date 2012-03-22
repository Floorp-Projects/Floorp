/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/service.js");

const MORE_THAN_CLIENTS_TTL_REFRESH = 691200; // 8 days
const LESS_THAN_CLIENTS_TTL_REFRESH = 86400;  // 1 day

add_test(function test_bad_hmac() {
  _("Ensure that Clients engine deletes corrupt records.");
  let contents = {
    meta: {global: {engines: {clients: {version: Clients.version,
                                        syncID: Clients.syncID}}}},
    clients: {},
    crypto: {}
  };
  let deletedCollections = [];
  let deletedItems       = [];
  let callback = {
    __proto__: SyncServerCallback,
    onItemDeleted: function (username, coll, wboID) {
      deletedItems.push(coll + "/" + wboID);
    },
    onCollectionDeleted: function (username, coll) {
      deletedCollections.push(coll);
    }
  }
  let server = serverForUsers({"foo": "password"}, contents, callback);
  let user   = server.user("foo");

  function check_clients_count(expectedCount) {
    let stack = Components.stack.caller;
    let coll  = user.collection("clients");

    // Treat a non-existent collection as empty.
    do_check_eq(expectedCount, coll ? coll.count() : 0, stack);
  }

  function check_client_deleted(id) {
    let coll = user.collection("clients");
    let wbo  = coll.wbo(id);
    return !wbo || !wbo.payload;
  }

  function uploadNewKeys() {
    generateNewKeys();
    let serverKeys = CollectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Weave.Identity.syncKeyBundle);
    do_check_true(serverKeys.upload(Weave.Service.cryptoKeysURL).success);
  }

  try {
    let passphrase     = "abcdeabcdeabcdeabcdeabcdea";
    Service.serverURL  = TEST_SERVER_URL;
    Service.clusterURL = TEST_CLUSTER_URL;
    Service.login("foo", "ilovejane", passphrase);

    generateNewKeys();

    _("First sync, client record is uploaded");
    do_check_eq(Clients.lastRecordUpload, 0);
    check_clients_count(0);
    Clients._sync();
    check_clients_count(1);
    do_check_true(Clients.lastRecordUpload > 0);

    // Initial setup can wipe the server, so clean up.
    deletedCollections = [];
    deletedItems       = [];

    _("Change our keys and our client ID, reupload keys.");
    let oldLocalID  = Clients.localID;     // Preserve to test for deletion!
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    generateNewKeys();
    let serverKeys = CollectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Weave.Identity.syncKeyBundle);
    do_check_true(serverKeys.upload(Weave.Service.cryptoKeysURL).success);

    _("Sync.");
    Clients._sync();

    _("Old record " + oldLocalID + " was deleted, new one uploaded.");
    check_clients_count(1);
    check_client_deleted(oldLocalID);

    _("Now change our keys but don't upload them. " +
      "That means we get an HMAC error but redownload keys.");
    Service.lastHMACEvent = 0;
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    generateNewKeys();
    deletedCollections = [];
    deletedItems       = [];
    check_clients_count(1);
    Clients._sync();

    _("Old record was not deleted, new one uploaded.");
    do_check_eq(deletedCollections.length, 0);
    do_check_eq(deletedItems.length, 0);
    check_clients_count(2);

    _("Now try the scenario where our keys are wrong *and* there's a bad record.");
    // Clean up and start fresh.
    user.collection("clients")._wbos = {};
    Service.lastHMACEvent = 0;
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    deletedCollections = [];
    deletedItems       = [];
    check_clients_count(0);

    uploadNewKeys();

    // Sync once to upload a record.
    Clients._sync();
    check_clients_count(1);

    // Generate and upload new keys, so the old client record is wrong.
    uploadNewKeys();

    // Create a new client record and new keys. Now our keys are wrong, as well
    // as the object on the server. We'll download the new keys and also delete
    // the bad client record.
    oldLocalID  = Clients.localID;         // Preserve to test for deletion!
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    generateNewKeys();
    let oldKey = CollectionKeys.keyForCollection();

    do_check_eq(deletedCollections.length, 0);
    do_check_eq(deletedItems.length, 0);
    Clients._sync();
    do_check_eq(deletedItems.length, 1);
    check_client_deleted(oldLocalID);
    check_clients_count(1);
    let newKey = CollectionKeys.keyForCollection();
    do_check_false(oldKey.equals(newKey));

  } finally {
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_properties() {
  _("Test lastRecordUpload property");
  try {
    do_check_eq(Svc.Prefs.get("clients.lastRecordUpload"), undefined);
    do_check_eq(Clients.lastRecordUpload, 0);

    let now = Date.now();
    Clients.lastRecordUpload = now / 1000;
    do_check_eq(Clients.lastRecordUpload, Math.floor(now / 1000));
  } finally {
    Svc.Prefs.resetBranch("");
    run_next_test();
  }
});

add_test(function test_sync() {
  _("Ensure that Clients engine uploads a new client record once a week.");

  new SyncTestingInfrastructure();
  generateNewKeys();

  let contents = {
    meta: {global: {engines: {clients: {version: Clients.version,
                                        syncID: Clients.syncID}}}},
    clients: {},
    crypto: {}
  };
  let server = serverForUsers({"foo": "password"}, contents);
  let user   = server.user("foo");

  function clientWBO() {
    return user.collection("clients").wbo(Clients.localID);
  }

  try {

    _("First sync. Client record is uploaded.");
    do_check_eq(clientWBO(), undefined);
    do_check_eq(Clients.lastRecordUpload, 0);
    Clients._sync();
    do_check_true(!!clientWBO().payload);
    do_check_true(Clients.lastRecordUpload > 0);

    _("Let's time travel more than a week back, new record should've been uploaded.");
    Clients.lastRecordUpload -= MORE_THAN_CLIENTS_TTL_REFRESH;
    let lastweek = Clients.lastRecordUpload;
    clientWBO().payload = undefined;
    Clients._sync();
    do_check_true(!!clientWBO().payload);
    do_check_true(Clients.lastRecordUpload > lastweek);

    _("Remove client record.");
    Clients.removeClientData();
    do_check_eq(clientWBO().payload, undefined);

    _("Time travel one day back, no record uploaded.");
    Clients.lastRecordUpload -= LESS_THAN_CLIENTS_TTL_REFRESH;
    let yesterday = Clients.lastRecordUpload;
    Clients._sync();
    do_check_eq(clientWBO().payload, undefined);
    do_check_eq(Clients.lastRecordUpload, yesterday);

  } finally {
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_client_name_change() {
  _("Ensure client name change incurs a client record update.");

  let tracker = Clients._tracker;

  let localID = Clients.localID;
  let initialName = Clients.localName;

  Svc.Obs.notify("weave:engine:start-tracking");
  _("initial name: " + initialName);

  // Tracker already has data, so clear it.
  tracker.clearChangedIDs();

  let initialScore = tracker.score;

  do_check_eq(Object.keys(tracker.changedIDs).length, 0);

  Svc.Prefs.set("client.name", "new name");

  _("new name: " + Clients.localName);
  do_check_neq(initialName, Clients.localName);
  do_check_eq(Object.keys(tracker.changedIDs).length, 1);
  do_check_true(Clients.localID in tracker.changedIDs);
  do_check_true(tracker.score > initialScore);
  do_check_true(tracker.score >= SCORE_INCREMENT_XLARGE);

  Svc.Obs.notify("weave:engine:stop-tracking");

  run_next_test();
});

add_test(function test_send_command() {
  _("Verifies _sendCommandToClient puts commands in the outbound queue.");

  let store = Clients._store;
  let tracker = Clients._tracker;
  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);

  store.create(rec);
  let remoteRecord = store.createRecord(remoteId, "clients");

  let action = "testCommand";
  let args = ["foo", "bar"];

  Clients._sendCommandToClient(action, args, remoteId);

  let newRecord = store._remoteClients[remoteId];
  do_check_neq(newRecord, undefined);
  do_check_eq(newRecord.commands.length, 1);

  let command = newRecord.commands[0];
  do_check_eq(command.command, action);
  do_check_eq(command.args.length, 2);
  do_check_eq(command.args, args);

  do_check_neq(tracker.changedIDs[remoteId], undefined);

  run_next_test();
});

add_test(function test_command_validation() {
  _("Verifies that command validation works properly.");

  let store = Clients._store;

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

  for each (let [action, args, expectedResult] in testCommands) {
    let remoteId = Utils.makeGUID();
    let rec = new ClientsRec("clients", remoteId);

    store.create(rec);
    store.createRecord(remoteId, "clients");

    Clients.sendCommand(action, args, remoteId);

    let newRecord = store._remoteClients[remoteId];
    do_check_neq(newRecord, undefined);

    if (expectedResult) {
      _("Ensuring command is sent: " + action);
      do_check_eq(newRecord.commands.length, 1);

      let command = newRecord.commands[0];
      do_check_eq(command.command, action);
      do_check_eq(command.args, args);

      do_check_neq(Clients._tracker, undefined);
      do_check_neq(Clients._tracker.changedIDs[remoteId], undefined);
    } else {
      _("Ensuring command is scrubbed: " + action);
      do_check_eq(newRecord.commands, undefined);

      if (store._tracker) {
        do_check_eq(Clients._tracker[remoteId], undefined);
      }
    }

  }
  run_next_test();
});

add_test(function test_command_duplication() {
  _("Ensures duplicate commands are detected and not added");

  let store = Clients._store;
  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);
  store.create(rec);
  store.createRecord(remoteId, "clients");

  let action = "resetAll";
  let args = [];

  Clients.sendCommand(action, args, remoteId);
  Clients.sendCommand(action, args, remoteId);

  let newRecord = store._remoteClients[remoteId];
  do_check_eq(newRecord.commands.length, 1);

  _("Check variant args length");
  newRecord.commands = [];

  action = "resetEngine";
  Clients.sendCommand(action, [{ x: "foo" }], remoteId);
  Clients.sendCommand(action, [{ x: "bar" }], remoteId);

  _("Make sure we spot a real dupe argument.");
  Clients.sendCommand(action, [{ x: "bar" }], remoteId);

  do_check_eq(newRecord.commands.length, 2);

  run_next_test();
});

add_test(function test_command_invalid_client() {
  _("Ensures invalid client IDs are caught");

  let id = Utils.makeGUID();
  let error;

  try {
    Clients.sendCommand("wipeAll", [], id);
  } catch (ex) {
    error = ex;
  }

  do_check_eq(error.message.indexOf("Unknown remote client ID: "), 0);

  run_next_test();
});

add_test(function test_process_incoming_commands() {
  _("Ensures local commands are executed");

  Clients.localCommands = [{ command: "logout", args: [] }];

  let ev = "weave:service:logout:finish";

  var handler = function() {
    Svc.Obs.remove(ev, handler);
    run_next_test();
  };

  Svc.Obs.add(ev, handler);

  // logout command causes processIncomingCommands to return explicit false.
  do_check_false(Clients.processIncomingCommands());
});

add_test(function test_command_sync() {
  _("Ensure that commands are synced across clients.");

  new SyncTestingInfrastructure();

  Clients._store.wipe();
  generateNewKeys();

  let contents = {
    meta: {global: {engines: {clients: {version: Clients.version,
                                        syncID: Clients.syncID}}}},
    clients: {},
    crypto: {}
  };
  let server   = serverForUsers({"foo": "password"}, contents);
  let user     = server.user("foo");
  let remoteId = Utils.makeGUID();

  function clientWBO(id) {
    return user.collection("clients").wbo(id);
  }

  _("Create remote client record");
  let rec = new ClientsRec("clients", remoteId);
  Clients._store.create(rec);
  let remoteRecord = Clients._store.createRecord(remoteId, "clients");
  Clients.sendCommand("wipeAll", []);

  let clientRecord = Clients._store._remoteClients[remoteId];
  do_check_neq(clientRecord, undefined);
  do_check_eq(clientRecord.commands.length, 1);

  try {
    _("Syncing.");
    Clients._sync();
    _("Checking record was uploaded.");
    do_check_neq(clientWBO(Clients.localID).payload, undefined);
    do_check_true(Clients.lastRecordUpload > 0);

    do_check_neq(clientWBO(remoteId).payload, undefined);

    Svc.Prefs.set("client.GUID", remoteId);
    Clients._resetClient();
    do_check_eq(Clients.localID, remoteId);
    _("Performing sync on resetted client.");
    Clients._sync();
    do_check_neq(Clients.localCommands, undefined);
    do_check_eq(Clients.localCommands.length, 1);

    let command = Clients.localCommands[0];
    do_check_eq(command.command, "wipeAll");
    do_check_eq(command.args.length, 0);

  } finally {
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    server.stop(run_next_test);
  }
});

add_test(function test_send_uri_to_client_for_display() {
  _("Ensure sendURIToClientForDisplay() sends command properly.");

  let tracker = Clients._tracker;
  let store = Clients._store;

  let remoteId = Utils.makeGUID();
  let rec = new ClientsRec("clients", remoteId);
  rec.name = "remote";
  store.create(rec);
  let remoteRecord = store.createRecord(remoteId, "clients");

  tracker.clearChangedIDs();
  let initialScore = tracker.score;

  let uri = "http://www.mozilla.org/";
  Clients.sendURIToClientForDisplay(uri, remoteId);

  let newRecord = store._remoteClients[remoteId];

  do_check_neq(newRecord, undefined);
  do_check_eq(newRecord.commands.length, 1);

  let command = newRecord.commands[0];
  do_check_eq(command.command, "displayURI");
  do_check_eq(command.args.length, 2);
  do_check_eq(command.args[0], uri);

  do_check_true(tracker.score > initialScore);
  do_check_true(tracker.score - initialScore >= SCORE_INCREMENT_XLARGE);

  _("Ensure unknown client IDs result in exception.");
  let unknownId = Utils.makeGUID();
  let error;

  try {
    Clients.sendURIToClientForDisplay(uri, unknownId);
  } catch (ex) {
    error = ex;
  }

  do_check_eq(error.message.indexOf("Unknown remote client ID: "), 0);

  run_next_test();
});

add_test(function test_receive_display_uri() {
  _("Ensure processing of received 'displayURI' commands works.");

  // We don't set up WBOs and perform syncing because other tests verify
  // the command API works as advertised. This saves us a little work.

  let uri = "http://www.mozilla.org/";
  let remoteId = Utils.makeGUID();

  let command = {
    command: "displayURI",
    args: [uri, remoteId],
  };

  Clients.localCommands = [command];

  // Received 'displayURI' command should result in the topic defined below
  // being called.
  let ev = "weave:engine:clients:display-uri";

  let handler = function(subject, data) {
    Svc.Obs.remove(ev, handler);

    do_check_eq(subject.uri, uri);
    do_check_eq(subject.client, remoteId);
    do_check_eq(data, null);

    run_next_test();
  };

  Svc.Obs.add(ev, handler);

  do_check_true(Clients.processIncomingCommands());
});

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Clients").level = Log4Moz.Level.Trace;
  run_next_test();
}
