/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
