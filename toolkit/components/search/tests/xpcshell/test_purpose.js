/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search purpose can be specified and that query parameters for
 * that purpose are included in the search URL.
 */

"use strict";

function run_test() {
  removeMetadata();
  updateAppInfo();

  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams used to set the purpose will be ignored.
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url));

  run_next_test();
}

add_task(function* test_purpose() {
  let engine = Services.search.getEngineByName("Test search engine");

  function check_submission(aExpected, aSearchTerm, aType, aPurpose) {
    do_check_eq(engine.getSubmission(aSearchTerm, aType, aPurpose).uri.spec,
                base + aExpected);
  }

  let base = "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t";
  check_submission("", "foo");
  check_submission("", "foo", null);
  check_submission("", "foo", "text/html");
  check_submission("&channel=rcs", "foo", null, "contextmenu");
  check_submission("&channel=rcs", "foo", "text/html", "contextmenu");
  check_submission("&channel=fflb", "foo", null, "keyword");
  check_submission("&channel=fflb", "foo", "text/html", "keyword");
  check_submission("", "foo", "text/html", "invalid");

  // Tests for a param that varies with a purpose but has a default value.
  base = "http://www.google.com/search?q=foo";
  check_submission("&channel=ffsb", "foo", "application/x-moz-default-purpose");
  check_submission("&channel=ffsb", "foo", "application/x-moz-default-purpose", null);
  check_submission("&channel=ffsb", "foo", "application/x-moz-default-purpose", "");
  check_submission("&channel=rcs", "foo", "application/x-moz-default-purpose", "contextmenu");
  check_submission("&channel=fflb", "foo", "application/x-moz-default-purpose", "keyword");
  check_submission("&channel=ffsb", "foo", "application/x-moz-default-purpose", "searchbar");
  check_submission("&channel=ffsb", "foo", "application/x-moz-default-purpose", "invalid");

  // Tests for a purpose on the search form (ie. empty query).
  engine = Services.search.getEngineByName("engine-rel-searchform-purpose");
  base = "http://www.google.com/search?q=";
  check_submission("&channel=sb", "", null, "searchbar");
  check_submission("&channel=sb", "", "text/html", "searchbar");

  // verify that the 'system' purpose falls back to the 'searchbar' purpose.
  base = "http://www.google.com/search?q=foo";
  check_submission("&channel=sb", "foo", "text/html", "system");
  check_submission("&channel=sb", "foo", "text/html", "searchbar");
  // Use an engine that actually defines the 'system' purpose...
  engine = Services.search.getEngineByName("engine-system-purpose");
  // ... and check that the system purpose is used correctly.
  check_submission("&channel=sys", "foo", "text/html", "system");

  do_test_finished();
});
