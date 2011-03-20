Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/service.js");

const MORE_THAN_CLIENTS_TTL_REFRESH = 691200; // 8 days
const LESS_THAN_CLIENTS_TTL_REFRESH = 86400; // 1 day

function test_bad_hmac() {
  _("Ensure that Clients engine deletes corrupt records.");
  let global = new ServerWBO('global',
                             {engines: {clients: {version: Clients.version,
                                                  syncID: Clients.syncID}}});
  let clientsColl = new ServerCollection({}, true);
  let keysWBO = new ServerWBO("keys");

  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  // Watch for deletions in the given collection.
  let deleted = false;
  function trackDeletedHandler(coll, handler) {
    let u = upd(coll, handler);
    return function(request, response) {
      if (request.method == "DELETE")
        deleted = true;

      return u(request, response);
    };
  }

  let handlers = {
    "/1.0/foo/info/collections": collectionsHelper.handler,
    "/1.0/foo/storage/meta/global": upd("meta", global.handler()),
    "/1.0/foo/storage/crypto/keys": upd("crypto", keysWBO.handler()),
    "/1.0/foo/storage/clients": trackDeletedHandler("crypto", clientsColl.handler())
  };

  let server = httpd_setup(handlers);
  do_test_pending();

  try {
    let passphrase = "abcdeabcdeabcdeabcdeabcdea";
    Service.serverURL = "http://localhost:8080/";
    Service.clusterURL = "http://localhost:8080/";
    Service.login("foo", "ilovejane", passphrase);

    CollectionKeys.generateNewKeys();

    _("First sync, client record is uploaded");
    do_check_eq(0, clientsColl.count());
    do_check_eq(Clients.lastRecordUpload, 0);
    Clients.sync();
    do_check_eq(1, clientsColl.count());
    do_check_true(Clients.lastRecordUpload > 0);
    deleted = false;    // Initial setup can wipe the server, so clean up.

    _("Records now: " + clientsColl.get({}));
    _("Change our keys and our client ID, reupload keys.");
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    CollectionKeys.generateNewKeys();
    let serverKeys = CollectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Weave.Service.syncKeyBundle);
    do_check_true(serverKeys.upload(Weave.Service.cryptoKeysURL).success);

    _("Sync.");
    do_check_true(!deleted);
    Clients.sync();

    _("Old record was deleted, new one uploaded.");
    do_check_true(deleted);
    do_check_eq(1, clientsColl.count());
    _("Records now: " + clientsColl.get({}));

    _("Now change our keys but don't upload them. " +
      "That means we get an HMAC error but redownload keys.");
    Service.lastHMACEvent = 0;
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    CollectionKeys.generateNewKeys();
    deleted = false;
    do_check_eq(1, clientsColl.count());
    Clients.sync();

    _("Old record was not deleted, new one uploaded.");
    do_check_false(deleted);
    do_check_eq(2, clientsColl.count());
    _("Records now: " + clientsColl.get({}));

    _("Now try the scenario where our keys are wrong *and* there's a bad record.");
    // Clean up and start fresh.
    clientsColl.wbos = {};
    Service.lastHMACEvent = 0;
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    deleted = false;
    do_check_eq(0, clientsColl.count());

    // Create and upload keys.
    CollectionKeys.generateNewKeys();
    serverKeys = CollectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Weave.Service.syncKeyBundle);
    do_check_true(serverKeys.upload(Weave.Service.cryptoKeysURL).success);

    // Sync once to upload a record.
    Clients.sync();
    do_check_eq(1, clientsColl.count());

    // Generate and upload new keys, so the old client record is wrong.
    CollectionKeys.generateNewKeys();
    serverKeys = CollectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Weave.Service.syncKeyBundle);
    do_check_true(serverKeys.upload(Weave.Service.cryptoKeysURL).success);

    // Create a new client record and new keys. Now our keys are wrong, as well
    // as the object on the server. We'll download the new keys and also delete
    // the bad client record.
    Clients.localID = Utils.makeGUID();
    Clients.resetClient();
    CollectionKeys.generateNewKeys();
    let oldKey = CollectionKeys.keyForCollection();

    do_check_false(deleted);
    Clients.sync();
    do_check_true(deleted);
    do_check_eq(1, clientsColl.count());
    let newKey = CollectionKeys.keyForCollection();
    do_check_false(oldKey.equals(newKey));

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
  }
}

function test_properties() {
  try {
    _("Test lastRecordUpload property");
    do_check_eq(Svc.Prefs.get("clients.lastRecordUpload"), undefined);
    do_check_eq(Clients.lastRecordUpload, 0);

    let now = Date.now();
    Clients.lastRecordUpload = now / 1000;
    do_check_eq(Clients.lastRecordUpload, Math.floor(now / 1000));
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_sync() {
  _("Ensure that Clients engine uploads a new client record once a week.");
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  new SyncTestingInfrastructure();

  CollectionKeys.generateNewKeys();

  let global = new ServerWBO('global',
                             {engines: {clients: {version: Clients.version,
                                                  syncID: Clients.syncID}}});
  let coll = new ServerCollection();
  let clientwbo = coll.wbos[Clients.localID] = new ServerWBO(Clients.localID);
  let server = httpd_setup({
      "/1.0/foo/storage/meta/global": global.handler(),
      "/1.0/foo/storage/clients": coll.handler()
  });
  do_test_pending();

  try {

    _("First sync, client record is uploaded");
    do_check_eq(clientwbo.payload, undefined);
    do_check_eq(Clients.lastRecordUpload, 0);
    Clients.sync();
    do_check_true(!!clientwbo.payload);
    do_check_true(Clients.lastRecordUpload > 0);

    _("Let's time travel more than a week back, new record should've been uploaded.");
    Clients.lastRecordUpload -= MORE_THAN_CLIENTS_TTL_REFRESH;
    let lastweek = Clients.lastRecordUpload;
    clientwbo.payload = undefined;
    Clients.sync();
    do_check_true(!!clientwbo.payload);
    do_check_true(Clients.lastRecordUpload > lastweek);

    _("Time travel one day back, no record uploaded.");
    Clients.lastRecordUpload -= LESS_THAN_CLIENTS_TTL_REFRESH;
    let yesterday = Clients.lastRecordUpload;
    clientwbo.payload = undefined;
    Clients.sync();
    do_check_eq(clientwbo.payload, undefined);
    do_check_eq(Clients.lastRecordUpload, yesterday);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
  }
}


function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Engine.Clients").level = Log4Moz.Level.Trace;
  test_bad_hmac();      // Needs to run first: doesn't use fake service!
  test_properties();
  test_sync();
}
