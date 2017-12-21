/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This test is:
    - don't block while doing backup and restore if tag containers contain
      bogus items (separators, folders)
*/

const ITEM_TITLE = "invalid uri";
const ITEM_URL = "http://test.mozilla.org/";
const TAG_NAME = "testTag";

function validateResults() {
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
  var options = PlacesUtils.history.getNewQueryOptions();
  var result = PlacesUtils.history.executeQuery(query, options);

  var toolbar = result.root;
  toolbar.containerOpen = true;

  // test for our bookmark
  Assert.equal(toolbar.childCount, 1);
  for (var i = 0; i < toolbar.childCount; i++) {
    var folderNode = toolbar.getChild(0);
    Assert.equal(folderNode.type, folderNode.RESULT_TYPE_URI);
    Assert.equal(folderNode.title, ITEM_TITLE);
  }
  toolbar.containerOpen = false;

  // test for our tag
  var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(ITEM_URL));
  Assert.equal(tags.length, 1);
  Assert.equal(tags[0], TAG_NAME);
}

add_task(async function() {
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // add a valid bookmark
  let item = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: ITEM_TITLE,
    url: ITEM_URL,
  });

  // create a tag
  PlacesUtils.tagging.tagURI(PlacesUtils._uri(ITEM_URL), [TAG_NAME]);
  // get tag folder id
  var options = PlacesUtils.history.getNewQueryOptions();
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.tagsFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, options);
  var tagRoot = result.root;
  tagRoot.containerOpen = true;
  Assert.equal(tagRoot.childCount, 1);
  var tagNode = tagRoot.getChild(0)
                        .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  let tagItemId = tagNode.itemId;
  tagRoot.containerOpen = false;

  // Currently these use the old API as the new API doesn't support inserting
  // invalid items into the tag folder.

  // add a separator and a folder inside tag folder
  PlacesUtils.bookmarks.insertSeparator(tagItemId,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.createFolder(tagItemId,
                                     "test folder",
                                     PlacesUtils.bookmarks.DEFAULT_INDEX);

  // add a separator and a folder inside tag root
  PlacesUtils.bookmarks.insertSeparator(PlacesUtils.bookmarks.tagsFolder,
                                        PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.createFolder(PlacesUtils.bookmarks.tagsFolder,
                                     "test tags root folder",
                                     PlacesUtils.bookmarks.DEFAULT_INDEX);
  // sanity
  validateResults();

  await BookmarkJSONUtils.exportToFile(jsonFile);

  // clean
  PlacesUtils.tagging.untagURI(PlacesUtils._uri(ITEM_URL), [TAG_NAME]);
  await PlacesUtils.bookmarks.remove(item);

  // restore json file
  await BookmarkJSONUtils.importFromFile(jsonFile, true);

  validateResults();

  // clean up
  await OS.File.remove(jsonFile);
});
