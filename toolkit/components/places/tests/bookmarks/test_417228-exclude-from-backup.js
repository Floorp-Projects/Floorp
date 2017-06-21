/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Menu, Toolbar, Unsorted, Tags, Mobile
const PLACES_ROOTS_COUNT  = 5;

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
                                              PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);

    // create a root to be exclude
    this._excludeRootTitle = "exclude root";
    this._excludeRootId = PlacesUtils.bookmarks
                                     .createFolder(PlacesUtils.placesRootId,
                                                   this._excludeRootTitle, idx);
    // Annotate the root for exclusion.
    PlacesUtils.annotations.setItemAnnotation(this._excludeRootId,
                                              PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1, 0,
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
      var excludeRootIndex = PLACES_ROOTS_COUNT + 1;
      var excludeRootNode = rootNode.getChild(excludeRootIndex);
      do_check_eq(this._excludeRootTitle, excludeRootNode.title);
      excludeRootNode.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      excludeRootNode.containerOpen = true;
      do_check_eq(excludeRootNode.childCount, 1);
      var excludeRootChildNode = excludeRootNode.getChild(0);
      do_check_eq(excludeRootChildNode.uri, this._restoreRootExcludeURI.spec);
      excludeRootNode.containerOpen = false;
    } else {
      // exclude root should not exist anymore
      do_check_eq(rootNode.childCount, PLACES_ROOTS_COUNT + 1);
      restoreRootIndex = PLACES_ROOTS_COUNT;
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

// make json file
var jsonFile;

add_task(async function setup() {
  jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");
});

add_task(async function test_export_import_excluded_file() {
  // populate db
  test.populate();

  await BookmarkJSONUtils.exportToFile(jsonFile);

  // restore json file
  do_print("Restoring json file");
  await BookmarkJSONUtils.importFromFile(jsonFile, true);

  // validate without removing all bookmarks
  // restore do not remove backup exclude entries
  do_print("Validating...");
  test.validate(false);
});

add_task(async function test_clearing_then_importing() {
  // cleanup
  await PlacesUtils.bookmarks.eraseEverything();
  // manually remove the excluded root
  PlacesUtils.bookmarks.removeItem(test._excludeRootId);
  // restore json file
  await BookmarkJSONUtils.importFromFile(jsonFile, true);

  // validate after a complete bookmarks cleanup
  test.validate(true);

  // clean up
  await OS.File.remove(jsonFile);
});
