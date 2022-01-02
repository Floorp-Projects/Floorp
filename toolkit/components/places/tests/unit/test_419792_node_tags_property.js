/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// get services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
  Ci.nsINavHistoryService
);
var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].getService(
  Ci.nsITaggingService
);

add_task(async function test_query_node_tags_property() {
  // get toolbar node
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  var result = histsvc.executeQuery(query, options);
  var toolbarNode = result.root;
  toolbarNode.containerOpen = true;

  // add a bookmark
  var bookmarkURI = uri("http://foo.com");
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "",
    url: bookmarkURI,
  });

  // get the node for the new bookmark
  var node = toolbarNode.getChild(toolbarNode.childCount - 1);
  Assert.equal(node.bookmarkGuid, bookmark.guid);

  // confirm there's no tags via the .tags property
  Assert.equal(node.tags, null);

  // add a tag
  tagssvc.tagURI(bookmarkURI, ["foo"]);
  Assert.equal(node.tags, "foo");

  // add another tag, to test delimiter and sorting
  tagssvc.tagURI(bookmarkURI, ["bar"]);
  Assert.equal(node.tags, "bar, foo");

  // remove the tags, confirming the property is cleared
  tagssvc.untagURI(bookmarkURI, null);
  Assert.equal(node.tags, null);

  toolbarNode.containerOpen = false;
});
