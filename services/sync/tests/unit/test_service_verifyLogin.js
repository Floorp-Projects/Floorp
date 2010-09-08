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

function send(statusCode, status, body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, statusCode, status);
    response.bodyOutputStream.write(body, body.length);
  };
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

  do_test_pending();
  let server = httpd_setup({
    "/api/1.0/johndoe/info/collections": login_handler,
    "/api/1.0/janedoe/info/collections": service_unavailable,
    "/user/1.0/johndoe/node/weave": send(200, "OK", "http://localhost:8080/api/")
  });

  try {
    Service.serverURL = "http://localhost:8080/";

    _("Force the initial state.");
    Status.service = STATUS_OK;
    do_check_eq(Status.service, STATUS_OK);

    _("Credentials won't check out because we're not configured yet.");
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);

    _("Try again with username and password set.");
    Service.username = "johndoe";
    Service.password = "ilovejane";
    do_check_false(Service.verifyLogin());
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);

    _("verifyLogin() has found out the user's cluster URL, though.");
    do_check_eq(Service.clusterURL, "http://localhost:8080/api/");

    _("Success if passphrase is set.");
    Service.passphrase = "foo";
    do_check_true(Service.verifyLogin());
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);

    _("If verifyLogin() encounters a server error, it flips on the backoff flag and notifies observers on a 503 with Retry-After.");
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

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
