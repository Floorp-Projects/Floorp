Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function send(statusCode, status, body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, statusCode, status);
    response.bodyOutputStream.write(body, body.length);
  };
}

function run_test() {
  do_test_pending();
  let server = httpd_setup({
    "/user/1.0/johndoe": send(200, "OK", "1"),
    "/user/1.0/janedoe": send(200, "OK", "0"),
    // john@doe.com
    "/user/1.0/7wohs32cngzuqt466q3ge7indszva4of": send(200, "OK", "0"),
    // jane@doe.com
    "/user/1.0/vuuf3eqgloxpxmzph27f5a6ve7gzlrms": send(200, "OK", "1")
  });
  try {
    Service.serverURL = "http://localhost:8080/";

    _("A 404 will be recorded as 'generic-server-error'");
    do_check_eq(Service.checkUsername("jimdoe"), "generic-server-error");

    _("Account that's available.");
    do_check_eq(Service.checkAccount("john@doe.com"), "available");

    _("Account that's not available.");
    do_check_eq(Service.checkAccount("jane@doe.com"), "notAvailable");

    // Backwards compat with the Firefox UI. Remove once bug 595066 has landed.

    _("Account that's not available.");
    do_check_eq(Service.checkUsername("johndoe"), "notAvailable");

    _("Account that's available.");
    do_check_eq(Service.checkUsername("janedoe"), "available");

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
