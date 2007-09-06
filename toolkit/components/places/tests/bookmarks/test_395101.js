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

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}

// get bookmarks root id
var root = bmsvc.bookmarksRoot;

// main
function run_test() {
  // test searching for tagged bookmarks

  // test folder
  var folder = bmsvc.createFolder(root, "bug 395101 test", bmsvc.DEFAULT_INDEX);

  // create a bookmark
  var testURI = uri("http://a1.com");
  var b1 = bmsvc.insertBookmark(folder, testURI,
                                bmsvc.DEFAULT_INDEX, "1 title");

  // tag the bookmarked URI
  tagssvc.tagURI(testURI, ["elephant", "walrus", "giraffe", "turkey", "hiPPo", "BABOON", "alf"]);

  // search for the bookmark, using a tag
  var query = histsvc.getNewQuery();
  query.searchTerms = "elephant";
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  query.setFolders([folder], 1);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);

  // only exact matches
  query.searchTerms = "wal";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 0);

  // case insensitive search term
  query.searchTerms = "WALRUS";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);

  // case insensitive tag
  query.searchTerms = "baboon";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);
}
