/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
var root = bmsvc.bookmarksRoot;

// main
function run_test() {
  // a search term that matches a default bookmark
  var searchTerm = "about";

  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to the default bookmarks set
  // also, name it using the search term, for testing that containers that match don't show up in query results
  var testRoot = bmsvc.createFolder(root, searchTerm, bmsvc.DEFAULT_INDEX);

  /******************************************
  * saved searches - bookmarks 
  ******************************************/

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
  }
  catch(ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }

  // delete the bookmark search
  bmsvc.removeItem(searchId);

  /******************************************
  * saved searches - history 
  ******************************************/

  // add a visit that matches the search term
  var testURI = uri("http://" + searchTerm + ".com");
  bhist.addPageWithDetails(testURI, searchTerm, Date.now());

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
      bhist.addPageWithDetails(uri("http://foo.com"), searchTerm + "blah", Date.now());
      do_check_eq(node.childCount, 2);

      // test live-update of query results - delete a history visit that matches the query
      bhist.removePage(uri("http://foo.com"));
      do_check_eq(node.childCount, 1);
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
  }
  catch(ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }
}
