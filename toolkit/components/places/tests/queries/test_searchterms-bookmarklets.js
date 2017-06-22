/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that bookmarklets are returned by searches with searchTerms.

var testData = [
  { isInQuery: true,
    isBookmark: true,
    title: "bookmark 1",
    uri: "http://mozilla.org/script/"
  },

  { isInQuery: true,
    isBookmark: true,
    title: "bookmark 2",
    uri: "javascript:alert('moz');"
  }
];

add_task(async function test_initalize() {
  await task_populateDB(testData);
});

add_test(function test_search_by_title() {
  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "bookmark";
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult(testData, root);
  root.containerOpen = false;

  run_next_test();
});

add_test(function test_search_by_schemeToken() {
  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "script";
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult(testData, root);
  root.containerOpen = false;

  run_next_test();
});

add_test(function test_search_by_uriAndTitle() {
  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "moz";
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult(testData, root);
  root.containerOpen = false;

  run_next_test();
});
