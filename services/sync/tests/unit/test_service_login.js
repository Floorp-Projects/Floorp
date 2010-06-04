Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/service.js");
Cu.import("resource://weave/status.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

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

  let server = httpd_setup({
    "/1.0/johndoe/info/collections": login_handler
  });

  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";

    _("Try logging in. It wont' work because we're not configured yet.");
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    Weave.Service.login();
    do_check_false(Weave.Service.isLoggedIn);

    _("Try again with username and password set.");
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";
    // We need a non-empty passphrase for login to work.  Even the
    // setup wizard just sets this to 'foo' if there isn't a
    // passphrase yet.
    Weave.Service.passphrase = "foo";
    Weave.Service.login();
    do_check_true(Weave.Service.isLoggedIn);

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}
