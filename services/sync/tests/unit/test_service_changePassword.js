Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/constants.js");

function run_test() {
  var requestBody;
  function send(statusCode, status, body) {
    return function(request, response) {
      requestBody = readBytesFromInputStream(request.bodyInputStream);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

  let server;

  try {

    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";

    _("changePassword() returns false for a network error, the password won't change.");
    let res = Weave.Service.changePassword("ILoveJane83");
    do_check_false(res);
    do_check_eq(Weave.Service.password, "ilovejane");

    _("Let's fire up the server and actually change the password.");
    server  = httpd_setup({
      "/user/1.0/johndoe/password": send(200, "OK", ""),
      "/user/1.0/janedoe/password": send(401, "Unauthorized", "Forbidden!")
    });

    res = Weave.Service.changePassword("ILoveJane83");
    do_check_true(res);
    do_check_eq(Weave.Service.password, "ILoveJane83");

    _("Make sure the password has been persisted in the login manager.");
    let logins = Weave.Svc.Login.findLogins({}, PWDMGR_HOST, null,
                                            PWDMGR_PASSWORD_REALM);
    do_check_eq(logins[0].password, "ILoveJane83");

    _("changePassword() returns false for a server error, the password won't change.");
    Weave.Svc.Login.removeAllLogins();
    Weave.Service.username = "janedoe";
    Weave.Service.password = "ilovejohn";
    res = Weave.Service.changePassword("ILoveJohn86");
    do_check_false(res);
    do_check_eq(Weave.Service.password, "ilovejohn");

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    Weave.Svc.Login.removeAllLogins();
    if (server) {
      server.stop(function() {});
    }
  }
}
