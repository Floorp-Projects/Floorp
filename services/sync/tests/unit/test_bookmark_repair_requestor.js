/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
Cu.import("resource://services-sync/bookmark_repair.js");

initTestLogging("Trace");

function makeClientRecord(id, fields = {}) {
  return {
    id,
    version: fields.version || "54.0a1",
    type: fields.type || "desktop",
    stale: fields.stale || false,
    serverLastModified: fields.serverLastModified || 0,
  }
}

class MockClientsEngine {
  constructor(clientList) {
    this._clientList = clientList;
    this._sentCommands = {}
  }

  get remoteClients() {
    return Object.values(this._clientList);
  }

  remoteClient(id) {
    return this._clientList[id];
  }

  sendCommand(command, args, clientID) {
    let cc = this._sentCommands[clientID] || [];
    cc.push({ command, args });
    this._sentCommands[clientID] = cc;
  }

  getClientCommands(clientID) {
    return this._sentCommands[clientID] || [];
  }
}

class MockIdentity {
  hashedDeviceID(did) {
    return did; // don't hash it to make testing easier.
  }
}

class MockService {
  constructor(clientList) {
    this.clientsEngine = new MockClientsEngine(clientList);
    this.identity = new MockIdentity();
    this._recordedEvents = [];
  }

  recordTelemetryEvent(object, method, value, extra = undefined) {
    this._recordedEvents.push({ method, object, value, extra });
  }
}

function checkState(expected) {
  equal(Services.prefs.getCharPref("services.sync.repairs.bookmarks.state"), expected);
}

function checkRepairFinished() {
  try {
    let state = Services.prefs.getCharPref("services.sync.repairs.bookmarks.state");
    ok(false, state);
  } catch (ex) {
    ok(true, "no repair preference exists");
  }
}

function checkOutgoingCommand(service, clientID) {
  let sent = service.clientsEngine._sentCommands;
  deepEqual(Object.keys(sent), [clientID]);
  equal(sent[clientID].length, 1);
  equal(sent[clientID][0].command, "repairRequest");
}

function NewBookmarkRepairRequestor(mockService) {
  let req = new BookmarkRepairRequestor(mockService);
  req._now = () => Date.now() / 1000; // _now() is seconds.
  return req;
}

add_task(async function test_requestor_no_clients() {
  let mockService = new MockService({ });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();

  requestor.startRepairs(validationInfo, flowID);
  // there are no clients, so we should end up in "finished" (which we need to
  // check via telemetry)
  deepEqual(mockService._recordedEvents, [
    { object: "repair",
      method: "started",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    },
    { object: "repair",
      method: "finished",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    }
  ]);
});

add_task(async function test_requestor_one_client_no_response() {
  let mockService = new MockService({ "client-a": makeClientRecord("client-a") });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();
  requestor.startRepairs(validationInfo, flowID);
  // the command should now be outgoing.
  checkOutgoingCommand(mockService, "client-a");

  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);
  // asking it to continue stays in that state until we timeout or the command
  // is removed.
  requestor.continueRepairs();
  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);

  // now pretend that client synced.
  mockService.clientsEngine._sentCommands = {};
  requestor.continueRepairs();
  checkState(BookmarkRepairRequestor.STATE.SENT_SECOND_REQUEST);
  // the command should be outgoing again.
  checkOutgoingCommand(mockService, "client-a");

  // pretend that client synced again without writing a command.
  mockService.clientsEngine._sentCommands = {};
  requestor.continueRepairs();
  // There are no more clients, so we've given up.

  checkRepairFinished();
  deepEqual(mockService._recordedEvents, [
    { object: "repair",
      method: "started",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-a" },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-a" },
    },
    { object: "repair",
      method: "finished",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    }
  ]);
});

