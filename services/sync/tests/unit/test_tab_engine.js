/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabProvider } = ChromeUtils.importESModule(
  "resource://services-sync/engines/tabs.sys.mjs"
);
const { WBORecord } = ChromeUtils.importESModule(
  "resource://services-sync/record.sys.mjs"
);
const { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);

let engine;
// We'll need the clients engine for testing as tabs is closely related
let clientsEngine;

async function syncClientsEngine(server) {
  clientsEngine._lastFxADevicesFetch = 0;
  clientsEngine.lastModified = server.getCollection("foo", "clients").timestamp;
  await clientsEngine._sync();
}

async function makeRemoteClients() {
  let server = await serverForFoo(clientsEngine);
  await configureIdentity({ username: "foo" }, server);
  await Service.login();

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  let remoteId = Utils.makeGUID();
  let remoteId2 = Utils.makeGUID();
  let collection = server.getCollection("foo", "clients");

  _("Create remote client records");
  collection.insertRecord({
    id: remoteId,
    name: "Remote client",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteId,
    fxaDeviceName: "Fxa - Remote client",
    protocols: ["1.5"],
  });

  collection.insertRecord({
    id: remoteId2,
    name: "Remote client 2",
    type: "desktop",
    commands: [],
    version: "48",
    fxaDeviceId: remoteId2,
    fxaDeviceName: "Fxa - Remote client 2",
    protocols: ["1.5"],
  });

  let fxAccounts = clientsEngine.fxAccounts;
  clientsEngine.fxAccounts = {
    notifyDevices() {
      return Promise.resolve(true);
    },
    device: {
      getLocalId() {
        return fxAccounts.device.getLocalId();
      },
      getLocalName() {
        return fxAccounts.device.getLocalName();
      },
      getLocalType() {
        return fxAccounts.device.getLocalType();
      },
      recentDeviceList: [{ id: remoteId, name: "remote device" }],
      refreshDeviceList() {
        return Promise.resolve(true);
      },
    },
    _internal: {
      now() {
        return Date.now();
      },
    },
  };

  await syncClientsEngine(server);
}

add_task(async function setup() {
  clientsEngine = Service.clientsEngine;
  // Make some clients to test with
  await makeRemoteClients();

  // Make the tabs engine for all the tests to use
  engine = Service.engineManager.get("tabs");
  await engine.initialize();

  // Since these are xpcshell tests, we'll need to mock this
  TabProvider.shouldSkipWindow = mockShouldSkipWindow;
});

add_task(async function test_tab_engine_skips_incoming_local_record() {
  _("Ensure incoming records that match local client ID are never applied.");

  let localID = clientsEngine.localID;
  let collection = new ServerCollection();

  _("Creating remote tab record with local client ID");
  let localRecord = encryptPayload({
    id: localID,
    clientName: "local",
    tabs: [
      {
        title: "title",
        urlHistory: ["http://foo.com/"],
        icon: "",
        lastUsed: 2000,
      },
    ],
  });
  collection.insert(localID, localRecord);

  _("Creating remote tab record with a different client ID");
  let remoteID = "fake-guid-00"; // remote should match one of the test clients
  let remoteRecord = encryptPayload({
    id: remoteID,
    clientName: "not local",
    tabs: [
      {
        title: "title2",
        urlHistory: ["http://bar.com/"],
        icon: "",
        lastUsed: 3000,
      },
    ],
  });
  collection.insert(remoteID, remoteRecord);

  _("Setting up Sync server");
  let server = sync_httpd_setup({
    "/1.1/foo/storage/tabs": collection.handler(),
  });

  await SyncTestingInfrastructure(server);

  let syncID = await engine.resetLocalSyncID();
  let meta_global = Service.recordManager.set(
    engine.metaURL,
    new WBORecord(engine.metaURL)
  );
  meta_global.payload.engines = {
    tabs: { version: engine.version, syncID },
  };

  await generateNewKeys(Service.collectionKeys);

  let promiseFinished = new Promise(resolve => {
    let syncFinish = engine._syncFinish;
    engine._syncFinish = async function () {
      let remoteTabs = await engine._rustStore.getAll();
      equal(
        remoteTabs.length,
        1,
        "Remote client record was applied and local wasn't"
      );
      let record = remoteTabs[0];
      equal(record.clientId, remoteID, "Remote client ID matches");

      _("Ensure getAllClients returns the correct shape");
      let clients = await engine.getAllClients();
      equal(clients.length, 1);
      let client = clients[0];
      equal(client.id, "fake-guid-00");
      equal(client.name, "Remote client");
      equal(client.type, "desktop");
      Assert.ok(client.lastModified); // lastModified should be filled in once serverModified is populated from the server
      deepEqual(client.tabs, [
        {
          title: "title2",
          urlHistory: ["http://bar.com/"],
          icon: "",
          inactive: false,
          lastUsed: 3000,
        },
      ]);
      await syncFinish.call(engine);
      resolve();
    };
  });

  _("Start sync");
  Service.scheduler.hasIncomingItems = false;
  await engine._sync();
  await promiseFinished;
  // Bug 1800185 - we don't want the sync scheduler to see these records as incoming.
  Assert.ok(!Service.scheduler.hasIncomingItems);
});

// Ensure we trim tabs in the case of going past the max payload size allowed
add_task(async function test_too_many_tabs() {
  let a_lot_of_tabs = [];

  for (let i = 0; i < 4000; ++i) {
    a_lot_of_tabs.push(
      `http://example${i}.com/some-super-long-url-chain-to-help-with-bytes`
    );
  }

  TabProvider.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    a_lot_of_tabs
  );

  let encoder = Utils.utf8Encoder;
  // see tryfitItems(..) in util.js
  const computeSerializedSize = records =>
    encoder.encode(JSON.stringify(records)).byteLength;

  const maxPayloadSize = Service.getMaxRecordPayloadSize();
  const maxSerializedSize = (maxPayloadSize / 4) * 3 - 1500;
  // We are over max payload size
  Assert.ok(computeSerializedSize(a_lot_of_tabs) > maxSerializedSize);
  let tabs = await engine.getTabsWithinPayloadSize();
  // We are now under max payload size
  Assert.ok(computeSerializedSize(tabs) < maxSerializedSize);
});
