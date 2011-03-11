/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bug 384370 code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  excludeItemsFromRestore: [],
  populate: function populate() {
    // check initial size
    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;
    do_check_eq(rootNode.childCount, 4);

    // create a test root
    this._folderTitle = "test folder";
    this._folderId = 
      PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId,
                                         this._folderTitle,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_check_eq(rootNode.childCount, 5);

    // add a tag
    this._testURI = PlacesUtils._uri("http://test");
    this._tags = ["a", "b"];
    PlacesUtils.tagging.tagURI(this._testURI, this._tags);

    // add a child to each root, including our test root
    this._roots = [PlacesUtils.bookmarksMenuFolderId, PlacesUtils.toolbarFolderId,
                   PlacesUtils.unfiledBookmarksFolderId, this._folderId];
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
    do_check_eq(rootNode.childCount, 6);
    this.excludeItemsFromRestore.push(excludedFolderId); 

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
    do_check_eq(rootNode.childCount, 6);

    var foundTestFolder = 0;
    for (var i = 0; i < rootNode.childCount; i++) {
      var node = rootNode.getChild(i);

      LOG("validating " + node.title);
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
  do_check_eq(typeof PlacesUtils, "object");

  // make json file
  var jsonFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  jsonFile.append("bookmarks.json");
  if (jsonFile.exists())
    jsonFile.remove(false);
  jsonFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!jsonFile.exists())
    do_throw("couldn't create file: bookmarks.exported.json");

  // array of ids not to delete when restoring
  var excludedItemsFromRestore = [];

  // populate db
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate();

    if (aTest.excludedItemsFromRestore)
      excludedItemsFromRestore = excludedItems.concat(aTest.excludedItemsFromRestore);
  });

  try {
    PlacesUtils.backups.saveBookmarksToJSONFile(jsonFile);
  } catch(ex) { do_throw("couldn't export to file: " + ex); }

  tests.forEach(function(aTest) {
    aTest.inbetween();
  });

  // restore json file
  try {
    PlacesUtils.restoreBookmarksFromJSONFile(jsonFile, excludedItemsFromRestore);
  } catch(ex) { do_throw("couldn't import the exported file: " + ex); }

  // validate
  tests.forEach(function(aTest) {
    aTest.validate();
  });

  // clean up
  jsonFile.remove(false);
}
