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

// get bookmarks menu folder id
var testRoot = bmsvc.bookmarksMenuFolder;

// main
function run_test() {
  var fz = bmsvc.createFolder(testRoot, "fz", bmsvc.DEFAULT_INDEX);
  var fzb1 = bmsvc.insertBookmark(fz, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");

  var fa = bmsvc.createFolder(testRoot, "fa", bmsvc.DEFAULT_INDEX);
  var fab1 = bmsvc.insertBookmark(fa, uri("http://a.a1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");

  var f2 = bmsvc.createFolder(testRoot, "f2", bmsvc.DEFAULT_INDEX);
  var f2b2 = bmsvc.insertBookmark(f2, uri("http://a2.com/"),
                                bmsvc.DEFAULT_INDEX, "2 title");
  var f2b3 = bmsvc.insertBookmark(f2, uri("http://a3.com/"),
                                bmsvc.DEFAULT_INDEX, "3 title");

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
  options.applyOptionsToContainers = true;
  query.setFolders([testRoot], 1);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // test SORT_BY_COUNT_ASCENDING
  result.sortingMode = options.SORT_BY_COUNT_ASCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).title, "fa");
  do_check_eq(rootNode.getChild(1).title, "fz");
  do_check_eq(rootNode.getChild(2).title, "f2");

  // check the contents of the containers
  var rfNode = rootNode.getChild(0);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, fab1);

  rfNode = rootNode.getChild(1);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, fzb1);

  rfNode = rootNode.getChild(2);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 2);
  do_check_eq(rfNode.getChild(0).itemId, f2b2);
  do_check_eq(rfNode.getChild(1).itemId, f2b3);

  // test SORT_BY_COUNT_DESCENDING
  result.sortingMode = options.SORT_BY_COUNT_DESCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).title, "f2");
  do_check_eq(rootNode.getChild(1).title, "fz");
  do_check_eq(rootNode.getChild(2).title, "fa");

  // check the contents of the containers
  rfNode = rootNode.getChild(0);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 2);
  do_check_eq(rfNode.getChild(0).itemId, f2b3);
  do_check_eq(rfNode.getChild(1).itemId, f2b2);

  rfNode = rootNode.getChild(1);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, fzb1);

  rfNode = rootNode.getChild(2);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, fab1);

  // test SORT_BY_COUNT_DESCENDING with max results of 2
  // note that we have 3 items, but only 2 folders 
  options = histsvc.getNewQueryOptions();
  query = histsvc.getNewQuery();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
  options.maxResults = 2;
  options.applyOptionsToContainers = true;
  query.setFolders([testRoot], 1);

  result = histsvc.executeQuery(query, options);
  result.sortingMode = options.SORT_BY_COUNT_DESCENDING;

  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 2);
  do_check_eq(rootNode.getChild(0).title, "f2");
  do_check_eq(rootNode.getChild(1).title, "fz");

  // check the contents of the containers
  rfNode = rootNode.getChild(0);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 2);
  do_check_eq(rfNode.getChild(0).itemId, f2b3);
  do_check_eq(rfNode.getChild(1).itemId, f2b2);

  rfNode = rootNode.getChild(1);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, fzb1);
}
