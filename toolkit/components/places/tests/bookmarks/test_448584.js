/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tests = [];

// Get database connection
try {
  var mDBConn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                   .DBConnection;
}
catch (ex) {
  do_throw("Could not get database connection\n");
}

/*
  This test is:
    - don't try to add invalid uri nodes to a JSON backup
*/

var invalidURITest = {
  _itemTitle: "invalid uri",
  _itemUrl: "http://test.mozilla.org/",
  _itemId: null,

  populate: function () {
    // add a valid bookmark
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         PlacesUtils._uri(this._itemUrl),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         this._itemTitle);
    // this bookmark will go corrupt
    this._itemId =
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                           PlacesUtils._uri(this._itemUrl),
                                           PlacesUtils.bookmarks.DEFAULT_INDEX,
                                           this._itemTitle);
  },

  clean: function () {
    PlacesUtils.bookmarks.removeItem(this._itemId);
  },

  validate: function (aExpectValidItemsCount) {
    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
    var options = PlacesUtils.history.getNewQueryOptions();
    var result = PlacesUtils.history.executeQuery(query, options);

    var toolbar = result.root;
    toolbar.containerOpen = true;

    // test for our bookmark
    do_check_eq(toolbar.childCount, aExpectValidItemsCount);
    for (var i = 0; i < toolbar.childCount; i++) {
      var folderNode = toolbar.getChild(0);
      do_check_eq(folderNode.type, folderNode.RESULT_TYPE_URI);
      do_check_eq(folderNode.title, this._itemTitle);
    }

    // clean up
    toolbar.containerOpen = false;
  }
}
tests.push(invalidURITest);

function run_test() {
  run_next_test();
}

add_task(function*() {
  // make json file
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // populate db
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate(2);
    // Something in the code went wrong and we finish up losing the place, so
    // the bookmark uri becomes null.
    var sql = "UPDATE moz_bookmarks SET fk = 1337 WHERE id = ?1";
    var stmt = mDBConn.createStatement(sql);
    stmt.bindByIndex(0, aTest._itemId);
    try {
      stmt.execute();
    } finally {
      stmt.finalize();
    }
  });

  yield BookmarkJSONUtils.exportToFile(jsonFile);

  // clean
  tests.forEach(function(aTest) {
    aTest.clean();
  });

  // restore json file
  try {
    yield BookmarkJSONUtils.importFromFile(jsonFile, true);
  } catch (ex) { do_throw("couldn't import the exported file: " + ex); }

  // validate
  tests.forEach(function(aTest) {
    aTest.validate(1);
  });

  // clean up
  yield OS.File.remove(jsonFile);
});
