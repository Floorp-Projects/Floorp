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
 * The Original Code is Bug 384370 code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

Components.utils.import("resource://gre/modules/utils.js");

const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

function run_test() {
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
  PlacesUtils.importJSONNode(stream._str, PlacesUtils.toolbarFolderId, -1);

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

  var queryNode2 = root.getChild(root.childCount-1);
  do_check_eq(queryNode2.type, queryNode2.RESULT_TYPE_QUERY);
  queryNode2.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  queryNode2.containerOpen = true;
  do_check_eq(queryNode2.childCount, 0);
}
