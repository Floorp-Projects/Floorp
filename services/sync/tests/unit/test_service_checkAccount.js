/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  do_test_pending();
  let server = httpd_setup({
    "/user/1.0/johndoe": httpd_handler(200, "OK", "1"),
    "/user/1.0/janedoe": httpd_handler(200, "OK", "0"),
    // john@doe.com
    "/user/1.0/7wohs32cngzuqt466q3ge7indszva4of": httpd_handler(200, "OK", "0"),
    // jane@doe.com
    "/user/1.0/vuuf3eqgloxpxmzph27f5a6ve7gzlrms": httpd_handler(200, "OK", "1")
  });
  try {
    Service.serverURL = TEST_SERVER_URL;

    _("A 404 will be recorded as 'generic-server-error'");
    do_check_eq(Service.checkAccount("jimdoe"), "generic-server-error");

    _("Account that's available.");
    do_check_eq(Service.checkAccount("john@doe.com"), "available");

    _("Account that's not available.");
    do_check_eq(Service.checkAccount("jane@doe.com"), "notAvailable");

    _("Username fallback: Account that's not available.");
    do_check_eq(Service.checkAccount("johndoe"), "notAvailable");

    _("Username fallback: Account that's available.");
    do_check_eq(Service.checkAccount("janedoe"), "available");

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
