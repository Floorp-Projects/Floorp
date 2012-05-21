/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function add_visit(aURI, aType) {
  PlacesUtils.history.addVisit(uri(aURI), Date.now() * 1000, null, aType,
                                 false, 0);
}

function run_test() {
  let count_visited_URIs = ["http://www.test-link.com/",
                            "http://www.test-typed.com/",
                            "http://www.test-bookmark.com/",
                            "http://www.test-redirect-permanent.com/",
                            "http://www.test-redirect-temporary.com/"];

  let notcount_visited_URIs = ["http://www.test-embed.com/",
                               "http://www.test-download.com/",
                               "http://www.test-framed.com/"];

  // add visits, one for each transition type
  add_visit("http://www.test-link.com/", TRANSITION_LINK);
  add_visit("http://www.test-typed.com/", TRANSITION_TYPED);
  add_visit("http://www.test-bookmark.com/", TRANSITION_BOOKMARK);
  add_visit("http://www.test-embed.com/", TRANSITION_EMBED);
  add_visit("http://www.test-framed.com/", TRANSITION_FRAMED_LINK);
  add_visit("http://www.test-redirect-permanent.com/", TRANSITION_REDIRECT_PERMANENT);
  add_visit("http://www.test-redirect-temporary.com/", TRANSITION_REDIRECT_TEMPORARY);
  add_visit("http://www.test-download.com/", TRANSITION_DOWNLOAD);

  // check that all links are marked as visited
  count_visited_URIs.forEach(function (visited_uri) {
    do_check_eq(PlacesUtils.bhistory.isVisited(uri(visited_uri)), true);
  });
  notcount_visited_URIs.forEach(function (visited_uri) {
    do_check_eq(PlacesUtils.bhistory.isVisited(uri(visited_uri)), true);
  });

  // check that visit_count does not take in count embed and downloads
  // maxVisits query are directly binded to visit_count
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  let query = PlacesUtils.history.getNewQuery();
  query.minVisits = 1;
  let root = PlacesUtils.history.executeQuery(query, options).root;

  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, count_visited_URIs.length);

  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    do_check_neq(count_visited_URIs.indexOf(node.uri), -1);
  }
  root.containerOpen = false;
}
