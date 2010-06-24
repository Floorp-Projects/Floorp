Cu.import("resource://services-sync/service.js");

function send(statusCode, status, body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, statusCode, status);
    response.bodyOutputStream.write(body, body.length);
  };
}

function run_test() {
  let server = httpd_setup({
    "/user/1.0/johndoe": send(200, "OK", "1"),
    "/user/1.0/janedoe": send(200, "OK", "0")
  });
  try {
    Weave.Service.serverURL = "http://localhost:8080/";

    _("A 404 will be recorded as 'generic-server-error'");
    do_check_eq(Weave.Service.checkUsername("jimdoe"), "generic-server-error");

    _("Username that's not available.");
    do_check_eq(Weave.Service.checkUsername("johndoe"), "notAvailable");

    _("Username that's available.");
    do_check_eq(Weave.Service.checkUsername("janedoe"), "available");

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}