add_task(async function test_requestor_one_client_no_sync() {
  let mockService = new MockService({ "client-a": makeClientRecord("client-a") });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();
  requestor.startRepairs(validationInfo, flowID);
  // the command should now be outgoing.
  checkOutgoingCommand(mockService, "client-a");

  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);

  // pretend we are now in the future.
  let theFuture = Date.now() + 300000000;
  requestor._now = () => theFuture;

  requestor.continueRepairs();

  // We should be finished as we gave up in disgust.
  checkRepairFinished();
  deepEqual(mockService._recordedEvents, [
    { object: "repair",
      method: "started",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-a" },
    },
    { object: "repair",
      method: "abandon",
      value: "silent",
      extra: { flowID, deviceID: "client-a" },
    },
    { object: "repair",
      method: "finished",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    }
  ]);
});

add_task(async function test_requestor_latest_client_used() {
  let mockService = new MockService({
    "client-early": makeClientRecord("client-early", { serverLastModified: Date.now() - 10 }),
    "client-late": makeClientRecord("client-late", { serverLastModified: Date.now() }),
  });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        { parent: "x", child: "a" },
      ],
      orphans: [],
    }
  }
  requestor.startRepairs(validationInfo, Utils.makeGUID());
  // the repair command should be outgoing to the most-recent client.
  checkOutgoingCommand(mockService, "client-late");
  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);
  // and this test is done - reset the repair.
  requestor.prefs.resetBranch();
});

add_task(async function test_requestor_client_vanishes() {
  let mockService = new MockService({
    "client-a": makeClientRecord("client-a"),
    "client-b": makeClientRecord("client-b"),
  });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();
  requestor.startRepairs(validationInfo, flowID);
  // the command should now be outgoing.
  checkOutgoingCommand(mockService, "client-a");

  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);

  mockService.clientsEngine._sentCommands = {};
  // Now let's pretend the client vanished.
  delete mockService.clientsEngine._clientList["client-a"];

  requestor.continueRepairs();
  // We should have moved on to client-b.
  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);
  checkOutgoingCommand(mockService, "client-b");

  // Now let's pretend client B wrote all missing IDs.
  let response = {
    collection: "bookmarks",
    request: "upload",
    flowID: requestor._flowID,
    clientID: "client-b",
    ids: ["a", "b", "c"],
  }
  requestor.continueRepairs(response);

  // We should be finished as we got all our IDs.
  checkRepairFinished();
  deepEqual(mockService._recordedEvents, [
    { object: "repair",
      method: "started",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-a" },
    },
    { object: "repair",
      method: "abandon",
      value: "missing",
      extra: { flowID, deviceID: "client-a" },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-b" },
    },
    { object: "repair",
      method: "response",
      value: "upload",
      extra: { flowID, deviceID: "client-b", numIDs: 3 },
    },
    { object: "repair",
      method: "finished",
      value: undefined,
      extra: { flowID, numIDs: 0 },
    }
  ]);
});

add_task(async function test_requestor_success_responses() {
  let mockService = new MockService({
    "client-a": makeClientRecord("client-a"),
    "client-b": makeClientRecord("client-b"),
  });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();
  requestor.startRepairs(validationInfo, flowID);
  // the command should now be outgoing.
  checkOutgoingCommand(mockService, "client-a");

  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);

  mockService.clientsEngine._sentCommands = {};
  // Now let's pretend the client wrote a response.
  let response = {
    collection: "bookmarks",
    request: "upload",
    clientID: "client-a",
    flowID: requestor._flowID,
    ids: ["a", "b"],
  }
  requestor.continueRepairs(response);
  // We should have moved on to client 2.
  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);
  checkOutgoingCommand(mockService, "client-b");

  // Now let's pretend client B write the missing ID.
  response = {
    collection: "bookmarks",
    request: "upload",
    clientID: "client-b",
    flowID: requestor._flowID,
    ids: ["c"],
  }
  requestor.continueRepairs(response);

  // We should be finished as we got all our IDs.
  checkRepairFinished();
  deepEqual(mockService._recordedEvents, [
    { object: "repair",
      method: "started",
      value: undefined,
      extra: { flowID, numIDs: 3 },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 3, deviceID: "client-a" },
    },
    { object: "repair",
      method: "response",
      value: "upload",
      extra: { flowID, deviceID: "client-a", numIDs: 2 },
    },
    { object: "repair",
      method: "request",
      value: "upload",
      extra: { flowID, numIDs: 1, deviceID: "client-b" },
    },
    { object: "repair",
      method: "response",
      value: "upload",
      extra: { flowID, deviceID: "client-b", numIDs: 1 },
    },
    { object: "repair",
      method: "finished",
      value: undefined,
      extra: { flowID, numIDs: 0 },
    }
  ]);
});

