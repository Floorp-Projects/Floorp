/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  // because there is one query, one folder,
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

  sf2Node.containerOpen = false;
  sf1Node.containerOpen = false;
  root.containerOpen = false;

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
  root.containerOpen = false;

  // XXX TODO
  // test that if we have: more than one query, 
  // multiple folders, a begin time, an end time, a domain, a uri
  // or a search term, that we get the (correct) flat list results
  // (like we do when specified maxResults)
}
