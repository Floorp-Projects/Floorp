Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

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

function service_unavailable(request, response) {
  let body = "Service Unavailable";
  response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
  response.setHeader("Retry-After", "42");
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  // This test expects a clean slate -- no saved passphrase.
  Weave.Svc.Login.removeAllLogins();
  
  do_test_pending();
  let server = httpd_setup({
    "/api/1.0/johndoe/info/collections": login_handler,
    "/api/1.0/janedoe/info/collections": service_unavailable,
    "/api/1.0/johndoe/storage/meta/global": new ServerWBO().handler(),
    "/api/1.0/johndoe/storage/crypto/keys": new ServerWBO().handler(),
    "/user/1.0/johndoe/node/weave": httpd_handler(200, "OK", "http://localhost:8080/api/")
  });

  try {
    Service.serverURL = "http://localhost:8080/";

    _("Force the initial state.");
    Status.service = STATUS_OK;
    do_check_eq(Status.service, STATUS_OK);

    _("Credentials won't check out because we're not configured yet.");
    Status.resetSync();
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);

    _("Try again with username and password set.");
    Status.resetSync();
    Service.username = "johndoe";
    Service.password = "ilovejane";
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);

    _("verifyLogin() has found out the user's cluster URL, though.");
    do_check_eq(Service.clusterURL, "http://localhost:8080/api/");

    _("Success if passphrase is set.");
    Status.resetSync();
    Service.passphrase = "foo";
    do_check_true(Service.verifyLogin());
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);

    _("If verifyLogin() encounters a server error, it flips on the backoff flag and notifies observers on a 503 with Retry-After.");
    Status.resetSync();
    Service.username = "janedoe";
    do_check_false(Status.enforceBackoff);
    let backoffInterval;    
    Svc.Obs.add("weave:service:backoff:interval", function(subject, data) {
      backoffInterval = subject;
    });
    do_check_false(Service.verifyLogin());
    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, 42);
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, LOGIN_FAILED_SERVER_ERROR);

    _("Ensure a network error when finding the cluster sets the right Status bits.");
    Status.resetSync();
    Service.serverURL = "http://localhost:12345/";
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

    _("Ensure a network error when getting the collection info sets the right Status bits.");
    Status.resetSync();
    Service.clusterURL = "http://localhost:12345/";
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
