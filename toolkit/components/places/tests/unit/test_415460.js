/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
  Ci.nsINavHistoryService
);

/**
 * Checks to see that a search has exactly one result in the database.
 *
 * @param aTerms
 *        The terms to search for.
 * @returns true if the search returns one result, false otherwise.
 */
function search_has_result(aTerms) {
  var options = hs.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = hs.getNewQuery();
  query.searchTerms = aTerms;
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  return cc == 1;
}

add_task(async function test_execute() {
  const SEARCH_TERM = "ユニコード";
  const TEST_URL = "http://example.com/" + SEARCH_TERM + "/";
  await PlacesTestUtils.addVisits(uri(TEST_URL));
  Assert.ok(search_has_result(SEARCH_TERM));
});
