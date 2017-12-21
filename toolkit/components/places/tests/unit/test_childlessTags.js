/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Ensures that removal of a bookmark untags the bookmark if it's no longer
 * contained in any regular, non-tag folders.  See bug 444849.
 */

 var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
               getService(Ci.nsINavHistoryService);

 var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
               getService(Ci.nsITaggingService);

 const BOOKMARK_URI = uri("http://example.com/");

add_task(async function test_removing_tagged_bookmark_removes_tag() {
  print("  Make a bookmark.");
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: BOOKMARK_URI,
    title: "test bookmark",
  });

  print("  Tag it up.");
  let tags = ["foo", "bar"];
  tagssvc.tagURI(BOOKMARK_URI, tags);
  ensureTagsExist(tags);

  print("  Remove the bookmark.  The tags should no longer exist.");
  await PlacesUtils.bookmarks.remove(bookmark.guid);
  ensureTagsExist([]);
});

add_task(async function test_removing_folder_containing_tagged_bookmark_removes_tag() {
  print("  Make a folder.");
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  print("  Stick a bookmark in the folder.");
  var bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: BOOKMARK_URI,
    title: "test bookmark",
  });

  print("  Tag the bookmark.");
  var tags = ["foo", "bar"];
  tagssvc.tagURI(BOOKMARK_URI, tags);
  ensureTagsExist(tags);

  print("  Remove the folder.  The tags should no longer exist.");
  await PlacesUtils.bookmarks.remove(bookmark.guid);
  ensureTagsExist([]);
});

/**
 * Runs a tag query and ensures that the tags returned are those and only those
 * in aTags.  aTags may be empty, in which case this function ensures that no
 * tags exist.
 *
 * @param aTags
 *        An array of tags (strings)
 */
function ensureTagsExist(aTags) {
  var query = histsvc.getNewQuery();
  var opts = histsvc.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_TAG_QUERY;
  var resultRoot = histsvc.executeQuery(query, opts).root;

  // Dupe aTags.
  var tags = aTags.slice(0);

  resultRoot.containerOpen = true;

  // Ensure that the number of tags returned from the query is the same as the
  // number in |tags|.
  Assert.equal(resultRoot.childCount, tags.length);

  // For each tag result from the query, ensure that it's contained in |tags|.
  // Remove the tag from |tags| so that we ensure the sets are equal.
  for (let i = 0; i < resultRoot.childCount; i++) {
    var tag = resultRoot.getChild(i).title;
    var indexOfTag = tags.indexOf(tag);
    Assert.ok(indexOfTag >= 0);
    tags.splice(indexOfTag, 1);
  }

  resultRoot.containerOpen = false;
}
