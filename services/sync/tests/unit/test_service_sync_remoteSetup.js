/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://testing-common/services/sync/utils.js");

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
    Service.serverURL = server.baseURI;

    _("Checking Status.sync with no credentials.");
    Service.verifyAndFetchSymmetricKeys();
    do_check_eq(Service.status.sync, CREDENTIALS_CHANGED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NO_PASSPHRASE);

    _("Log in with an old secret phrase, is upgraded to Sync Key.");
    Service.login("johndoe", "ilovejane", "my old secret phrase!!1!");
    _("End of login");
    do_check_true(Service.isLoggedIn);
    do_check_true(Utils.isPassphrase(Service.identity.syncKey));
    let syncKey = Service.identity.syncKey;
    Service.startOver();

    Service.serverURL = server.baseURI;
    Service.login("johndoe", "ilovejane", syncKey);
    do_check_true(Service.isLoggedIn);

    _("Checking that remoteSetup returns true when credentials have changed.");
    Service.recordManager.get(Service.metaURL).payload.syncID = "foobar";
    do_check_true(Service._remoteSetup());

    _("Do an initial sync.");
    let beforeSync = Date.now()/1000;
    Service.sync();

    _("Checking that remoteSetup returns true.");
    do_check_true(Service._remoteSetup());

    _("Verify that the meta record was uploaded.");
    do_check_eq(meta_global.data.syncID, Service.syncID);
    do_check_eq(meta_global.data.storageVersion, STORAGE_VERSION);
    do_check_eq(meta_global.data.engines.clients.version, Service.clientsEngine.version);
    do_check_eq(meta_global.data.engines.clients.syncID, Service.clientsEngine.syncID);

    _("Set the collection info hash so that sync() will remember the modified times for future runs.");
    collections.meta = Service.clientsEngine.lastSync;
    collections.clients = Service.clientsEngine.lastSync;
    Service.sync();

    _("Sync again and verify that meta/global wasn't downloaded again");
    meta_global.wasCalled = false;
    Service.sync();
    do_check_false(meta_global.wasCalled);

    _("Fake modified records. This will cause a redownload, but not reupload since it hasn't changed.");
    collections.meta += 42;
    meta_global.wasCalled = false;

    let metaModified = meta_global.modified;

    Service.sync();
    do_check_true(meta_global.wasCalled);
    do_check_eq(metaModified, meta_global.modified);

    _("Checking bad passphrases.");
    let pp = Service.identity.syncKey;
    Service.identity.syncKey = "notvalid";
    do_check_false(Service.verifyAndFetchSymmetricKeys());
    do_check_eq(Service.status.sync, CREDENTIALS_CHANGED);
    do_check_eq(Service.status.login, LOGIN_FAILED_INVALID_PASSPHRASE);
    Service.identity.syncKey = pp;
    do_check_true(Service.verifyAndFetchSymmetricKeys());

    // changePassphrase wipes our keys, and they're regenerated on next sync.
    _("Checking changed passphrase.");
    let existingDefault = Service.collectionKeys.keyForCollection();
    let existingKeysPayload = keysWBO.payload;
    let newPassphrase = "bbbbbabcdeabcdeabcdeabcdea";
    Service.changePassphrase(newPassphrase);

    _("Local key cache is full, but different.");
    do_check_true(!!Service.collectionKeys._default);
    do_check_false(Service.collectionKeys._default.equals(existingDefault));

    _("Server has new keys.");
    do_check_true(!!keysWBO.payload);
    do_check_true(!!keysWBO.modified);
    do_check_neq(keysWBO.payload, existingKeysPayload);

    // Try to screw up HMAC calculation.
    // Re-encrypt keys with a new random keybundle, and upload them to the
    // server, just as might happen with a second client.
    _("Attempting to screw up HMAC by re-encrypting keys.");
    let keys = Service.collectionKeys.asWBO();
    let b = new BulkKeyBundle("hmacerror");
    b.generateRandom();
    collections.crypto = keys.modified = 100 + (Date.now()/1000);  // Future modification time.
    keys.encrypt(b);
    keys.upload(Service.resource(Service.cryptoKeysURL));

    do_check_false(Service.verifyAndFetchSymmetricKeys());
    do_check_eq(Service.status.login, LOGIN_FAILED_INVALID_PASSPHRASE);

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
