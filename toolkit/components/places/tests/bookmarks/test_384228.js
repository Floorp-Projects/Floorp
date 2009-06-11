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
 *  Asaf Romano <mano@mozilla.com>
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
var root = bmsvc.bookmarksRoot;

// main
function run_test() {
  // test querying for bookmarks in multiple folders
  var testFolder1 = bmsvc.createFolder(root, "bug 384228 test folder 1",
                                       bmsvc.DEFAULT_INDEX);
  var testFolder2 = bmsvc.createFolder(root, "bug 384228 test folder 2",
                                       bmsvc.DEFAULT_INDEX);
  var testFolder3 = bmsvc.createFolder(root, "bug 384228 test folder 3",
                                       bmsvc.DEFAULT_INDEX);

  var b1 = bmsvc.insertBookmark(testFolder1, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b1 (folder 1)");
  var b2 = bmsvc.insertBookmark(testFolder1, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b2 (folder 1)");
  var b3 = bmsvc.insertBookmark(testFolder2, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b3 (folder 2)");
  var b4 = bmsvc.insertBookmark(testFolder3, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b4 (folder 3)");
  // also test recursive search
  var testFolder1_1 = bmsvc.createFolder(testFolder1, "bug 384228 test folder 1.1",
                                         bmsvc.DEFAULT_INDEX);
  var b5 = bmsvc.insertBookmark(testFolder1_1, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "title b5 (folder 1.1)");
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.searchTerms = "title";
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  query.setFolders([testFolder1, testFolder2], 2);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // should not match item from folder 3
  do_check_eq(rootNode.childCount, 4);

  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_eq(rootNode.getChild(3).itemId, b5);
}
