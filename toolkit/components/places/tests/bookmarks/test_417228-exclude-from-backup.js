/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXCLUDE_FROM_BACKUP_ANNO = "places/excludeFromBackup";
// Menu, Toolbar, Unsorted, Tags
const PLACES_ROOTS_COUNT  = 4;
var tests = [];

/*

Backup/restore tests example:

var myTest = {
  populate: function () { ... add bookmarks ... },
  validate: function () { ... query for your bookmarks ... }
}

this.push(myTest);

*/

var test = {
  populate: function populate() {
    // check initial size
    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;
    do_check_eq(rootNode.childCount, PLACES_ROOTS_COUNT );
    rootNode.containerOpen = false;

    var idx = PlacesUtils.bookmarks.DEFAULT_INDEX;

    // create a root to be restore
    this._restoreRootTitle = "restore root";
    var restoreRootId = PlacesUtils.bookmarks
                                   .createFolder(PlacesUtils.placesRootId,
                                                 this._restoreRootTitle, idx);
    // add a test bookmark
    this._restoreRootURI = uri("http://restore.uri");
    PlacesUtils.bookmarks.insertBookmark(restoreRootId, this._restoreRootURI,
                                         idx, "restore uri");
    // add a test bookmark to be exclude
    this._restoreRootExcludeURI = uri("http://exclude.uri");
    var exItemId = PlacesUtils.bookmarks
                              .insertBookmark(restoreRootId,
                                              this._restoreRootExcludeURI,
                                              idx, "exclude uri");
    // Annotate the bookmark for exclusion.
    PlacesUtils.annotations.setItemAnnotation(exItemId,
                                              EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);

    // create a root to be exclude 
    this._excludeRootTitle = "exclude root";
    this._excludeRootId = PlacesUtils.bookmarks
                                     .createFolder(PlacesUtils.placesRootId,
                                                   this._excludeRootTitle, idx);
    // Annotate the root for exclusion.
    PlacesUtils.annotations.setItemAnnotation(this._excludeRootId,
                                              EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);
    // add a test bookmark exclude by exclusion of its parent
    PlacesUtils.bookmarks.insertBookmark(this._excludeRootId,
                                         this._restoreRootExcludeURI,
                                         idx, "exclude uri");
  },

  validate: function validate(aEmptyBookmarks) {
    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;

    if (!aEmptyBookmarks) {
      // since restore does not remove backup exclude items both
      // roots should still exist.
      do_check_eq(rootNode.childCount, PLACES_ROOTS_COUNT + 2);
      // open exclude root and check it still contains one item
      var restoreRootIndex = PLACES_ROOTS_COUNT;
      var excludeRootIndex = PLACES_ROOTS_COUNT+1;
      var excludeRootNode = rootNode.getChild(excludeRootIndex);
      do_check_eq(this._excludeRootTitle, excludeRootNode.title);
      excludeRootNode.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      excludeRootNode.containerOpen = true;
      do_check_eq(excludeRootNode.childCount, 1);
      var excludeRootChildNode = excludeRootNode.getChild(0);
      do_check_eq(excludeRootChildNode.uri, this._restoreRootExcludeURI.spec);
      excludeRootNode.containerOpen = false;
    }
    else {
      // exclude root should not exist anymore
      do_check_eq(rootNode.childCount, PLACES_ROOTS_COUNT + 1);
      var restoreRootIndex = PLACES_ROOTS_COUNT;
    }

    var restoreRootNode = rootNode.getChild(restoreRootIndex);
    do_check_eq(this._restoreRootTitle, restoreRootNode.title);
    restoreRootNode.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    restoreRootNode.containerOpen = true;
    do_check_eq(restoreRootNode.childCount, 1);
    var restoreRootChildNode = restoreRootNode.getChild(0);
    do_check_eq(restoreRootChildNode.uri, this._restoreRootURI.spec);
    restoreRootNode.containerOpen = false;

    rootNode.containerOpen = false;
  }
}

function run_test() {
  do_test_pending();

  do_check_eq(typeof PlacesUtils, "object");

  // make json file
  var jsonFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  jsonFile.append("bookmarks.json");
  if (jsonFile.exists())
    jsonFile.remove(false);
  jsonFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!jsonFile.exists())
    do_throw("couldn't create file: bookmarks.exported.json");

  // populate db
  test.populate();

  Task.spawn(function() {
    try {
      yield BookmarkJSONUtils.exportToFile(jsonFile);
    } catch(ex) {
      do_throw("couldn't export to file: " + ex);
    }

    // restore json file
    try {
      PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
    } catch(ex) {
      do_throw("couldn't import the exported file: " + ex);
    }

    // validate without removing all bookmarks
    // restore do not remove backup exclude entries
    test.validate(false);

    // cleanup
    remove_all_bookmarks();
    // manually remove the excluded root
    PlacesUtils.bookmarks.removeItem(test._excludeRootId);
    // restore json file
    try {
      PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
    } catch(ex) {
      do_throw("couldn't import the exported file: " + ex);
    }

    // validate after a complete bookmarks cleanup
    test.validate(true);

    // clean up
    jsonFile.remove(false);

    do_test_finished();
  });
}
