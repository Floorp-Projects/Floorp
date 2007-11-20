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
 *  Dan Mills <thunder@mozilla.com>
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

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);

// main
function run_test() {
  // add a folder
  var folder = bmsvc.createFolder(bmsvc.placesRoot, "test folder", bmsvc.DEFAULT_INDEX);

  // add a bookmark to the folder
  var b1 = bmsvc.insertBookmark(folder, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");
  // add a subfolder
  var sf1 = bmsvc.createFolder(folder, "subfolder 1", bmsvc.DEFAULT_INDEX);

  // add a bookmark to the subfolder
  var b2 = bmsvc.insertBookmark(sf1, uri("http://a2.com/"),
                                bmsvc.DEFAULT_INDEX, "2 title");

  // add a subfolder to the subfolder
  var sf2 = bmsvc.createFolder(sf1, "subfolder 2", bmsvc.DEFAULT_INDEX);

  // add a bookmark to the subfolder of the subfolder
  var b3 = bmsvc.insertBookmark(sf2, uri("http://a3.com/"),
                                bmsvc.DEFAULT_INDEX, "3 title");

  // bookmark query that should result in the "hierarchical" result
  // because there is one query, no grouping, one folder,
  // no begin time, no end time, no domain, no uri, no search term
  // and no max results.  See GetSimpleBookmarksQueryFolder()
  // for more details.
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var query = histsvc.getNewQuery();
  query.setFolders([folder], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 2);
  do_check_eq(root.getChild(0).itemId, b1);
  do_check_eq(root.getChild(1).itemId, sf1);

  // check the contents of the subfolder
  var sf1Node = root.getChild(1);
  sf1Node = sf1Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf1Node.containerOpen = true;
  do_check_eq(sf1Node.childCount, 2);
  do_check_eq(sf1Node.getChild(0).itemId, b2);
  do_check_eq(sf1Node.getChild(1).itemId, sf2);

  // check the contents of the subfolder's subfolder
  var sf2Node = sf1Node.getChild(1);
  sf2Node = sf2Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf2Node.containerOpen = true;
  do_check_eq(sf2Node.childCount, 1);
  do_check_eq(sf2Node.getChild(0).itemId, b3);

  // bookmark query that should result in a flat list
  // because we specified max results
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 10;
  var query = histsvc.getNewQuery();
  query.setFolders([folder], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);
  do_check_eq(root.getChild(0).itemId, b1);
  do_check_eq(root.getChild(1).itemId, b2);
  do_check_eq(root.getChild(2).itemId, b3);

  // XXX TODO
  // test that if we have: more than one query, 
  // multiple folders, a begin time, an end time, a domain, a uri
  // or a search term, that we get the (correct) flat list results
  // (like we do when specified maxResults)

  // bookmark query that should result in results grouped by folder
  // because we specified GROUP_BY_FOLDER
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
  var query = histsvc.getNewQuery();
  query.setFolders([folder], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);

  // note these containers are nsNavHistoryContainerResultNodes, 
  // created in GroupByFolder() and not nsNavHistoryFolderResultNodes,
  do_check_eq(root.getChild(0).title, "test folder");
  do_check_eq(root.getChild(1).title, "subfolder 1");
  do_check_eq(root.getChild(2).title, "subfolder 2");

  // check the contents of the "test folder" container
  var rfNode = root.getChild(0);
  rfNode = rfNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  rfNode.containerOpen = true;
  do_check_eq(rfNode.childCount, 1);
  do_check_eq(rfNode.getChild(0).itemId, b1);

  // check the contents of the "subfolder 1" container
  var sf1Node = root.getChild(1);
  sf1Node = sf1Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf1Node.containerOpen = true;
  do_check_eq(sf1Node.childCount, 1);
  do_check_eq(sf1Node.getChild(0).itemId, b2);

  // check the contents of the "subfolder 1" container
  var sf2Node = root.getChild(2);
  sf2Node = sf2Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf2Node.containerOpen = true;
  do_check_eq(sf2Node.childCount, 1);
  do_check_eq(sf2Node.getChild(0).itemId, b3);
}
