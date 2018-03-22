/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Obtains the id of the folder obtained from the query.
 *
 * @param aQuery
 *        The query to obtain the folder id from.
 * @returns the folder id of the folder of the root node of the query.
 */
function folder_id(aQuery) {
  info("Checking query '" + aQuery + "'\n");
  let query = {}, options = {};
  PlacesUtils.history.queryStringToQuery(aQuery, query, options);
  var result = PlacesUtils.history.executeQuery(query.value, options.value);
  var root = result.root;
  root.containerOpen = true;
  Assert.ok(root.hasChildren);
  var folderID = root.getChild(0).parent.itemId;
  root.containerOpen = false;
  return folderID;
}

add_task(async function test_history_string_to_query() {
  const QUERIES = [
      "place:folder=PLACES_ROOT",
      "place:folder=BOOKMARKS_MENU",
      "place:folder=TAGS",
      "place:folder=UNFILED_BOOKMARKS",
      "place:folder=TOOLBAR"
  ];
  const FOLDER_IDS = [
    PlacesUtils.placesRootId,
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.tagsFolderId,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.toolbarFolderId,
  ];

  // add something in the bookmarks menu folder so a query to it returns results
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "bmf",
    url: "http://example.com/bmf/",
  });

  // add something to the tags folder
  PlacesUtils.tagging.tagURI(uri("http://www.example.com/"), ["tag"]);

  // add something to the unfiled bookmarks folder
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "ubf",
    url: "http://example.com/ubf/",
  });

  // add something to the toolbar folder
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "tf",
    url: "http://example.com/tf/",
  });

  for (let i = 0; i < QUERIES.length; i++) {
    let result = folder_id(QUERIES[i]);
    Assert.equal(FOLDER_IDS[i], result);
  }
});
