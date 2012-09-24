/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  initTestLogging("Trace");

  let requestBody;
  let secretHeader;
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
    // john@doe.com
    "/user/1.0/7wohs32cngzuqt466q3ge7indszva4of": send(200, "OK", "0"),
    // jane@doe.com
    "/user/1.0/vuuf3eqgloxpxmzph27f5a6ve7gzlrms": send(400, "Bad Request", "2"),
    // jim@doe.com
    "/user/1.0/vz6fhecgw5t3sgx3a4cektoiokyczkqd": send(500, "Server Error", "Server Error")
  });
  try {
    Service.serverURL = TEST_SERVER_URL;

    _("Create an account.");
    let res = Service.createAccount("john@doe.com", "mysecretpw",
                                    "challenge", "response");
    do_check_eq(res, null);
    let payload = JSON.parse(requestBody);
    do_check_eq(payload.password, "mysecretpw");
    do_check_eq(payload.email, "john@doe.com");
    do_check_eq(payload["captcha-challenge"], "challenge");
    do_check_eq(payload["captcha-response"], "response");

    _("A non-ASCII password is UTF-8 encoded.");
    const moneyPassword = "moneyislike$£¥";
    res = Service.createAccount("john@doe.com", moneyPassword,
                                "challenge", "response");
    do_check_eq(res, null);
    payload = JSON.parse(requestBody);
    do_check_eq(payload.password, Utils.encodeUTF8(moneyPassword));

    _("Invalid captcha or other user-friendly error.");
    res = Service.createAccount("jane@doe.com", "anothersecretpw",
                                "challenge", "response");
    do_check_eq(res, "invalid-captcha");

    _("Generic server error.");
    res = Service.createAccount("jim@doe.com", "preciousss",
                                "challenge", "response");
    do_check_eq(res, "generic-server-error");

    _("Admin secret preference is passed as HTTP header token.");
    Svc.Prefs.set("admin-secret", "my-server-secret");
    res = Service.createAccount("john@doe.com", "mysecretpw",
                                "challenge", "response");
    do_check_eq(secretHeader, "my-server-secret");

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
