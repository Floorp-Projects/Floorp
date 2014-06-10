/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.AsyncResource").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Resource").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;

  ensureLegacyIdentityManager();

  run_next_test();
}

add_test(function test_change_password() {
  let requestBody;
  let server;

  function send(statusCode, status, body) {
    return function(request, response) {
      requestBody = readBytesFromInputStream(request.bodyInputStream);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

  try {
    Service.baseURI = "http://localhost:9999/";
    Service.serverURL = "http://localhost:9999/";
    setBasicCredentials("johndoe", "ilovejane");

    _("changePassword() returns false for a network error, the password won't change.");
    let res = Service.changePassword("ILoveJane83");
    do_check_false(res);
    do_check_eq(Service.identity.basicPassword, "ilovejane");

    _("Let's fire up the server and actually change the password.");
    server = httpd_setup({
      "/user/1.0/johndoe/password": send(200, "OK", ""),
      "/user/1.0/janedoe/password": send(401, "Unauthorized", "Forbidden!")
    });

    Service.serverURL = server.baseURI;
    res = Service.changePassword("ILoveJane83");
    do_check_true(res);
    do_check_eq(Service.identity.basicPassword, "ILoveJane83");
    do_check_eq(requestBody, "ILoveJane83");

    _("Make sure the password has been persisted in the login manager.");
    let logins = Services.logins.findLogins({}, PWDMGR_HOST, null,
                                            PWDMGR_PASSWORD_REALM);
    do_check_eq(logins.length, 1);
    do_check_eq(logins[0].password, "ILoveJane83");

    _("A non-ASCII password is UTF-8 encoded.");
    const moneyPassword = "moneyislike$£¥";
    res = Service.changePassword(moneyPassword);
    do_check_true(res);
    do_check_eq(Service.identity.basicPassword, Utils.encodeUTF8(moneyPassword));
    do_check_eq(requestBody, Utils.encodeUTF8(moneyPassword));

    _("changePassword() returns false for a server error, the password won't change.");
    Services.logins.removeAllLogins();
    setBasicCredentials("janedoe", "ilovejohn");
    res = Service.changePassword("ILoveJohn86");
    do_check_false(res);
    do_check_eq(Service.identity.basicPassword, "ilovejohn");

  } finally {
    Svc.Prefs.resetBranch("");
    Services.logins.removeAllLogins();
    server.stop(run_next_test);
  }
});
