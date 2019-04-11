/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search purpose can be specified and that query parameters for
 * that purpose are included in the search URL.
 */

"use strict";

add_task(async function setup() {
  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams used to set the purpose will be ignored.
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-extensions",
                          Services.io.newURI(url));

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_purpose() {
  let engine = Services.search.getEngineByName("Test search engine");

  function check_submission(aValue, aSearchTerm, aType, aPurpose) {
    let submissionURL = engine.getSubmission(aSearchTerm, aType, aPurpose).uri.spec;
    let searchParams = new URLSearchParams(submissionURL.split("?")[1]);
    if (aValue) {
      Assert.equal(searchParams.get("channel"), aValue);
    } else {
      Assert.ok(!searchParams.has("channel"));
    }
    Assert.equal(searchParams.get("q"), aSearchTerm);
  }

  check_submission("", "foo");
  check_submission("", "foo", null);
  check_submission("", "foo", "text/html");
  check_submission("rcs", "foo", null, "contextmenu");
  check_submission("rcs", "foo", "text/html", "contextmenu");
  check_submission("fflb", "foo", null, "keyword");
  check_submission("fflb", "foo", "text/html", "keyword");
  check_submission("", "foo", "text/html", "invalid");

  // Tests for a purpose on the search form (ie. empty query).
  engine = Services.search.getEngineByName("engine-rel-searchform-purpose");

  // See bug 1485508
  Assert.ok(!engine.searchForm.includes("?&"));

  // verify that the 'system' purpose falls back to the 'searchbar' purpose.
  check_submission("sb", "foo", "text/html", "system");
  check_submission("sb", "foo", "text/html", "searchbar");
});

add_task(async function test_purpose() {
  let engine = Services.search.getEngineByName("Test search engine (Reordered)");

  function check_submission(aValue, aSearchTerm, aType, aPurpose) {
    let submissionURL = engine.getSubmission(aSearchTerm, aType, aPurpose).uri.spec;
    let searchParams = new URLSearchParams(submissionURL.split("?")[1]);
    if (aValue) {
      Assert.equal(searchParams.get("channel"), aValue);
    } else {
      Assert.ok(!searchParams.has("channel"));
    }
    Assert.equal(searchParams.get("q"), aSearchTerm);
  }

  check_submission("", "foo");
  check_submission("", "foo", null);
  check_submission("", "foo", "text/html");
  check_submission("rcs", "foo", null, "contextmenu");
  check_submission("rcs", "foo", "text/html", "contextmenu");
  check_submission("fflb", "foo", null, "keyword");
  check_submission("fflb", "foo", "text/html", "keyword");
  check_submission("", "foo", "text/html", "invalid");

  // See bug 1485508
  Assert.ok(!engine.searchForm.includes("?&"));
});
