/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = PlacesUtils.history.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = PlacesUtils.history.getNewQuery();
  query.uri = aURI;
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  return (cc == 1);
}

const TOTAL_SITES = 20;

// main
add_task(async function test_execute() {
  // add pages to global history
  for (let i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test-" + i + ".com/";
    let testURI = uri(site);
    let when = Date.now() * 1000 + (i * TOTAL_SITES);
    await PlacesTestUtils.addVisits({ uri: testURI, visitDate: when });
  }
  for (let i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test.com/" + i + "/";
    let testURI = uri(site);
    let when = Date.now() * 1000 + (i * TOTAL_SITES);
    await PlacesTestUtils.addVisits({ uri: testURI, visitDate: when });
  }

  // set a page annotation on one of the urls that will be removed
  var testAnnoDeletedURI = uri("http://www.test.com/1/");
  var testAnnoDeletedName = "foo";
  var testAnnoDeletedValue = "bar";
  PlacesUtils.annotations.setPageAnnotation(testAnnoDeletedURI,
                                            testAnnoDeletedName,
                                            testAnnoDeletedValue, 0,
                                            PlacesUtils.annotations.EXPIRE_WITH_HISTORY);

  // set a page annotation on one of the urls that will NOT be removed
  var testAnnoRetainedURI = uri("http://www.test-1.com/");
  var testAnnoRetainedName = "foo";
  var testAnnoRetainedValue = "bar";
  PlacesUtils.annotations.setPageAnnotation(testAnnoRetainedURI,
                                            testAnnoRetainedName,
                                            testAnnoRetainedValue, 0,
                                            PlacesUtils.annotations.EXPIRE_WITH_HISTORY);

  // remove pages from www.test.com
  PlacesUtils.history.removePagesFromHost("www.test.com", false);

  // check that all pages in www.test.com have been removed
  for (let i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test.com/" + i + "/";
    let testURI = uri(site);
    Assert.ok(!uri_in_db(testURI));
  }

  // check that all pages in www.test-X.com have NOT been removed
  for (let i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test-" + i + ".com/";
    let testURI = uri(site);
    Assert.ok(uri_in_db(testURI));
  }

  // check that annotation on the removed item does not exists
  try {
    PlacesUtils.annotations.getPageAnnotation(testAnnoDeletedURI, testAnnoDeletedName);
    do_throw("fetching page-annotation that doesn't exist, should've thrown");
  } catch (ex) {}

  // check that annotation on the NOT removed item still exists
  try {
    var annoVal = PlacesUtils.annotations.getPageAnnotation(testAnnoRetainedURI,
                                                            testAnnoRetainedName);
  } catch (ex) {
    do_throw("The annotation has been removed erroneously");
  }
  Assert.equal(annoVal, testAnnoRetainedValue);

});
