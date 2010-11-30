Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/base_records/wbo.js");      // For Records.
Cu.import("resource://services-sync/base_records/crypto.js");   // For CollectionKeys.
Cu.import("resource://services-sync/log4moz.js");
  
function run_test() {
  let passphrase = "abcdeabcdeabcdeabcdeabcdea";
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let clients = new ServerCollection();
  let meta_global = new ServerWBO('global');

  let collections = {};
  function info_collections(request, response) {
    let body = JSON.stringify(collections);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }

  do_test_pending();
  let server = httpd_setup({
    "/1.0/johndoe/storage/crypto/keys": new ServerWBO().handler(),
    "/1.0/johndoe/storage/clients": clients.handler(),
    "/1.0/johndoe/storage/meta/global": meta_global.handler(),
    "/1.0/johndoe/info/collections": info_collections
  });

  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    
    Weave.Service.login("johndoe", "ilovejane", passphrase);
    do_check_true(Weave.Service.isLoggedIn);
    Weave.Service.verifyAndFetchSymmetricKeys();
    do_check_true(Weave.Service._remoteSetup());

    function test_out_of_date() {
      _("meta_global: " + JSON.stringify(meta_global));
      meta_global.payload = {"syncID": "foooooooooooooooooooooooooo",
                             "storageVersion": STORAGE_VERSION + 1};
      _("meta_global: " + JSON.stringify(meta_global));
      Records.set(Weave.Service.metaURL, meta_global);
      try {
        Weave.Service.sync();
      }
      catch (ex) {
      }
      do_check_eq(Status.sync, VERSION_OUT_OF_DATE);
    }
    
    // See what happens when we bump the storage version.
    _("Syncing after server has been upgraded.");
    test_out_of_date();
    
    // Same should happen after a wipe.
    _("Syncing after server has been upgraded and wiped.");
    Weave.Service.wipeServer();
    test_out_of_date();

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
