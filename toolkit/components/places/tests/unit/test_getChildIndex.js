/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests nsNavHistoryContainerResultNode::GetChildIndex(aNode) functionality.
 */

add_task(async function test_get_child_index() {
  // Add a bookmark to the menu.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://test.mozilla.org/bookmark/",
    title: "Test bookmark"
  });

  // Add a bookmark to unfiled folder.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test.mozilla.org/unfiled/",
    title: "Unfiled bookmark"
  });

  // Get the unfiled bookmark node.
  let unfiledNode = getNodeAt(PlacesUtils.bookmarks.unfiledGuid, 0);
  if (!unfiledNode)
    do_throw("Unable to find bookmark in hierarchy!");
  Assert.equal(unfiledNode.title, "Unfiled bookmark");

  let hs = PlacesUtils.history;
  let query = hs.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.menuGuid], 1);
  let options = hs.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = hs.executeQuery(query, options).root;
  root.containerOpen = true;

  // Check functionality for proper nodes.
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    print("Now testing: " + node.title);
    Assert.equal(root.getChildIndex(node), i);
  }

  // Now search for an invalid node and expect an exception.
  try {
    root.getChildIndex(unfiledNode);
    do_throw("Searching for an invalid node should have thrown.");
  } catch (ex) {
    print("We correctly got an exception.");
  }

  root.containerOpen = false;
});

function getNodeAt(aFolderGuid, aIndex) {
  let hs = PlacesUtils.history;
  let query = hs.getNewQuery();
  query.setParents([aFolderGuid], 1);
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
