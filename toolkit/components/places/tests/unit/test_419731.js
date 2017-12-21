/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_tag_updates() {
  const url = "http://foo.bar/";
  let lastModified = new Date(Date.now() - 10000);

  // create 2 bookmarks
  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    dateAdded: lastModified,
    lastModified: new Date(),
    title: "title 1",
    url,
  });
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    dateAdded: lastModified,
    lastModified,
    title: "title 2",
    url,
  });

  // add a new tag
  PlacesUtils.tagging.tagURI(uri(url), ["foo"]);

  // get tag folder id
  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.tagsFolderId], 1);
  let result = PlacesUtils.history.executeQuery(query, options);
  let tagRoot = result.root;
  tagRoot.containerOpen = true;
  let tagNode = tagRoot.getChild(0)
                       .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  let tagItemGuid = tagNode.bookmarkGuid;
  let tagItemId = tagNode.itemId;
  tagRoot.containerOpen = false;

  // change bookmark 1 title
  await PlacesUtils.bookmarks.update({
    guid: bm1.guid,
    title: "new title 1",
  });

  // Workaround timers resolution and time skews.
  bm2 = await PlacesUtils.bookmarks.fetch(bm2.guid);

  lastModified = new Date(lastModified.getTime() + 1000);

  await PlacesUtils.bookmarks.update({
    guid: bm1.guid,
    lastModified,
  });

  // Query the tag.
  options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_QUERY;

  query = PlacesUtils.history.getNewQuery();
  result = PlacesUtils.history.executeQuery(query, options);
  let root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);

  let theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  // Bug 524219: Check that renaming the tag shows up in the result.
  Assert.equal(theTag.title, "foo");

  await PlacesUtils.bookmarks.update({
    guid: tagItemGuid,
    title: "bar",
  });

  // Check that the item has been replaced
  Assert.notEqual(theTag, root.getChild(0));
  theTag = root.getChild(0)
                   .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(theTag.title, "bar");

  // Check that tag container contains new title
  theTag.containerOpen = true;
  Assert.equal(theTag.childCount, 1);
  let node = theTag.getChild(0);
  Assert.equal(node.title, "new title 1");
  theTag.containerOpen = false;
  root.containerOpen = false;

  // Change bookmark 2 title.
  PlacesUtils.bookmarks.update({
    guid: bm2.guid,
    title: "new title 2"
  });

  // Workaround timers resolution and time skews.
  lastModified = new Date(lastModified.getTime() + 1000);

  await PlacesUtils.bookmarks.update({
    guid: bm2.guid,
    lastModified,
  });

  // Check that tag container contains new title
  options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.resultType = options.RESULTS_AS_TAG_CONTENTS;

  query = PlacesUtils.history.getNewQuery();
  query.setFolders([tagItemId], 1);
  result = PlacesUtils.history.executeQuery(query, options);
  root = result.root;

  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  node = root.getChild(0);
  Assert.equal(node.title, "new title 2");
  root.containerOpen = false;
});
