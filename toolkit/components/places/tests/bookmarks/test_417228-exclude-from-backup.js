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

    var idx = PlacesUtils.bookmarks.DEFAULT_INDEX;

    var testRoot =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId,
                                         "", idx);

    // create a folder to be restored
    this._folderTitle1 = "test folder";
    this._folderId1 = 
      PlacesUtils.bookmarks.createFolder(testRoot, this._folderTitle1, idx);

    // add a test bookmark
    this._testURI = uri("http://test");
    PlacesUtils.bookmarks.insertBookmark(this._folderId1, this._testURI,
                                         idx, "test");

    // create a folder to be excluded 
    this._folderTitle2 = "test folder";
    this._folderId2 = 
      PlacesUtils.bookmarks.createFolder(testRoot, this._folderTitle2, idx);

    // add a test bookmark
    PlacesUtils.bookmarks.insertBookmark(this._folderId2, this._testURI,
                                         idx, "test");

    rootNode.containerOpen = false;
  },

  validate: function validate() {
    var rootNode = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                 false, false).root;

    var testRootNode = rootNode.getChild(rootNode.childCount-1);

    testRootNode.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    testRootNode.containerOpen = true;
    do_check_eq(testRootNode.childCount, 1);

    var childNode = testRootNode.getChild(0);
    do_check_eq(childNode.title, this._folderTitle1);

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
    PlacesUtils.backupBookmarksToFile(jsonFile, [test._folderId2]);
  } catch(ex) {
    do_throw("couldn't export to file: " + ex);
  }

  // restore json file
  try {
    PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
  } catch(ex) {
    do_throw("couldn't import the exported file: " + ex);
  }

  // validate
  test.validate();

  // clean up
  jsonFile.remove(false);
}