add_task(async function test_client_suitability() {
  let mockService = new MockService({
    "client-a": makeClientRecord("client-a"),
    "client-b": makeClientRecord("client-b", { type: "mobile" }),
    "client-c": makeClientRecord("client-c", { version: "52.0a1" }),
    "client-d": makeClientRecord("client-c", { version: "54.0a1" }),
  });
  let requestor = NewBookmarkRepairRequestor(mockService);
  ok(requestor._isSuitableClient(mockService.clientsEngine.remoteClient("client-a")));
  ok(!requestor._isSuitableClient(mockService.clientsEngine.remoteClient("client-b")));
  ok(!requestor._isSuitableClient(mockService.clientsEngine.remoteClient("client-c")));
  ok(requestor._isSuitableClient(mockService.clientsEngine.remoteClient("client-d")));
});

add_task(async function test_requestor_already_repairing_at_start() {
  let mockService = new MockService({ });
  let requestor = NewBookmarkRepairRequestor(mockService);
  requestor.anyClientsRepairing = () => true;
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();

  ok(!requestor.startRepairs(validationInfo, flowID),
     "Shouldn't start repairs");
  equal(mockService._recordedEvents.length, 1);
  equal(mockService._recordedEvents[0].method, "aborted");
});

add_task(async function test_requestor_already_repairing_continue() {
  let clientB = makeClientRecord("client-b")
  let mockService = new MockService({
    "client-a": makeClientRecord("client-a"),
    "client-b": clientB
  });
  let requestor = NewBookmarkRepairRequestor(mockService);
  let validationInfo = {
    problems: {
      missingChildren: [
        {parent: "x", child: "a"},
        {parent: "x", child: "b"},
        {parent: "x", child: "c"}
      ],
      orphans: [],
    }
  }
  let flowID = Utils.makeGUID();
  requestor.startRepairs(validationInfo, flowID);
  // the command should now be outgoing.
  checkOutgoingCommand(mockService, "client-a");

  checkState(BookmarkRepairRequestor.STATE.SENT_REQUEST);
  mockService.clientsEngine._sentCommands = {};

  // Now let's pretend the client wrote a response (it doesn't matter what's in here)
  let response = {
    collection: "bookmarks",
    request: "upload",
    clientID: "client-a",
    flowID: requestor._flowID,
    ids: ["a", "b"],
  };

  // and another client also started a request
  clientB.commands = [{
    args: [{ collection: "bookmarks", flowID: "asdf" }],
    command: "repairRequest",
  }];


  requestor.continueRepairs(response);

  // We should have aborted now
  checkRepairFinished();
  const expected = [
    { method: "started",
      object: "repair",
      value: undefined,
      extra: { flowID, numIDs: "3" },
    },
    { method: "request",
      object: "repair",
      value: "upload",
      extra: { flowID, numIDs: "3", deviceID: "client-a" },
    },
    { method: "aborted",
      object: "repair",
      value: undefined,
      extra: { flowID, numIDs: "3", reason: "other clients repairing" },
    }
  ];

  deepEqual(mockService._recordedEvents, expected);
});
