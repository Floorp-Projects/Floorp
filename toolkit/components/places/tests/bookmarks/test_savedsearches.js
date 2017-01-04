/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// get bookmarks root id
var root = PlacesUtils.bookmarksMenuFolderId;

// a search term that matches a default bookmark
const searchTerm = "about";

var testRoot;

// main
function run_test() {
  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to the default bookmarks set
  // also, name it using the search term, for testing that containers that match don't show up in query results
  testRoot = PlacesUtils.bookmarks.createFolder(
    root, searchTerm, PlacesUtils.bookmarks.DEFAULT_INDEX);

  run_next_test();
}

add_test(function test_savedsearches_bookmarks() {
  // add a bookmark that matches the search term
  var bookmarkId = PlacesUtils.bookmarks.insertBookmark(
    root, uri("http://foo.com"), PlacesUtils.bookmarks.DEFAULT_INDEX,
    searchTerm);

  // create a saved-search that matches a default bookmark
  var searchId = PlacesUtils.bookmarks.insertBookmark(
    testRoot, uri("place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=1"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, searchTerm);

  // query for the test root, expandQueries=0
  // the query should show up as a regular bookmark
  try {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 0;
    let query = PlacesUtils.history.getNewQuery();
    query.setFolders([testRoot], 1);
    let result = PlacesUtils.history.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    do_check_eq(cc, 1);
    for (let i = 0; i < cc; i++) {
      let node = rootNode.getChild(i);
      // test that queries have valid itemId
      do_check_true(node.itemId > 0);
      // test that the container is closed
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      do_check_eq(node.containerOpen, false);
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=0 query error: " + ex);
  }

  // bookmark saved search
  // query for the test root, expandQueries=1
  // the query should show up as a query container, with 1 child
  try {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 1;
    let query = PlacesUtils.history.getNewQuery();
    query.setFolders([testRoot], 1);
    let result = PlacesUtils.history.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    do_check_eq(cc, 1);
    for (let i = 0; i < cc; i++) {
      let node = rootNode.getChild(i);
      // test that query node type is container when expandQueries=1
      do_check_eq(node.type, node.RESULT_TYPE_QUERY);
      // test that queries (as containers) have valid itemId
      do_check_true(node.itemId > 0);
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      node.containerOpen = true;

      // test that queries have children when excludeItems=1
      // test that query nodes don't show containers (shouldn't have our folder that matches)
      // test that queries don't show themselves in query results (shouldn't have our saved search)
      do_check_eq(node.childCount, 1);

      // test that bookmark shows in query results
      var item = node.getChild(0);
      do_check_eq(item.itemId, bookmarkId);

      // XXX - FAILING - test live-update of query results - add a bookmark that matches the query
      // var tmpBmId = PlacesUtils.bookmarks.insertBookmark(
      //  root, uri("http://" + searchTerm + ".com"),
      //  PlacesUtils.bookmarks.DEFAULT_INDEX, searchTerm + "blah");
      // do_check_eq(query.childCount, 2);

      // XXX - test live-update of query results - delete a bookmark that matches the query
      // PlacesUtils.bookmarks.removeItem(tmpBMId);
      // do_check_eq(query.childCount, 1);

      // test live-update of query results - add a folder that matches the query
      PlacesUtils.bookmarks.createFolder(
        root, searchTerm + "zaa", PlacesUtils.bookmarks.DEFAULT_INDEX);
      do_check_eq(node.childCount, 1);
      // test live-update of query results - add a query that matches the query
      PlacesUtils.bookmarks.insertBookmark(
        root, uri("place:terms=foo&excludeQueries=1&expandQueries=1&queryType=1"),
        PlacesUtils.bookmarks.DEFAULT_INDEX, searchTerm + "blah");
      do_check_eq(node.childCount, 1);
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }

  // delete the bookmark search
  PlacesUtils.bookmarks.removeItem(searchId);

  run_next_test();
});

add_task(function* test_savedsearches_history() {
  // add a visit that matches the search term
  var testURI = uri("http://" + searchTerm + ".com");
  yield PlacesTestUtils.addVisits({ uri: testURI, title: searchTerm });

  // create a saved-search that matches the visit we added
  var searchId = PlacesUtils.bookmarks.insertBookmark(testRoot,
    uri("place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=0"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, searchTerm);

  // query for the test root, expandQueries=1
  // the query should show up as a query container, with 1 child
  try {
    var options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 1;
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = PlacesUtils.history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 1);
    for (var i = 0; i < cc; i++) {
      var node = rootNode.getChild(i);
      // test that query node type is container when expandQueries=1
      do_check_eq(node.type, node.RESULT_TYPE_QUERY);
      // test that queries (as containers) have valid itemId
      do_check_eq(node.itemId, searchId);
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      node.containerOpen = true;

      // test that queries have children when excludeItems=1
      // test that query nodes don't show containers (shouldn't have our folder that matches)
      // test that queries don't show themselves in query results (shouldn't have our saved search)
      do_check_eq(node.childCount, 1);

      // test that history visit shows in query results
      var item = node.getChild(0);
      do_check_eq(item.type, item.RESULT_TYPE_URI);
      do_check_eq(item.itemId, -1); // history visit
      do_check_eq(item.uri, testURI.spec); // history visit

      // test live-update of query results - add a history visit that matches the query
      yield PlacesTestUtils.addVisits({
        uri: uri("http://foo.com"),
        title: searchTerm + "blah"
      });
      do_check_eq(node.childCount, 2);

      // test live-update of query results - delete a history visit that matches the query
      PlacesUtils.history.removePage(uri("http://foo.com"));
      do_check_eq(node.childCount, 1);
      node.containerOpen = false;
    }

    // test live-update of moved queries
    var tmpFolderId = PlacesUtils.bookmarks.createFolder(
      testRoot, "foo", PlacesUtils.bookmarks.DEFAULT_INDEX);
    PlacesUtils.bookmarks.moveItem(
      searchId, tmpFolderId, PlacesUtils.bookmarks.DEFAULT_INDEX);
    var tmpFolderNode = rootNode.getChild(0);
    do_check_eq(tmpFolderNode.itemId, tmpFolderId);
    tmpFolderNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
    tmpFolderNode.containerOpen = true;
    do_check_eq(tmpFolderNode.childCount, 1);

    // test live-update of renamed queries
    PlacesUtils.bookmarks.setItemTitle(searchId, "foo");
    do_check_eq(tmpFolderNode.title, "foo");

    // test live-update of deleted queries
    PlacesUtils.bookmarks.removeItem(searchId);
    try {
      tmpFolderNode = root.getChild(1);
      do_throw("query was not removed");
    } catch (ex) {}

    tmpFolderNode.containerOpen = false;
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }
});
