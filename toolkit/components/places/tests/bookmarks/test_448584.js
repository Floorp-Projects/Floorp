/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get database connection
try {
  var mDBConn = PlacesUtils.history.DBConnection;
} catch (ex) {
  do_throw("Could not get database connection\n");
}

/*
  This test is:
    - don't try to add invalid uri nodes to a JSON backup
*/

const ITEM_TITLE = "invalid uri";
const ITEM_URL = "http://test.mozilla.org";

function validateResults(expectedValidItemsCount) {
  var query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  var options = PlacesUtils.history.getNewQueryOptions();
  var result = PlacesUtils.history.executeQuery(query, options);

  var toolbar = result.root;
  toolbar.containerOpen = true;

  // test for our bookmark
  Assert.equal(toolbar.childCount, expectedValidItemsCount);
  for (var i = 0; i < toolbar.childCount; i++) {
    var folderNode = toolbar.getChild(0);
    Assert.equal(folderNode.type, folderNode.RESULT_TYPE_URI);
    Assert.equal(folderNode.title, ITEM_TITLE);
  }

  // clean up
  toolbar.containerOpen = false;
}

add_task(async function() {
  // make json file
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // populate db
  // add a valid bookmark
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: ITEM_TITLE,
    url: ITEM_URL,
  });

  let badBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: ITEM_TITLE,
    url: ITEM_URL,
  });
  // sanity
  validateResults(2);
  // Something in the code went wrong and we finish up losing the place, so
  // the bookmark uri becomes null.
  var sql = "UPDATE moz_bookmarks SET fk = 1337 WHERE guid = ?1";
  var stmt = mDBConn.createStatement(sql);
  stmt.bindByIndex(0, badBookmark.guid);
  try {
    stmt.execute();
  } finally {
    stmt.finalize();
  }

  await BookmarkJSONUtils.exportToFile(jsonFile);

  // clean
  await PlacesUtils.bookmarks.remove(badBookmark);

  // restore json file
  try {
    await BookmarkJSONUtils.importFromFile(jsonFile, { replace: true });
  } catch (ex) { do_throw("couldn't import the exported file: " + ex); }

  // validate
  validateResults(1);

  // clean up
  await OS.File.remove(jsonFile);
});
