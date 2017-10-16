/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [];

/*

Backup/restore tests example:

var myTest = {
  populate: function () { ... add bookmarks ... },
  validate: function () { ... query for your bookmarks ... }
}

this.push(myTest);

*/

var quotesTest = {
  _folderTitle: '"quoted folder"',
  _folderId: null,

  populate() {
    this._folderId =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                         this._folderTitle,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
  },

  clean() {
    PlacesUtils.bookmarks.removeItem(this._folderId);
  },

  validate() {
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());

    var toolbar = result.root;
    toolbar.containerOpen = true;

    // test for our quoted folder
    do_check_true(toolbar.childCount, 1);
    var folderNode = toolbar.getChild(0);
    do_check_eq(folderNode.type, folderNode.RESULT_TYPE_FOLDER);
    do_check_eq(folderNode.title, this._folderTitle);

    // clean up
    toolbar.containerOpen = false;
  }
};
tests.push(quotesTest);

add_task(async function() {
  // make json file
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // populate db
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate();
  });

  // export json to file
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
