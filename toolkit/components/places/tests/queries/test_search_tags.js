/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_search_for_tagged_bookmarks() {
  const testURI = "http://a1.com";

  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "bug 395101 test",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title: "1 title",
    url: testURI,
  });

  // tag the bookmarked URI
  PlacesUtils.tagging.tagURI(uri(testURI), [
    "elephant",
    "walrus",
    "giraffe",
    "turkey",
    "hiPPo",
    "BABOON",
    "alf",
  ]);

  // search for the bookmark, using a tag
  var query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "elephant";
  var options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  query.setParents([folder.guid]);

  var result = PlacesUtils.history.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  Assert.equal(rootNode.childCount, 1);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, bookmark.guid);
  rootNode.containerOpen = false;

  // partial matches are okay
  query.searchTerms = "wal";
  result = PlacesUtils.history.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 1);
  rootNode.containerOpen = false;

  // case insensitive search term
  query.searchTerms = "WALRUS";
  result = PlacesUtils.history.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 1);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, bookmark.guid);
  rootNode.containerOpen = false;

  // case insensitive tag
  query.searchTerms = "baboon";
  result = PlacesUtils.history.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 1);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, bookmark.guid);
  rootNode.containerOpen = false;
});
