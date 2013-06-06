/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search purpose can be specified and that query parameters for
 * that purpose are included in the search URL.
 */

"use strict";

const Ci = Components.interfaces;

Components.utils.import("resource://testing-common/httpd.js");

function search_observer(aSubject, aTopic, aData) {
  let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
  do_print("Observer: " + aData + " for " + engine.name);

  if (aData != "engine-added")
    return;

  if (engine.name != "Test search engine")
    return;

  function check_submission(aExpected, aSearchTerm, aType, aPurpose) {
    do_check_eq(engine.getSubmission(aSearchTerm, aType, aPurpose).uri.spec,
                base + aExpected);
  }

  let base = "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t&client=firefox";
  check_submission("",              "foo");
  check_submission("",              "foo", null);
  check_submission("",              "foo", "text/html");
  check_submission("&channel=rcs",  "foo", null,        "contextmenu");
  check_submission("&channel=rcs",  "foo", "text/html", "contextmenu");
  check_submission("&channel=fflb", "foo", null,        "keyword");
  check_submission("&channel=fflb", "foo", "text/html", "keyword");
  check_submission("",              "foo", "text/html", "invalid");

  // Tests for a param that varies with a purpose but has a default value.
  base = "http://www.google.com/search?q=foo&client=firefox";
  check_submission("&channel=none", "foo", "application/x-moz-default-purpose");
  check_submission("&channel=none", "foo", "application/x-moz-default-purpose", null);
  check_submission("&channel=none", "foo", "application/x-moz-default-purpose", "");
  check_submission("&channel=rcs",  "foo", "application/x-moz-default-purpose", "contextmenu");
  check_submission("&channel=fflb", "foo", "application/x-moz-default-purpose", "keyword");
  check_submission("",              "foo", "application/x-moz-default-purpose", "invalid");

  do_test_finished();
};

function run_test() {
  removeMetadata();
  updateAppInfo();
  do_load_manifest("data/chrome.manifest");

  let httpServer = new HttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  do_test_pending();
  Services.obs.addObserver(search_observer, "browser-search-engine-modified", false);

  Services.search.addEngine("http://localhost:4444/data/engine.xml",
                            Ci.nsISearchEngine.DATA_XML,
                            null, false);
}
