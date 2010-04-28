/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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

/*
 * Tests nsNavHistoryContainerResultNode::GetChildIndex(aNode) functionality.
 */

function run_test() {
  // Add a bookmark to the menu.
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                       uri("http://test.mozilla.org/bookmark/"),
                                       Ci.nsINavBookmarksService.DEFAULT_INDEX,
                                       "Test bookmark");

  // Add a bookmark to unfiled folder.
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri("http://test.mozilla.org/unfiled/"),
                                       Ci.nsINavBookmarksService.DEFAULT_INDEX,
                                       "Unfiled bookmark");

  // Get the unfiled bookmark node.
  let unfiledNode = getNodeAt(PlacesUtils.unfiledBookmarksFolderId, 0);
  if (!unfiledNode)
    do_throw("Unable to find bookmark in hierarchy!");
  do_check_eq(unfiledNode.title, "Unfiled bookmark");

  let hs = PlacesUtils.history;
  let query = hs.getNewQuery();
  query.setFolders([PlacesUtils.bookmarksMenuFolderId], 1);
  let options = hs.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = hs.executeQuery(query, options).root;
  root.containerOpen = true;

  // Check functionality for proper nodes.
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    print("Now testing: " + node.title);
    do_check_eq(root.getChildIndex(node), i);
  }

  // Now search for an invalid node and expect an exception.
  try {
    root.getChildIndex(unfiledNode);
    do_throw("Searching for an invalid node should have thrown.");
  } catch(ex) {
    print("We correctly got an exception.");
  }

  root.containerOpen = false;
}

function getNodeAt(aFolderId, aIndex) {
  let hs = PlacesUtils.history;
  let query = hs.getNewQuery();
  query.setFolders([aFolderId], 1);
  let options = hs.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = hs.executeQuery(query, options).root;
  root.containerOpen = true;
  if (root.childCount < aIndex)
    do_throw("Not enough children to find bookmark!");
  let node = root.getChild(aIndex);
  root.containerOpen = false;
  return node;
}
