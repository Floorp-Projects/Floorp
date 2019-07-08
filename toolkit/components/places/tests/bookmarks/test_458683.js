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
  let toolbar = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.toolbarGuid)
    .root;
  // test for our bookmark
  Assert.equal(toolbar.childCount, 1);
  for (var i = 0; i < toolbar.childCount; i++) {
    var folderNode = toolbar.getChild(0);
    Assert.equal(folderNode.type, folderNode.RESULT_TYPE_URI);
    Assert.equal(folderNode.title, ITEM_TITLE);
  }
  toolbar.containerOpen = false;

  // test for our tag
  var tags = PlacesUtils.tagging.getTagsForURI(Services.io.newURI(ITEM_URL));
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
  PlacesUtils.tagging.tagURI(Services.io.newURI(ITEM_URL), [TAG_NAME]);
  // get tag folder id
  let tagRoot = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.tagsGuid)
    .root;
  Assert.equal(tagRoot.childCount, 1);
  let tagItemGuid = PlacesUtils.asContainer(tagRoot.getChild(0)).bookmarkGuid;
  tagRoot.containerOpen = false;

  function insert({ type, parentGuid }) {
    return PlacesUtils.withConnectionWrapper(
      "test_458683: insert",
      async db => {
        await db.executeCached(
          `INSERT INTO moz_bookmarks (type, parent, position, guid)
         VALUES (:type,
                 (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
                 (SELECT MAX(position) + 1 FROM moz_bookmarks WHERE parent = (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid)),
                 GENERATE_GUID())`,
          { type, parentGuid }
        );
      }
    );
  }

  // add a separator and a folder inside tag folder
  // We must insert these manually, because the new bookmarking API doesn't
  // support inserting invalid items into the tag folder.
  await insert({
    parentGuid: tagItemGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
  await insert({
    parentGuid: tagItemGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  // add a separator and a folder inside tag root
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "test tags root folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  // sanity
  validateResults();

  await BookmarkJSONUtils.exportToFile(jsonFile);

  // clean
  PlacesUtils.tagging.untagURI(Services.io.newURI(ITEM_URL), [TAG_NAME]);
  await PlacesUtils.bookmarks.remove(item);

  // restore json file
  await BookmarkJSONUtils.importFromFile(jsonFile, { replace: true });

  validateResults();

  // clean up
  await OS.File.remove(jsonFile);
});
