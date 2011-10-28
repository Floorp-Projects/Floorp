Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/log4moz.js");

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let guidSvc = new FakeGUIDService();
  let clients = new ServerCollection();
  let meta_global = new ServerWBO('global');

  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;
  
  function wasCalledHandler(wbo) {
    let handler = wbo.handler();
    return function() {
      wbo.wasCalled = true;
      handler.apply(this, arguments);
    };
  }

  let keysWBO = new ServerWBO("keys");
  let cryptoColl = new ServerCollection({keys: keysWBO});
  let metaColl = new ServerCollection({global: meta_global});
  do_test_pending();

  /**
   * Handle the bulk DELETE request sent by wipeServer.
   */
  function storageHandler(request, response) {
    do_check_eq("DELETE", request.method);
    do_check_true(request.hasHeader("X-Confirm-Delete"));

    _("Wiping out all collections.");
    cryptoColl.delete({});
    clients.delete({});
    metaColl.delete({});

    let ts = new_timestamp();
    collectionsHelper.update_collection("crypto", ts);
    collectionsHelper.update_collection("clients", ts);
    collectionsHelper.update_collection("meta", ts);
    return_timestamp(request, response, ts);
  }

  let server = httpd_setup({
    "/1.1/johndoe/storage": storageHandler,
    "/1.1/johndoe/storage/crypto/keys": upd("crypto", keysWBO.handler()),
    "/1.1/johndoe/storage/crypto": upd("crypto", cryptoColl.handler()),
    "/1.1/johndoe/storage/clients": upd("clients", clients.handler()),
    "/1.1/johndoe/storage/meta/global": upd("meta", wasCalledHandler(meta_global)),
    "/1.1/johndoe/storage/meta": upd("meta", wasCalledHandler(metaColl)),
    "/1.1/johndoe/info/collections": collectionsHelper.handler
  });

  try {
    _("Log in.");
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    
    _("Checking Status.sync with no credentials.");
    Weave.Service.verifyAndFetchSymmetricKeys();
    do_check_eq(Status.sync, CREDENTIALS_CHANGED);
    do_check_eq(Status.login, LOGIN_FAILED_INVALID_PASSPHRASE);

    _("Log in with an old secret phrase, is upgraded to Sync Key.");
    Weave.Service.login("johndoe", "ilovejane", "my old secret phrase!!1!");
    do_check_true(Weave.Service.isLoggedIn);
    do_check_true(Utils.isPassphrase(Weave.Service.passphrase));
    do_check_true(Utils.isPassphrase(Weave.Service.syncKeyBundle.keyStr));
    let syncKey = Weave.Service.passphrase;
    Weave.Service.startOver();

    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    Weave.Service.login("johndoe", "ilovejane", syncKey);
    do_check_true(Weave.Service.isLoggedIn);

    _("Checking that remoteSetup returns true when credentials have changed.");
    Records.get(Weave.Service.metaURL).payload.syncID = "foobar";
    do_check_true(Weave.Service._remoteSetup());
    
    _("Do an initial sync.");
    let beforeSync = Date.now()/1000;
    Weave.Service.sync();

    _("Checking that remoteSetup returns true.");
    do_check_true(Weave.Service._remoteSetup());

    _("Verify that the meta record was uploaded.");
    do_check_eq(meta_global.data.syncID, Weave.Service.syncID);
    do_check_eq(meta_global.data.storageVersion, Weave.STORAGE_VERSION);
    do_check_eq(meta_global.data.engines.clients.version, Weave.Clients.version);
    do_check_eq(meta_global.data.engines.clients.syncID, Weave.Clients.syncID);

    _("Set the collection info hash so that sync() will remember the modified times for future runs.");
    collections.meta = Weave.Clients.lastSync;
    collections.clients = Weave.Clients.lastSync;
    Weave.Service.sync();

    _("Sync again and verify that meta/global wasn't downloaded again");
    meta_global.wasCalled = false;
    Weave.Service.sync();
    do_check_false(meta_global.wasCalled);

    _("Fake modified records. This will cause a redownload, but not reupload since it hasn't changed.");
    collections.meta += 42;
    meta_global.wasCalled = false;

    let metaModified = meta_global.modified;

    Weave.Service.sync();
    do_check_true(meta_global.wasCalled);
    do_check_eq(metaModified, meta_global.modified);

    _("Checking bad passphrases.");
    let pp = Weave.Service.passphrase;
    Weave.Service.passphrase = "notvalid";
    do_check_false(Weave.Service.verifyAndFetchSymmetricKeys());
    do_check_eq(Status.sync, CREDENTIALS_CHANGED);
    do_check_eq(Status.login, LOGIN_FAILED_INVALID_PASSPHRASE);
    Weave.Service.passphrase = pp;
    do_check_true(Weave.Service.verifyAndFetchSymmetricKeys());
    
    // changePassphrase wipes our keys, and they're regenerated on next sync.
    _("Checking changed passphrase.");
    let existingDefault = CollectionKeys.keyForCollection();
    let existingKeysPayload = keysWBO.payload;
    let newPassphrase = "bbbbbabcdeabcdeabcdeabcdea";
    Weave.Service.changePassphrase(newPassphrase);
    
    _("Local key cache is full, but different.");
    do_check_true(!!CollectionKeys._default);
    do_check_false(CollectionKeys._default.equals(existingDefault));
    
    _("Server has new keys.");
    do_check_true(!!keysWBO.payload);
    do_check_true(!!keysWBO.modified);
    do_check_neq(keysWBO.payload, existingKeysPayload);

    // Try to screw up HMAC calculation.
    // Re-encrypt keys with a new random keybundle, and upload them to the
    // server, just as might happen with a second client.
    _("Attempting to screw up HMAC by re-encrypting keys.");
    let keys = CollectionKeys.asWBO();
    let b = new BulkKeyBundle("hmacerror", "hmacerror");
    b.generateRandom();
    collections.crypto = keys.modified = 100 + (Date.now()/1000);  // Future modification time.
    keys.encrypt(b);
    keys.upload(Weave.Service.cryptoKeysURL);
    
    do_check_false(Weave.Service.verifyAndFetchSymmetricKeys());
    do_check_eq(Status.login, LOGIN_FAILED_INVALID_PASSPHRASE);

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
