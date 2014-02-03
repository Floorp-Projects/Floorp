/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let uri1 = NetUtil.newURI("http://foo.bar/");

  // create 2 bookmarks
  let bookmark1id = PlacesUtils.bookmarks
                               .insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                               uri1,
                                               PlacesUtils.bookmarks.DEFAULT_INDEX,
                                               "title 1");
  let bookmark2id = PlacesUtils.bookmarks
                               .insertBookmark(PlacesUtils.toolbarFolderId,
                                               uri1,
                                               PlacesUtils.bookmarks.DEFAULT_INDEX,
                                               "title 2");
  // add a new tag
  PlacesUtils.tagging.tagURI(uri1, ["foo"]);

  // get tag folder id
  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.tagsFolderId], 1);
  let result = PlacesUtils.history.executeQuery(query, options);
  let tagRoot = result.root;
  tagRoot.containerOpen = true;
  let tagNode = tagRoot.getChild(0)
                       .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  let tagItemId = tagNode.itemId;
  tagRoot.containerOpen = false;

  // change bookmark 1 title
  PlacesUtils.bookmarks.setItemTitle(bookmark1id, "new title 1");

  // Workaround timers resolution and time skews.
  let bookmark2LastMod = PlacesUtils.bookmarks.getItemLastModified(bookmark2id);
  PlacesUtils.bookmarks.setItemLastModified(bookmark1id, bookmark2LastMod + 1);

  // Query the tag.
  options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_QUERY;

  query = PlacesUtils.history.getNewQuery();
  result = PlacesUtils.history.executeQuery(query, options);
  let root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 1);

  let theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  // Bug 524219: Check that renaming the tag shows up in the result.
  do_check_eq(theTag.title, "foo")
  PlacesUtils.bookmarks.setItemTitle(tagItemId, "bar");

  // Check that the item has been replaced
  do_check_neq(theTag, root.getChild(0));
  theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(theTag.title, "bar");

  // Check that tag container contains new title
  theTag.containerOpen = true;
  do_check_eq(theTag.childCount, 1);
  let node = theTag.getChild(0);
  do_check_eq(node.title, "new title 1");
  theTag.containerOpen = false;
  root.containerOpen = false;

  // Change bookmark 2 title.
  PlacesUtils.bookmarks.setItemTitle(bookmark2id, "new title 2");

  // Workaround timers resolution and time skews.
  let bookmark1LastMod = PlacesUtils.bookmarks.getItemLastModified(bookmark1id);
  PlacesUtils.bookmarks.setItemLastModified(bookmark2id, bookmark1LastMod + 1);

  // Check that tag container contains new title
  options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_CONTENTS;

  query = PlacesUtils.history.getNewQuery();
  query.setFolders([tagItemId], 1);
  result = PlacesUtils.history.executeQuery(query, options);
  root = result.root;

  root.containerOpen = true;
  do_check_eq(root.childCount, 1);
  node = root.getChild(0);
  do_check_eq(node.title, "new title 2");
  root.containerOpen = false;
}
