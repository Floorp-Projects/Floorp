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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@supereva.it> (Original Author)
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

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history services\n");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
}
catch(ex) {
  do_throw("Could not get the nav-bookmarks-service\n");
}

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}


// main
function run_test() {
  var uri1 = uri("http://foo.bar/");

  // create 2 bookmarks
  var bookmark1id = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, uri1,
                                         bmsvc.DEFAULT_INDEX, "title 1");
  var bookmark2id = bmsvc.insertBookmark(bmsvc.toolbarFolder, uri1,
                                         bmsvc.DEFAULT_INDEX, "title 2");
  // add a new tag
  tagssvc.tagURI(uri1, ["foo"]);

  // get tag folder id
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.tagsFolder], 1);
  var result = histsvc.executeQuery(query, options);
  var tagRoot = result.root;
  tagRoot.containerOpen = true;
  var tagNode = tagRoot.getChild(0)
                       .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  var tagItemId = tagNode.itemId;
  tagRoot.containerOpen = false;

  // change bookmark 1 title
  bmsvc.setItemTitle(bookmark1id, "new title 1");

  // Query the tag.
  options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_QUERY;

  query = histsvc.getNewQuery();
  result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);

  var theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  // Bug 524219: Check that renaming the tag shows up in the result.
  do_check_eq(theTag.title, "foo")
  bmsvc.setItemTitle(tagItemId, "bar");

  // Check that the item has been replaced
  do_check_neq(theTag, root.getChild(0));
  var theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(theTag.title, "bar");

  // Check that tag container contains new title
  theTag.containerOpen = true;
  do_check_eq(theTag.childCount, 1);
  var node = theTag.getChild(0);
  do_check_eq(node.title, "new title 1");
  root.containerOpen = false;

  // Change bookmark 2 title.
  bmsvc.setItemTitle(bookmark2id, "new title 2");

  // Workaround VM timers issues.
  var bookmark1LastMod = bmsvc.getItemLastModified(bookmark1id);
  bmsvc.setItemLastModified(bookmark2id, bookmark1LastMod + 1);

  // Check that tag container contains new title
  options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_CONTENTS;

  query = histsvc.getNewQuery();
  query.setFolders([tagItemId], 1);
  result = histsvc.executeQuery(query, options);
  root = result.root;

  root.containerOpen = true;
  cc = root.childCount;
  do_check_eq(cc, 1);
  node = root.getChild(0);
  do_check_eq(node.title, "new title 2");
  root.containerOpen = false;
}
