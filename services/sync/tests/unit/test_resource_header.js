/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-sync/resource.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

const TEST_URL = "http://localhost:4444/content";
const HTTP_PORT = 4444;
const BODY = "response body";

// Keep headers for later inspection.
let auth = null;
let foo  = null;
function contentHandler(metadata, response) {
  _("Handling request.");
  auth = metadata.getHeader("Authorization");
  foo  = metadata.getHeader("X-Foo");

  _("Extracted headers. " + auth + ", " + foo);

  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(BODY, BODY.length);
}

function makeServer() {
  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(4444);
  return httpServer;
}

// Set a proxy function to cause an internal redirect.
function triggerRedirect() {
  const PROXY_FUNCTION = "function FindProxyForURL(url, host) {"                +
                         "  return 'PROXY a_non_existent_domain_x7x6c572v:80; " +
                                   "PROXY localhost:4444';"                     +
                         "}";

  let prefsService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
  let prefs = prefsService.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
  prefs.setCharPref("autoconfig_url", "data:text/plain," + PROXY_FUNCTION);
}

add_test(function test_headers_copied() {
  let server = makeServer();
  triggerRedirect();

  _("Issuing request.");
  let resource = new Resource(TEST_URL);
  resource.setHeader("Authorization", "Basic foobar");
  resource.setHeader("X-Foo", "foofoo");

  let result = resource.get(TEST_URL);
  _("Result: " + result);

  do_check_eq(result, BODY);
  do_check_eq(auth, "Basic foobar");
  do_check_eq(foo, "foofoo");

  server.stop(run_next_test);
});
