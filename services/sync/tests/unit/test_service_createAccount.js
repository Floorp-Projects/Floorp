Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");

function run_test() {
  var requestBody;
  var secretHeader;
  function send(statusCode, status, body) {
    return function(request, response) {
      requestBody = readBytesFromInputStream(request.bodyInputStream);
      if (request.hasHeader("X-Weave-Secret")) {
        secretHeader = request.getHeader("X-Weave-Secret");
      }

      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

  do_test_pending();
  let server = httpd_setup({
    "/user/1.0/johndoe": send(200, "OK", "0"),
    "/user/1.0/janedoe": send(400, "Bad Request", "2"),
    "/user/1.0/jimdoe": send(500, "Server Error", "Server Error")
  });
  try {
    Service.serverURL = "http://localhost:8080/";

    _("Create an account.");
    let res = Service.createAccount("johndoe", "mysecretpw", "john@doe",
                                          "challenge", "response");
    do_check_eq(res, null);
    let payload = JSON.parse(requestBody);
    do_check_eq(payload.password, "mysecretpw");
    do_check_eq(payload.email, "john@doe");
    do_check_eq(payload["captcha-challenge"], "challenge");
    do_check_eq(payload["captcha-response"], "response");

    _("A non-ASCII password is UTF-8 encoded.");
    res = Service.createAccount("johndoe", "moneyislike$\u20ac\xa5\u5143",
                                      "john@doe", "challenge", "response");
    do_check_eq(res, null);
    payload = JSON.parse(requestBody);
    do_check_eq(payload.password,
                Utils.encodeUTF8("moneyislike$\u20ac\xa5\u5143"));

    _("Invalid captcha or other user-friendly error.");
    res = Service.createAccount("janedoe", "anothersecretpw", "jane@doe",
                                      "challenge", "response");
    do_check_eq(res, "invalid-captcha");

    _("Generic server error.");
    res = Service.createAccount("jimdoe", "preciousss", "jim@doe",
                                      "challenge", "response");
    do_check_eq(res, "generic-server-error");

    _("Admin secret preference is passed as HTTP header token.");
    Svc.Prefs.set("admin-secret", "my-server-secret");
    res = Service.createAccount("johndoe", "mysecretpw", "john@doe",
                                      "challenge", "response");
    do_check_eq(secretHeader, "my-server-secret");

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
