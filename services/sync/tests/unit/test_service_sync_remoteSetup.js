Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/log4moz.js");

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let guidSvc = new FakeGUIDService();
  let cryptoSvc = new FakeCryptoService();

  let keys = new ServerCollection({privkey: new ServerWBO('privkey'),
                                   pubkey: new ServerWBO('pubkey')});
  let crypto = new ServerCollection({keys: new ServerWBO('keys'),
                                     clients: new ServerWBO('clients')});
  let clients = new ServerCollection();
  let meta_global = new ServerWBO('global');

  let collections = {};
  function info_collections(request, response) {
    let body = JSON.stringify(collections);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }

  function wasCalledHandler(wbo) {
    let handler = wbo.handler();
    return function() {
      wbo.wasCalled = true;
      handler.apply(this, arguments);
    };
  }

  do_test_pending();
  let server = httpd_setup({
    "/1.0/johndoe/storage/keys": keys.handler(),
    "/1.0/johndoe/storage/keys/pubkey": wasCalledHandler(keys.wbos.pubkey),
    "/1.0/johndoe/storage/keys/privkey": wasCalledHandler(keys.wbos.privkey),

    "/1.0/johndoe/storage/crypto": crypto.handler(),
    "/1.0/johndoe/storage/crypto/keys": crypto.wbos.keys.handler(),
    "/1.0/johndoe/storage/crypto/clients": crypto.wbos.clients.handler(),

    "/1.0/johndoe/storage/clients": clients.handler(),
    "/1.0/johndoe/storage/meta/global": wasCalledHandler(meta_global),
    "/1.0/johndoe/info/collections": info_collections
  });

  try {
    _("Log in.");
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    Weave.Service.login("johndoe", "ilovejane", "foo");
    do_check_true(Weave.Service.isLoggedIn);

    _("Do an initial sync.");
    let beforeSync = Date.now()/1000;
    Weave.Service.sync();

    _("Verify that the meta record and keys was uploaded.");
    do_check_eq(meta_global.data.syncID, Weave.Service.syncID);
    do_check_eq(meta_global.data.storageVersion, Weave.STORAGE_VERSION);
    do_check_eq(meta_global.data.engines.clients.version, Weave.Clients.version);
    do_check_eq(meta_global.data.engines.clients.syncID, Weave.Clients.syncID);
    do_check_true(!!keys.wbos.privkey.payload);
    do_check_true(!!keys.wbos.pubkey.payload);

    _("Set the collection info hash so that sync() will remember the modified times for future runs.");
    collections = {meta: Weave.Clients.lastSync,
                   keys: Weave.Clients.lastSync,
                   clients: Weave.Clients.lastSync,
                   crypto: Weave.Clients.lastSync};
    Weave.Service.sync();

    _("Sync again and verify that meta/global and keys weren't downloaded again");
    meta_global.wasCalled = false;
    keys.wbos.pubkey.wasCalled = false;
    keys.wbos.privkey.wasCalled = false;
    Weave.Service.sync();
    do_check_false(meta_global.wasCalled);
    do_check_false(keys.wbos.pubkey.wasCalled);
    do_check_false(keys.wbos.privkey.wasCalled);

    _("Fake modified records. This will cause a redownload, but not reupload since it hasn't changed.");
    collections.meta += 42;
    collections.keys += 23;
    meta_global.wasCalled = false;
    keys.wbos.pubkey.wasCalled = false;
    keys.wbos.privkey.wasCalled = false;

    let metaModified = meta_global.modified;
    let pubkeyModified = keys.wbos.pubkey.modified;
    let privkeyModified = keys.wbos.privkey.modified;

    Weave.Service.sync();
    do_check_true(meta_global.wasCalled);
    do_check_true(keys.wbos.privkey.wasCalled);
    do_check_true(keys.wbos.pubkey.wasCalled);
    do_check_eq(metaModified, meta_global.modified);
    do_check_eq(privkeyModified, keys.wbos.privkey.modified);
    do_check_eq(pubkeyModified, keys.wbos.pubkey.modified);

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
