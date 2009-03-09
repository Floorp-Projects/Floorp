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
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Marco Bonardo <mak77@bonardo.net>
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

Components.utils.import("resource://gre/modules/utils.js");

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
  do_check_eq(typeof PlacesUtils, "object");

  // make json file
  var jsonFile = dirSvc.get("ProfD", Ci.nsILocalFile);
  jsonFile.append("bookmarks.json");
  if (jsonFile.exists())
    jsonFile.remove(false);
  jsonFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!jsonFile.exists())
    do_throw("couldn't create file: bookmarks.exported.json");

  // populate db
  test.populate();

  try {
    PlacesUtils.backupBookmarksToFile(jsonFile);
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
}
