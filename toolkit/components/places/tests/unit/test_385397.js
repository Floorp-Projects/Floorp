/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TOTAL_SITES = 20;

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  let now = Date.now() * 1000;

  for (let i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test-" + i + ".com/";
    let testURI = uri(site);
    let testImageURI = uri(site + "blank.gif");
    let when = now + (i * TOTAL_SITES);
    yield promiseAddVisits([
      { uri: testURI, visitDate: when, transition: TRANSITION_TYPED },
      { uri: testImageURI, visitDate: ++when, transition: TRANSITION_EMBED },
      { uri: testImageURI, visitDate: ++when, transition: TRANSITION_FRAMED_LINK },
      { uri: testURI, visitDate: ++when, transition: TRANSITION_LINK },
    ]);
  }

  // verify our visits AS_VISIT, ordered by date descending
  // including hidden
  // we should get 80 visits:
  // http://www.test-19.com/
  // http://www.test-19.com/blank.gif
  // http://www.test-19.com/
  // http://www.test-19.com/
  // ...
  // http://www.test-0.com/
  // http://www.test-0.com/blank.gif
  // http://www.test-0.com/
  // http://www.test-0.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  // Embed visits are not added to the database, thus they won't appear.
  do_check_eq(cc, 3 * TOTAL_SITES);
  for (let i = 0; i < TOTAL_SITES; i++) {
    let index = i * 3;
    let node = root.getChild(index);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
    node = root.getChild(++index);
    do_check_eq(node.uri, site + "blank.gif");
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
    node = root.getChild(++index);
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
  }
  root.containerOpen = false;

  // verify our visits AS_VISIT, ordered by date descending
  // we should get 40 visits:
  // http://www.test-19.com/
  // http://www.test-19.com/
  // ...
  // http://www.test-0.com/
  // http://www.test-0.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  // 2 * TOTAL_SITES because we count the TYPED and LINK, but not EMBED or FRAMED
  do_check_eq(cc, 2 * TOTAL_SITES); 
  for (let i=0; i < TOTAL_SITES; i++) {
    let index = i * 2;
    let node = root.getChild(index);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
    node = root.getChild(++index);
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
  }
  root.containerOpen = false;

  // test our optimized query for the places menu
  // place:type=0&sort=4&maxResults=10
  // verify our visits AS_URI, ordered by date descending
  // we should get 10 visits:
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.maxResults = 10;
  options.resultType = options.RESULTS_AS_URI;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, options.maxResults);
  for (let i=0; i < cc; i++) {
    let node = root.getChild(i);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
  }
  root.containerOpen = false;

  // test without a maxResults, which executes a different query
  // but the first 10 results should be the same.
  // verify our visits AS_URI, ordered by date descending
  // we should get 20 visits, but the first 10 should be
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_URI;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, TOTAL_SITES);
  for (let i=0; i < 10; i++) {
    let node = root.getChild(i);
    let site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
  }
  root.containerOpen = false;
});
