const JS_NOW = Date.now();
const DB_NOW = JS_NOW * 1000;
const TEST_URI = uri("http://example.com/");

async function cleanup() {
  await PlacesTestUtils.clearHistory();
  await PlacesUtils.bookmarks.eraseEverything();
  // This is needed to remove place: entries.
  DBConn().executeSimpleSQL("DELETE FROM moz_places");
}

add_task(async function remove_visits_outside_unbookmarked_uri() {
  do_print("*** TEST: Remove some visits outside valid timeframe from an unbookmarked URI");

  do_print("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - 100000 - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);

  do_print("Remove visits using timerange outside the URI's visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
    do_check_eq(visitTime, DB_NOW - 100000 - (i * 1000));
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(await promiseIsURIVisited(TEST_URI));

  await PlacesTestUtils.promiseAsyncUpdates();
  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_outside_bookmarked_uri() {
  do_print("*** TEST: Remove some visits outside valid timeframe from a bookmarked URI");

  do_print("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - 100000 - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("Remove visits using timerange outside the URI's visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
    do_check_eq(visitTime, DB_NOW - 100000 - (i * 1000));
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(await promiseIsURIVisited(TEST_URI));
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_unbookmarked_uri() {
  do_print("*** TEST: Remove some visits from an unbookmarked URI");

  do_print("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);

  do_print("Remove the 5 most recent visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 4),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
    do_check_eq(visitTime, DB_NOW - (i * 1000) - 5000);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(await promiseIsURIVisited(TEST_URI));
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_bookmarked_uri() {
  do_print("*** TEST: Remove some visits from a bookmarked URI");

  do_print("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("Remove the 5 most recent visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 4),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
    do_check_eq(visitTime, DB_NOW - (i * 1000) - 5000);
  }
  root.containerOpen = false;

  do_print("asyncHistory.isURIVisited should return true.");
  do_check_true(await promiseIsURIVisited(TEST_URI));
  await PlacesTestUtils.promiseAsyncUpdates()

  do_print("Frecency should be positive.")
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_all_visits_unbookmarked_uri() {
  do_print("*** TEST: Remove all visits from an unbookmarked URI");

  do_print("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);

  do_print("Remove all visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
  do_check_false(await promiseIsURIVisited(TEST_URI));

  await cleanup();
});

add_task(async function remove_all_visits_bookmarked_uri() {
  do_print("*** TEST: Remove all visits from a bookmarked URI");

  do_print("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - (i * 1000) });
  }
  await PlacesTestUtils.addVisits(visits);
  do_print("Bookmark the URI.");
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark title");
  await PlacesTestUtils.promiseAsyncUpdates();
  let initialFrecency = frecencyForUrl(TEST_URI);

  do_print("Remove all visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

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
  do_check_false(await promiseIsURIVisited(TEST_URI));

  do_print("nsINavBookmarksService.isBookmarked should return true.");
  do_check_true(PlacesUtils.bookmarks.isBookmarked(TEST_URI));
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("Frecency should be smaller.")
  do_check_true(frecencyForUrl(TEST_URI) < initialFrecency);

  await cleanup();
});

add_task(async function remove_all_visits_bookmarked_uri() {
  do_print("*** TEST: Remove some visits from a zero frecency URI retains zero frecency");

  do_print("Add some visits for the URI.");
  await PlacesTestUtils.addVisits([
    { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK, visitDate: (DB_NOW - 86400000000000) },
    { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK, visitDate: DB_NOW }
  ]);

  do_print("Remove newer visit.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW)
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  do_print("URI should still exist in moz_places.");
  do_check_true(page_in_database(TEST_URI.spec));
  do_print("Frecency should be zero.")
  do_check_eq(frecencyForUrl(TEST_URI), 0);

  await cleanup();
});
