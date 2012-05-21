/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function add_visit(aURI, aType) {
  PlacesUtils.history.addVisit(uri(aURI), Date.now() * 1000, null, aType,
                                 false, 0);
}

const TOTAL_SITES = 20;

function run_test() {
  PlacesUtils.history.runInBatchMode({
    runBatched: function (aUserData) {
      for (let i = 0; i < TOTAL_SITES; i++) {
        for (let j = 0; j <= i; j++) {
          add_visit("http://www.test-" + i + ".com/", TRANSITION_TYPED);
          // because these are embedded visits, they should not show up on our
          // query results.  If they do, we have a problem.
          add_visit("http://www.hidden.com/hidden.gif", TRANSITION_EMBED);
          add_visit("http://www.alsohidden.com/hidden.gif", TRANSITION_FRAMED_LINK);
        }
      }
    }
  }, null);

  // test our optimized query for the "Most Visited" item
  // in the "Smart Bookmarks" folder
  // place:queryType=0&sort=8&maxResults=10
  // verify our visits AS_URI, ordered by visit count descending
  // we should get 10 visits:
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
  options.maxResults = 10;
  options.resultType = options.RESULTS_AS_URI;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, options.maxResults);
  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_URI);
  }
  root.containerOpen = false;

  // test without a maxResults, which executes a different query
  // but the first 10 results should be the same.
  // verify our visits AS_URI, ordered by visit count descending
  // we should get 20 visits, but the first 10 should be
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
  options.resultType = options.RESULTS_AS_URI;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, TOTAL_SITES);
  for (let i = 0; i < 10; i++) {
    let node = root.getChild(i);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_URI);
  }
  root.containerOpen = false;
}
