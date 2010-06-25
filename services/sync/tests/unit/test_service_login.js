Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

function login_handler(request, response) {
  // btoa('johndoe:ilovejane') == am9obmRvZTppbG92ZWphbmU=
  // btoa('janedoe:ilovejohn') == amFuZWRvZTppbG92ZWpvaG4=
  let body;
  let header = request.getHeader("Authorization");
  if (header == "Basic am9obmRvZTppbG92ZWphbmU="
      || header == "Basic amFuZWRvZTppbG92ZWpvaG4=") {
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
    "/1.0/johndoe/info/collections": login_handler,
    "/1.0/janedoe/info/collections": login_handler
  });

  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.clusterURL = "http://localhost:8080/";
    Svc.Prefs.set("autoconnect", false);

    _("Initial state is ok.");
    do_check_eq(Status.service, STATUS_OK);

    _("Try logging in. It wont' work because we're not configured yet.");
    Weave.Service.login();
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);
    do_check_false(Weave.Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Try again with username and password set.");
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";
    Weave.Service.login();
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);
    do_check_false(Weave.Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Success if passphrase is set.");
    Weave.Service.passphrase = "foo";
    Weave.Service.login();
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);
    do_check_true(Weave.Service.isLoggedIn);
    do_check_true(Svc.Prefs.get("autoconnect"));

    _("We can also pass username, password and passphrase to login().");
    Weave.Service.login("janedoe", "incorrectpassword", "bar");
    do_check_eq(Weave.Service.username, "janedoe");
    do_check_eq(Weave.Service.password, "incorrectpassword");
    do_check_eq(Weave.Service.passphrase, "bar");
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);
    do_check_false(Weave.Service.isLoggedIn);

    _("Try again with correct password.");
    Weave.Service.login("janedoe", "ilovejohn");
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);
    do_check_true(Weave.Service.isLoggedIn);
    do_check_true(Svc.Prefs.get("autoconnect"));

    _("Logout.");
    Weave.Service.logout();
    do_check_false(Weave.Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Logging out again won't do any harm.");
    Weave.Service.logout();
    do_check_false(Weave.Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}
