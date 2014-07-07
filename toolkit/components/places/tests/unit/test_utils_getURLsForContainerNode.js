/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of PlacesUtils.getURLsForContainerNode and
  * PlacesUtils.hasChildURIs (those helpers share almost all of their code)
  */

var PU = PlacesUtils;
var hs = PU.history;
var bs = PU.bookmarks;

var tests = [

function() {
  dump("\n\n*** TEST: folder\n");
  // This is the folder we will check for children.
  var folderId = bs.createFolder(bs.toolbarFolder, "folder", bs.DEFAULT_INDEX);

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  bs.createFolder(folderId, "inside folder", bs.DEFAULT_INDEX);
  bs.insertBookmark(folderId, uri("place:sort=1"),
                    bs.DEFAULT_INDEX, "inside query");

  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  var options = hs.getNewQueryOptions();

  dump("Check folder without uri nodes\n");
  check_uri_nodes(query, options, 0);

  dump("Check folder with uri nodes\n");
  // Add an uri node, this should be considered.
  bs.insertBookmark(folderId, uri("http://www.mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark");
  check_uri_nodes(query, options, 1);
},

function() {
  dump("\n\n*** TEST: folder in an excludeItems root\n");
  // This is the folder we will check for children.
  var folderId = bs.createFolder(bs.toolbarFolder, "folder", bs.DEFAULT_INDEX);

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  bs.createFolder(folderId, "inside folder", bs.DEFAULT_INDEX);
  bs.insertBookmark(folderId, uri("place:sort=1"), bs.DEFAULT_INDEX, "inside query");

  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  var options = hs.getNewQueryOptions();
  options.excludeItems = true;

  dump("Check folder without uri nodes\n");
  check_uri_nodes(query, options, 0);

  dump("Check folder with uri nodes\n");
  // Add an uri node, this should be considered.
  bs.insertBookmark(folderId, uri("http://www.mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark");
  check_uri_nodes(query, options, 1);
},

function() {
  dump("\n\n*** TEST: query\n");
  // This is the query we will check for children.
  bs.insertBookmark(bs.toolbarFolder, uri("place:folder=BOOKMARKS_MENU&sort=1"),
                    bs.DEFAULT_INDEX, "inside query");

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  bs.createFolder(bs.bookmarksMenuFolder, "inside folder", bs.DEFAULT_INDEX);
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("place:sort=1"),
                    bs.DEFAULT_INDEX, "inside query");

  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  var options = hs.getNewQueryOptions();

  dump("Check query without uri nodes\n");
  check_uri_nodes(query, options, 0);

  dump("Check query with uri nodes\n");
  // Add an uri node, this should be considered.
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://www.mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark");
  check_uri_nodes(query, options, 1);
},

function() {
  dump("\n\n*** TEST: excludeItems Query\n");
  // This is the query we will check for children.
  bs.insertBookmark(bs.toolbarFolder, uri("place:folder=BOOKMARKS_MENU&sort=8"),
                    bs.DEFAULT_INDEX, "inside query");

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  bs.createFolder(bs.bookmarksMenuFolder, "inside folder", bs.DEFAULT_INDEX);
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("place:sort=1"),
                    bs.DEFAULT_INDEX, "inside query");

  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  var options = hs.getNewQueryOptions();
  options.excludeItems = true;

  dump("Check folder without uri nodes\n");
  check_uri_nodes(query, options, 0);

  dump("Check folder with uri nodes\n");
  // Add an uri node, this should be considered.
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://www.mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark");
  check_uri_nodes(query, options, 1);
},

function() {
  dump("\n\n*** TEST: !expandQueries Query\n");
  // This is the query we will check for children.
  bs.insertBookmark(bs.toolbarFolder, uri("place:folder=BOOKMARKS_MENU&sort=8"),
                    bs.DEFAULT_INDEX, "inside query");

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  bs.createFolder(bs.bookmarksMenuFolder, "inside folder", bs.DEFAULT_INDEX);
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("place:sort=1"),
                    bs.DEFAULT_INDEX, "inside query");

  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  var options = hs.getNewQueryOptions();
  options.expandQueries = false;

  dump("Check folder without uri nodes\n");
  check_uri_nodes(query, options, 0);

  dump("Check folder with uri nodes\n");
  // Add an uri node, this should be considered.
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://www.mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark");
  check_uri_nodes(query, options, 1);
}

];

/**
 * Executes a query and checks number of uri nodes in the first container in 
 * query's results.  To correctly test a container ensure that the query will
 * return only your container in the first level.
 *
 * @param  aQuery
 *         nsINavHistoryQuery object defining the query
 * @param  aOptions
 *         nsINavHistoryQueryOptions object defining the query's options
 * @param  aExpectedURINodes
 *         number of expected uri nodes
 */
function check_uri_nodes(aQuery, aOptions, aExpectedURINodes) {
  var result = hs.executeQuery(aQuery, aOptions);
  var root = result.root;
  root.containerOpen = true;
  var node = root.getChild(0);
  do_check_eq(PU.hasChildURIs(node), aExpectedURINodes > 0);
  do_check_eq(PU.hasChildURIs(node, true), aExpectedURINodes > 1);
  do_check_eq(PU.getURLsForContainerNode(node).length, aExpectedURINodes);
  root.containerOpen = false;
}

function run_test() {
  tests.forEach(function(aTest) {
                  remove_all_bookmarks();
                  aTest();
                });
  // Cleanup.
  remove_all_bookmarks();
}
