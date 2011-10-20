Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/policies.js");

function login_handling(handler) {
  return function (request, response) {
    if (basic_auth_matches(request, "johndoe", "ilovejane")) {
      handler(request, response);
    } else {
      let body = "Unauthorized";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  do_test_pending();
  let server = httpd_setup({
    "/1.1/johndoe/storage/crypto/keys": upd("crypto", new ServerWBO("keys").handler()),
    "/1.1/johndoe/storage/meta/global": upd("meta",   new ServerWBO("global").handler()),
    "/1.1/johndoe/info/collections":    login_handling(collectionsHelper.handler)
  });

  const GLOBAL_SCORE = 42;

  try {
    _("Set up test fixtures.");
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";
    Weave.Service.passphrase = "foo";
    SyncScheduler.globalScore = GLOBAL_SCORE;
    // Avoid daily ping
    Weave.Svc.Prefs.set("lastPing", Math.floor(Date.now() / 1000));

    let threw = false;
    Weave.Svc.Obs.add("weave:service:sync:error", function (subject, data) {
      threw = true;
    });

    _("Initial state: We're successfully logged in.");
    Weave.Service.login();
    do_check_true(Weave.Service.isLoggedIn);
    do_check_eq(Weave.Status.login, Weave.LOGIN_SUCCEEDED);

    _("Simulate having changed the password somewhere else.");
    Weave.Service.password = "ilovejosephine";

    _("Let's try to sync.");
    Weave.Service.sync();

    _("Verify that sync() threw an exception.");
    do_check_true(threw);

    _("We're no longer logged in.");
    do_check_false(Weave.Service.isLoggedIn);

    _("Sync status won't have changed yet, because we haven't tried again.");

    _("globalScore is reset upon starting a sync.");
    do_check_eq(SyncScheduler.globalScore, 0);

    _("Our next sync will fail appropriately.");
    try {
      Weave.Service.sync();
    } catch (ex) {
    }
    do_check_eq(Weave.Status.login, Weave.LOGIN_FAILED_LOGIN_REJECTED);

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
