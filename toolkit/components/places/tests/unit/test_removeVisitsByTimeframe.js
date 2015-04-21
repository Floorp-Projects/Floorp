/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NOW = Date.now() * 1000;
const TEST_URI = uri("http://example.com/");
const PLACE_URI = uri("place:queryType=0&sort=8&maxResults=10");

function* cleanup() {
  yield PlacesTestUtils.clearHistory();
  yield PlacesUtils.bookmarks.eraseEverything();
  // This is needed to remove place: entries.
  DBConn().executeSimpleSQL("DELETE FROM moz_places");
}

add_task(function remove_visits_outside_unbookmarked_uri() {
  do_print("*** TEST: Remove some visits outside valid timeframe from an unbookmarked URI");
 
  do_print("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - 1000 - i });
  }
  yield PlacesTestUtils.addVisits(visits);

  do_print("Remove visits using timerange outside the URI's visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that all visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 10);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    do_check_eq(visitTime, NOW - 1000 - i);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(yield promiseIsURIVisited(TEST_URI));

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  yield cleanup();
});

add_task(function remove_visits_outside_bookmarked_uri() {
  do_print("*** TEST: Remove some visits outside valid timeframe from a bookmarked URI");

  do_print("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - 1000 - i });
  }
  yield PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Remove visits using timerange outside the URI's visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that all visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 10);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    do_check_eq(visitTime, NOW - 1000 - i);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(yield promiseIsURIVisited(TEST_URI));
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  yield cleanup();
});

add_task(function remove_visits_unbookmarked_uri() {
  do_print("*** TEST: Remove some visits from an unbookmarked URI");

  do_print("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - i });
  }
  yield PlacesTestUtils.addVisits(visits);

  do_print("Remove the 5 most recent visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 4, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that only the older 5 visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 5);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    do_check_eq(visitTime, NOW - i - 5);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(yield promiseIsURIVisited(TEST_URI));
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  yield cleanup();
});

add_task(function remove_visits_bookmarked_uri() {
  do_print("*** TEST: Remove some visits from a bookmarked URI");

  do_print("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - i });
  }
  yield PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Remove the 5 most recent visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 4, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that only the older 5 visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 5);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    do_check_eq(visitTime, NOW - i - 5);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(yield promiseIsURIVisited(TEST_URI));
  yield PlacesTestUtils.promiseAsyncUpdates()

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  yield cleanup();
});

add_task(function remove_all_visits_unbookmarked_uri() {
  do_print("*** TEST: Remove all visits from an unbookmarked URI");

  do_print("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - i });
  }
  yield PlacesTestUtils.addVisits(visits);

  do_print("Remove all visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should no longer exist in moz_places.");
  do_check_false(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that no visits exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return false.");
  do_check_false(yield promiseIsURIVisited(TEST_URI));

  yield cleanup();
});

add_task(function remove_all_visits_unbookmarked_place_uri() {
  do_print("*** TEST: Remove all visits from an unbookmarked place: URI");
  do_print("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: PLACE_URI, visitDate: NOW - i });
  }
  yield PlacesTestUtils.addVisits(visits);

  do_print("Remove all visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(PLACE_URI.spec));

  do_print("Run a history query and check that no visits exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return false.");
  do_check_false(yield promiseIsURIVisited(PLACE_URI));
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be zero.")
  do_check_eq(frecencyForUrl(PLACE_URI.spec), 0);

  yield cleanup();
});

add_task(function remove_all_visits_bookmarked_uri() {
  do_print("*** TEST: Remove all visits from a bookmarked URI");

  do_print("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: NOW - i });
  }
  yield PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Remove all visits.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));

  do_print("Run a history query and check that no visits exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return false.");
  do_check_false(yield promiseIsURIVisited(TEST_URI));

  do_print("nsINavBookmarksService.isBookmarked should return true.");
  do_check_true(PlacesUtils.bookmarks.isBookmarked(TEST_URI));
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be negative.")
  do_check_true(frecencyForUrl(TEST_URI) < 0);

  yield cleanup();
});

add_task(function remove_all_visits_bookmarked_uri() {
  do_print("*** TEST: Remove some visits from a zero frecency URI retains zero frecency");

  do_print("Add some visits for the URI.");
  yield PlacesTestUtils.addVisits([
    { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK, visitDate: (NOW - 86400000000) },
    { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK, visitDate: NOW }
  ]);

  do_print("Remove newer visit.");
  PlacesUtils.history.removeVisitsByTimeframe(NOW - 10, NOW);
  yield PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));
  do_print("Frecency should be zero.")
  do_check_eq(frecencyForUrl(TEST_URI), 0);

  yield cleanup();
});

function run_test() {
  run_next_test();
}
