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
 *  Seth Spitzer <sspitzer@mozilla.com>
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

// get bookmarks root id
var root = bmsvc.bookmarksMenuFolder;

// main
function run_test() {
  // test search on folder with various sorts and max results
  // see bug #385829 for more details
  var folder = bmsvc.createFolder(root, "bug 385829 test", bmsvc.DEFAULT_INDEX);
  var b1 = bmsvc.insertBookmark(folder, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");

  var b2 = bmsvc.insertBookmark(folder, uri("http://a2.com/"),
                                bmsvc.DEFAULT_INDEX, "2 title");

  var b3 = bmsvc.insertBookmark(folder, uri("http://a3.com/"),
                                bmsvc.DEFAULT_INDEX, "3 title");

  var b4 = bmsvc.insertBookmark(folder, uri("http://a4.com/"),
                                bmsvc.DEFAULT_INDEX, "4 title");

  // ensure some unique values for date added and last modified
  // for date added:    b1 < b2 < b3 < b4
  // for last modified: b1 > b2 > b3 > b4
  bmsvc.setItemDateAdded(b1, 1000);
  bmsvc.setItemDateAdded(b2, 2000);
  bmsvc.setItemDateAdded(b3, 3000);
  bmsvc.setItemDateAdded(b4, 4000);

  bmsvc.setItemLastModified(b1, 4000);
  bmsvc.setItemLastModified(b2, 3000);
  bmsvc.setItemLastModified(b3, 2000);
  bmsvc.setItemLastModified(b4, 1000);

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 3;
  query.setFolders([folder], 1);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // test SORT_BY_DATEADDED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_DATEADDED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b3);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b1);
  do_check_true(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_LASTMODIFIED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b3);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b1);
  do_check_true(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);

  // test SORT_BY_LASTMODIFIED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;

  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_ASCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_DESCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b4);
  do_check_eq(rootNode.getChild(1).itemId, b3);
  do_check_eq(rootNode.getChild(2).itemId, b2);
  do_check_true(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_ASCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b4);
  do_check_eq(rootNode.getChild(1).itemId, b3);
  do_check_eq(rootNode.getChild(2).itemId, b2);
  do_check_true(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_DESCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;
}
