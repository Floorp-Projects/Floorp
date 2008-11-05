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
 * The Original Code is bug 419792 test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dietrich Ayala <dietrich@mozilla.com> (Original Author)
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

// get services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);
var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
              getService(Ci.nsITaggingService);

function run_test() {
  // get toolbar node
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.toolbarFolder], 1);
  var result = histsvc.executeQuery(query, options);
  var toolbarNode = result.root;
  toolbarNode.containerOpen = true;

  // add a bookmark
  var bookmarkURI = uri("http://foo.com");
  var bookmarkId = bmsvc.insertBookmark(bmsvc.toolbarFolder, bookmarkURI,
                                        bmsvc.DEFAULT_INDEX, "");

  // get the node for the new bookmark
  var node = toolbarNode.getChild(toolbarNode.childCount-1);
  do_check_eq(node.itemId, bookmarkId);

  // confirm there's no tags via the .tags property
  do_check_eq(node.tags, null); 

  // add a tag
  tagssvc.tagURI(bookmarkURI, ["foo"]);
  do_check_eq(node.tags, "foo");

  // add another tag, to test delimiter and sorting
  tagssvc.tagURI(bookmarkURI, ["bar"]);
  do_check_eq(node.tags, "bar, foo");

  // remove the tags, confirming the property is cleared
  tagssvc.untagURI(bookmarkURI, null);
  do_check_eq(node.tags, null); 

  toolbarNode.containerOpen = false;
}
