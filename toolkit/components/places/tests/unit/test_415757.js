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
  return cc == 1;
}

const TOTAL_SITES = 20;

// main
add_task(async function test_execute() {
  // add pages to global history
  for (let i = 0; i < TOTAL_SITES; i++) {
    let uri = "http://www.test-" + i + ".com/";
    let when = Date.now() * 1000 + i * TOTAL_SITES;
    await PlacesTestUtils.addVisits({ uri, visitDate: when });
  }
  for (let i = 0; i < TOTAL_SITES; i++) {
    let uri = "http://www.test.com/" + i + "/";
    let when = Date.now() * 1000 + i * TOTAL_SITES;
    await PlacesTestUtils.addVisits({ uri, visitDate: when });
  }

  // set a page annotation on one of the urls that will be removed
  var testAnnoDeletedURI = "http://www.test.com/1/";
  var testAnnoDeletedName = "foo";
  var testAnnoDeletedValue = "bar";
  await PlacesUtils.history.update({
    url: testAnnoDeletedURI,
    annotations: new Map([[testAnnoDeletedName, testAnnoDeletedValue]]),
  });

  // set a page annotation on one of the urls that will NOT be removed
  var testAnnoRetainedURI = "http://www.test-1.com/";
  var testAnnoRetainedName = "foo";
  var testAnnoRetainedValue = "bar";
  await PlacesUtils.history.update({
    url: testAnnoRetainedURI,
    annotations: new Map([[testAnnoRetainedName, testAnnoRetainedValue]]),
  });

  // remove pages from www.test.com
  await PlacesUtils.history.removeByFilter({ host: "www.test.com" });

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
  await assertNoOrphanPageAnnotations();

  // check that annotation on the NOT removed item still exists
  let pageInfo = await PlacesUtils.history.fetch(testAnnoRetainedURI, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.get(testAnnoRetainedName),
    testAnnoRetainedValue,
    "Should have kept the annotation for the non-removed items"
  );
});
