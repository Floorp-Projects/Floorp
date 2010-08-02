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

  let server = httpd_setup({
    "/user/1.0/johndoe": send(200, "OK", "0"),
    "/user/1.0/janedoe": send(400, "Bad Request", "2"),
    "/user/1.0/jimdoe": send(500, "Server Error", "Server Error")
  });
  try {
    Weave.Service.serverURL = "http://localhost:8080/";

    _("Create an account.");
    let res = Weave.Service.createAccount("johndoe", "mysecretpw", "john@doe",
                                          "challenge", "response");
    do_check_eq(res, null);
    let payload = JSON.parse(requestBody);
    do_check_eq(payload.password, "mysecretpw");
    do_check_eq(payload.email, "john@doe");
    do_check_eq(payload["captcha-challenge"], "challenge");
    do_check_eq(payload["captcha-response"], "response");

    _("A non-ASCII password is UTF-8 encoded.");
    res = Weave.Service.createAccount("johndoe", "moneyislike$\u20ac\xa5\u5143",
                                      "john@doe", "challenge", "response");
    do_check_eq(res, null);
    payload = JSON.parse(requestBody);
    do_check_eq(payload.password,
                Utils.encodeUTF8("moneyislike$\u20ac\xa5\u5143"));

    _("Invalid captcha or other user-friendly error.");
    res = Weave.Service.createAccount("janedoe", "anothersecretpw", "jane@doe",
                                      "challenge", "response");
    do_check_eq(res, "invalid-captcha");

    _("Generic server error.");
    res = Weave.Service.createAccount("jimdoe", "preciousss", "jim@doe",
                                      "challenge", "response");
    do_check_eq(res, "generic-server-error");

    _("Admin secret preference is passed as HTTP header token.");
    Weave.Svc.Prefs.set("admin-secret", "my-server-secret");
    res = Weave.Service.createAccount("johndoe", "mysecretpw", "john@doe",
                                      "challenge", "response");
    do_check_eq(secretHeader, "my-server-secret");

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}
