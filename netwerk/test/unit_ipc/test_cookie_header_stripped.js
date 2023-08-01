"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const TEST_DOMAIN = "www.example.com";
ChromeUtils.defineLazyGetter(this, "URL", function () {
  return (
    "http://" + TEST_DOMAIN + ":" + httpserv.identity.primaryPort + "/path"
  );
});

const responseBody1 = "response";
function requestHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Set-Cookie", "tom=cool; Max-Age=10", true);
  response.bodyOutputStream.write(responseBody1, responseBody1.length);
}

let httpserv = null;

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/path", requestHandler);
  httpserv.start(-1);
  httpserv.identity.add("http", TEST_DOMAIN, httpserv.identity.primaryPort);

  registerCleanupFunction(() => {
    Services.cookies.removeCookiesWithOriginAttributes("{}", TEST_DOMAIN);
    Services.prefs.clearUserPref("network.dns.localDomains");
    Services.prefs.clearUserPref("network.cookie.cookieBehavior");
    Services.prefs.clearUserPref(
      "network.cookieJarSettings.unblocked_for_testing"
    );

    httpserv.stop();
    httpserv = null;
  });

  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setCharPref("network.dns.localDomains", TEST_DOMAIN);
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.cookies.removeCookiesWithOriginAttributes("{}", TEST_DOMAIN);

  // Sends back the URL to the child script
  do_await_remote_message("start-test").then(() => {
    do_send_remote_message("start-test-done", URL);
  });

  // Sends back the cookie count for the domain
  // Should only be one - from Set-Cookie
  do_await_remote_message("check-cookie-count").then(() => {
    do_send_remote_message(
      "check-cookie-count-done",
      Services.cookies.countCookiesFromHost(TEST_DOMAIN)
    );
  });

  // Sends back the cookie count for the domain
  // There should be 2 cookies. One from the Set-Cookie header, the other set
  // manually.
  do_await_remote_message("second-check-cookie-count").then(() => {
    do_send_remote_message(
      "second-check-cookie-count-done",
      Services.cookies.countCookiesFromHost(TEST_DOMAIN)
    );
  });

  // Sets a cookie for the test domain
  do_await_remote_message("set-cookie").then(() => {
    const expiry = Date.now() + 24 * 60 * 60;
    Services.cookies.add(
      TEST_DOMAIN,
      "/",
      "cookieName",
      "cookieValue",
      false,
      false,
      false,
      expiry,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_HTTPS
    );
    do_send_remote_message("set-cookie-done");
  });

  // Run the actual test logic
  run_test_in_child("child_cookie_header.js");
}
