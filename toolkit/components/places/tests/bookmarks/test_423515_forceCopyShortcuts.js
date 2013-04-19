/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

function run_test() {
  run_next_test();
}

add_task(function test_force_copy() {
  /*
  - create folder A
  - add a bookmark to it
  - create a bookmark that's a place: folder shortcut to the new folder
  - serialize it to JSON, forcing copy
  - import JSON
  - confirm that the newly imported folder is a full copy and not a shortcut
  */

  var folderA =
    PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                       "test folder", DEFAULT_INDEX);
  var bookmarkURI = uri("http://test");
  PlacesUtils.bookmarks.insertBookmark(folderA, bookmarkURI,
                                       DEFAULT_INDEX, "");

  // create the query
  var queryURI = uri("place:folder=" + folderA);
  var queryTitle = "test query";
  var queryId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         queryURI, DEFAULT_INDEX, queryTitle);
  LOG("queryId: " + queryId);

  // create a query that's *not* a folder shortcut
  var queryURI2 = uri("place:");
  var queryTitle2 = "non-folder test query";
  var queryId2 =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         queryURI2, DEFAULT_INDEX, queryTitle2);

  // check default state
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = PlacesUtils.history.getNewQueryOptions();
  options.expandQueries = true;
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  // check folder query node
  var queryNode = root.getChild(root.childCount-2);
  do_check_eq(queryNode.type, queryNode.RESULT_TYPE_FOLDER_SHORTCUT);
  do_check_eq(queryNode.title, queryTitle);
  do_check_true(queryURI.equals(uri(queryNode.uri)));
  queryNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  queryNode.containerOpen = true;
  do_check_eq(queryNode.childCount, 1);
  var bookmark = queryNode.getChild(0);
  do_check_true(bookmarkURI.equals(uri(bookmark.uri)));
  queryNode.containerOpen = false;

  // check non-folder query node
  var queryNode2 = root.getChild(root.childCount-1);
  do_check_eq(queryNode2.type, queryNode2.RESULT_TYPE_QUERY);
  do_check_eq(queryNode2.title, queryTitle2);
  do_check_true(queryURI2.equals(uri(queryNode2.uri)));
  queryNode2.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  queryNode2.containerOpen = true;
  do_check_eq(queryNode2.childCount, 0);
  queryNode2.containerOpen = false;

  // clean up
  root.containerOpen = false;

  // serialize
  var stream = {
    _str: "",
    write: function(aData, aLen) {
      this._str += aData;
    }
  };
  PlacesUtils.serializeNodeAsJSONToOutputStream(queryNode, stream, false, true);

  LOG("SERIALIZED: " + stream._str);

  PlacesUtils.bookmarks.removeItem(queryId);

  // import
  Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
  yield BookmarkJSONUtils.importJSONNode(stream._str, PlacesUtils.toolbarFolderId, -1);

  // query for node
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = PlacesUtils.history.getNewQueryOptions();
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  // check folder node (no longer shortcut)
  var queryNode = root.getChild(root.childCount-2);
  do_check_eq(queryNode.type, queryNode.RESULT_TYPE_FOLDER);
  queryNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  queryNode.containerOpen = true;
  do_check_eq(queryNode.childCount, 1);
  var child = queryNode.getChild(0);
  do_check_true(bookmarkURI.equals(uri(child.uri)));
  queryNode.containerOpen = false;

  var queryNode2 = root.getChild(root.childCount-1);
  do_check_eq(queryNode2.type, queryNode2.RESULT_TYPE_QUERY);
  queryNode2.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  queryNode2.containerOpen = true;
  do_check_eq(queryNode2.childCount, 0);
  queryNode.containerOpen = false;

  root.containerOpen = false;
});
