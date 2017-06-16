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

tests.push({
  populate: function populate() {
    // check initial size
    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;
    do_check_eq(rootNode.childCount, 5);

    // create a test root
    this._folderTitle = "test folder";
    this._folderId =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId,
                                         this._folderTitle,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_check_eq(rootNode.childCount, 6);

    // add a tag
    this._testURI = PlacesUtils._uri("http://test");
    this._tags = ["a", "b"];
    PlacesUtils.tagging.tagURI(this._testURI, this._tags);

    // add a child to each root, including our test root
    this._roots = [PlacesUtils.bookmarksMenuFolderId, PlacesUtils.toolbarFolderId,
                   PlacesUtils.unfiledBookmarksFolderId, PlacesUtils.mobileFolderId,
                   this._folderId];
    this._roots.forEach(function(aRootId) {
      // clean slate
      PlacesUtils.bookmarks.removeFolderChildren(aRootId);
      // add a test bookmark
      PlacesUtils.bookmarks.insertBookmark(aRootId, this._testURI,
                                           PlacesUtils.bookmarks.DEFAULT_INDEX, "test");
    }, this);

    // add a folder to exclude from replacing during restore
    // this will still be present post-restore
    var excludedFolderId =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId,
                                         "excluded",
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_check_eq(rootNode.childCount, 7);

    // add a test bookmark to it
    PlacesUtils.bookmarks.insertBookmark(excludedFolderId, this._testURI,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX, "test");
  },

  inbetween: function inbetween() {
    // add some items that should be removed by the restore

    // add a folder
    this._litterTitle = "otter";
    PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId,
                                       this._litterTitle, 0);

    // add some tags
    PlacesUtils.tagging.tagURI(this._testURI, ["c", "d"]);
  },

  validate: function validate() {
    // validate tags restored
    var tags = PlacesUtils.tagging.getTagsForURI(this._testURI);
    // also validates that litter tags are gone
    do_check_eq(this._tags.toString(), tags.toString());

    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;

    // validate litter is gone
    do_check_neq(rootNode.getChild(0).title, this._litterTitle);

    // test root count is the same
    do_check_eq(rootNode.childCount, 7);

    var foundTestFolder = 0;
    for (var i = 0; i < rootNode.childCount; i++) {
      var node = rootNode.getChild(i);

      do_print("validating " + node.title);
      if (node.itemId != PlacesUtils.tagsFolderId) {
        if (node.title == this._folderTitle) {
          // check the test folder's properties
          do_check_eq(node.type, node.RESULT_TYPE_FOLDER);
          do_check_eq(node.title, this._folderTitle);
          foundTestFolder++;
        }

        // test contents
        node.QueryInterface(Ci.nsINavHistoryContainerResultNode).containerOpen = true;
        do_check_eq(node.childCount, 1);
        var child = node.getChild(0);
        do_check_true(PlacesUtils._uri(child.uri).equals(this._testURI));

        // clean up
        node.containerOpen = false;
      }
    }
    do_check_eq(foundTestFolder, 1);
    rootNode.containerOpen = false;
  }
});

function run_test() {
  run_next_test();
}

add_task(async function() {
  // make json file
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  // populate db
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate();
  });

  await BookmarkJSONUtils.exportToFile(jsonFile);

  tests.forEach(function(aTest) {
    aTest.inbetween();
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
