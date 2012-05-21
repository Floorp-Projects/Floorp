/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
} 

// Get global history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// get bookmarks root id
var root = bmsvc.bookmarksMenuFolder;

// a search term that matches a default bookmark
const searchTerm = "about";

var testRoot;

// main
function run_test() {
  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to the default bookmarks set
  // also, name it using the search term, for testing that containers that match don't show up in query results
  testRoot = bmsvc.createFolder(root, searchTerm, bmsvc.DEFAULT_INDEX);

  run_next_test();
}

add_test(function test_savedsearches_bookmarks() {
  // add a bookmark that matches the search term
  var bookmarkId = bmsvc.insertBookmark(root, uri("http://foo.com"), bmsvc.DEFAULT_INDEX, searchTerm);

  // create a saved-search that matches a default bookmark
  var searchId = bmsvc.insertBookmark(testRoot,
                                      uri("place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=1"),
                                      bmsvc.DEFAULT_INDEX, searchTerm);

  // query for the test root, expandQueries=0
  // the query should show up as a regular bookmark
  try {
    var options = histsvc.getNewQueryOptions();
    options.expandQueries = 0;
    var query = histsvc.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 1);
    for (var i = 0; i < cc; i++) {
      var node = rootNode.getChild(i);
      // test that queries have valid itemId
      do_check_true(node.itemId > 0);
      // test that the container is closed
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      do_check_eq(node.containerOpen, false);
    }
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("expandQueries=0 query error: " + ex);
  }

  // bookmark saved search
  // query for the test root, expandQueries=1
  // the query should show up as a query container, with 1 child
  try {
    var options = histsvc.getNewQueryOptions();
    options.expandQueries = 1;
    var query = histsvc.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 1);
    for (var i = 0; i < cc; i++) {
      var node = rootNode.getChild(i);
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
      //var tmpBmId = bmsvc.insertBookmark(root, uri("http://" + searchTerm + ".com"), bmsvc.DEFAULT_INDEX, searchTerm + "blah");
      //do_check_eq(query.childCount, 2);

      // XXX - test live-update of query results - delete a bookmark that matches the query
      //bmsvc.removeItem(tmpBMId);
      //do_check_eq(query.childCount, 1);

      // test live-update of query results - add a folder that matches the query
      bmsvc.createFolder(root, searchTerm + "zaa", bmsvc.DEFAULT_INDEX);
      do_check_eq(node.childCount, 1);
      // test live-update of query results - add a query that matches the query
      bmsvc.insertBookmark(root, uri("place:terms=foo&excludeQueries=1&expandQueries=1&queryType=1"),
                           bmsvc.DEFAULT_INDEX, searchTerm + "blah");
      do_check_eq(node.childCount, 1);
    }
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }

  // delete the bookmark search
  bmsvc.removeItem(searchId);

  run_next_test();
});

add_test(function test_savedsearches_history() {
  // add a visit that matches the search term
  var testURI = uri("http://" + searchTerm + ".com");
  addVisits({ uri: testURI, title: searchTerm }, function afterAddVisits() {

    // create a saved-search that matches the visit we added
    var searchId = bmsvc.insertBookmark(testRoot,
                                        uri("place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=0"),
                                        bmsvc.DEFAULT_INDEX, searchTerm);

    // query for the test root, expandQueries=1
    // the query should show up as a query container, with 1 child
    try {
      var options = histsvc.getNewQueryOptions();
      options.expandQueries = 1;
      var query = histsvc.getNewQuery();
      query.setFolders([testRoot], 1);
      var result = histsvc.executeQuery(query, options);
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
        PlacesUtils.history.addVisit(uri("http://foo.com"),
                                     Date.now() * 1000,
                                     null,
                                     Ci.nsINavHistoryService.TRANSITION_LINK,
                                     false,
                                     0);
        PlacesUtils.ghistory2.setPageTitle(uri("http://foo.com"),
                                           searchTerm + "blah");
        do_check_eq(node.childCount, 2);

        // test live-update of query results - delete a history visit that matches the query
        bhist.removePage(uri("http://foo.com"));
        do_check_eq(node.childCount, 1);
        node.containerOpen = false;
      }

      // test live-update of moved queries
      var tmpFolderId = bmsvc.createFolder(testRoot, "foo", bmsvc.DEFAULT_INDEX);
      bmsvc.moveItem(searchId, tmpFolderId, bmsvc.DEFAULT_INDEX);
      var tmpFolderNode = rootNode.getChild(0);
      do_check_eq(tmpFolderNode.itemId, tmpFolderId);
      tmpFolderNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      tmpFolderNode.containerOpen = true;
      do_check_eq(tmpFolderNode.childCount, 1);

      // test live-update of renamed queries
      bmsvc.setItemTitle(searchId, "foo");
      do_check_eq(tmpFolderNode.title, "foo");

      // test live-update of deleted queries
      bmsvc.removeItem(searchId);
      try {
        var tmpFolderNode = root.getChild(1);
        do_throw("query was not removed");
      } catch(ex) {}

      tmpFolderNode.containerOpen = false;
      rootNode.containerOpen = false;
    }
    catch(ex) {
      do_throw("expandQueries=1 bookmarks query: " + ex);
    }

    run_next_test();
  });
});