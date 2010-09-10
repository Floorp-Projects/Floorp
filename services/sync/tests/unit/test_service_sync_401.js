Cu.import("resource://services-sync/main.js");

function login_handler(request, response) {
  // btoa('johndoe:ilovejane') == am9obmRvZTppbG92ZWphbmU=
  let body;
  if (request.hasHeader("Authorization") &&
      request.getHeader("Authorization") == "Basic am9obmRvZTppbG92ZWphbmU=") {
    body = "{}";
    response.setStatusLine(request.httpVersion, 200, "OK");
  } else {
    body = "Unauthorized";
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
  }
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  do_test_pending();
  let server = httpd_setup({
    "/1.0/johndoe/info/collections": login_handler
  });

  const GLOBAL_SCORE = 42;

  try {
    _("Set up test fixtures.");
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";
    Weave.Service.passphrase = "foo";
    Weave.Service.globalScore = GLOBAL_SCORE;
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

    _("Simulate having changed the password somehwere else.");
    Weave.Service.password = "ilovejosephine";

    _("Let's try to sync.");
    Weave.Service.sync();

    _("Verify that sync() threw an exception.");
    do_check_true(threw);

    _("We're no longer logged in.");
    do_check_false(Weave.Service.isLoggedIn);

    _("Sync status.");
    do_check_eq(Weave.Status.login, Weave.LOGIN_FAILED_LOGIN_REJECTED);

    _("globalScore is unchanged.");
    do_check_eq(Weave.Service.globalScore, GLOBAL_SCORE);

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
