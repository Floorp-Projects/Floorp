/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [];

/*
  This test is:
    - don't block while doing backup and restore if tag containers contain
      bogus items (separators, folders)
*/

var invalidTagChildTest = {
  _itemTitle: "invalid uri",
  _itemUrl: "http://test.mozilla.org/",
  _itemId: -1,
  _tag: "testTag",
  _tagItemId: -1,

  populate() {
    // add a valid bookmark
    this._itemId = PlacesUtils.bookmarks
                              .insertBookmark(PlacesUtils.toolbarFolderId,
                                              PlacesUtils._uri(this._itemUrl),
                                              PlacesUtils.bookmarks.DEFAULT_INDEX,
                                              this._itemTitle);

    // create a tag
    PlacesUtils.tagging.tagURI(PlacesUtils._uri(this._itemUrl), [this._tag]);
    // get tag folder id
    var options = PlacesUtils.history.getNewQueryOptions();
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.tagsFolder], 1);
    var result = PlacesUtils.history.executeQuery(query, options);
    var tagRoot = result.root;
    tagRoot.containerOpen = true;
    do_check_eq(tagRoot.childCount, 1);
    var tagNode = tagRoot.getChild(0)
                          .QueryInterface(Ci.nsINavHistoryContainerResultNode);
    this._tagItemId = tagNode.itemId;
    tagRoot.containerOpen = false;

    // add a separator and a folder inside tag folder
    PlacesUtils.bookmarks.insertSeparator(this._tagItemId,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    PlacesUtils.bookmarks.createFolder(this._tagItemId,
                                       "test folder",
                                       PlacesUtils.bookmarks.DEFAULT_INDEX);

    // add a separator and a folder inside tag root
    PlacesUtils.bookmarks.insertSeparator(PlacesUtils.bookmarks.tagsFolder,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX);
    PlacesUtils.bookmarks.createFolder(PlacesUtils.bookmarks.tagsFolder,
                                       "test tags root folder",
                                       PlacesUtils.bookmarks.DEFAULT_INDEX);
  },

  clean() {
    PlacesUtils.tagging.untagURI(PlacesUtils._uri(this._itemUrl), [this._tag]);
    PlacesUtils.bookmarks.removeItem(this._itemId);
  },

  validate() {
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    var options = PlacesUtils.history.getNewQueryOptions();
    var result = PlacesUtils.history.executeQuery(query, options);

    var toolbar = result.root;
    toolbar.containerOpen = true;

    // test for our bookmark
    do_check_eq(toolbar.childCount, 1);
    for (var i = 0; i < toolbar.childCount; i++) {
      var folderNode = toolbar.getChild(0);
      do_check_eq(folderNode.type, folderNode.RESULT_TYPE_URI);
      do_check_eq(folderNode.title, this._itemTitle);
    }
    toolbar.containerOpen = false;

    // test for our tag
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(this._itemUrl));
    do_check_eq(tags.length, 1);
    do_check_eq(tags[0], this._tag);
  }
}
tests.push(invalidTagChildTest);

add_task(async function() {
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // populate db
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate();
  });

  await BookmarkJSONUtils.exportToFile(jsonFile);

  // clean
  tests.forEach(function(aTest) {
    aTest.clean();
  });

  // restore json file
  await BookmarkJSONUtils.importFromFile(jsonFile, true);

  // validate
  tests.forEach(function(aTest) {
    aTest.validate();
  });

  // clean up
  await OS.File.remove(jsonFile);
});
