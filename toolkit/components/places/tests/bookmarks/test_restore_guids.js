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
 *  Dietrich Ayala <dietrich@mozilla.com> (Original Author)
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

const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

var test = {
  _testBookmarkId: null,
  _testBookmarkGUID: null, 

  populate: function populate() {
    var bookmarkURI = uri("http://bookmark");
    this._testBookmarkId =
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId, bookmarkURI,
                                           DEFAULT_INDEX, "bookmark");
    this._testBookmarkGUID = PlacesUtils.bookmarks.getItemGUID(this._testBookmarkId);
  },

  clean: function () {},

  validate: function validate() {
    var guid = PlacesUtils.bookmarks.getItemGUID(this._testBookmarkId);
    do_check_eq(this._testBookmarkGUID, guid);
  }
}
tests.push(test);

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
  tests.forEach(function(aTest) {
    aTest.populate();
    // sanity
    aTest.validate();
  });

  // export json to file
  try {
    PlacesUtils.backupBookmarksToFile(jsonFile);
  } catch(ex) { do_throw("couldn't export to file: " + ex); }

  // clean
  tests.forEach(function(aTest) {
    aTest.clean();
  });

  // restore json file
  try {
    PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
  } catch(ex) { do_throw("couldn't import the exported file: " + ex); }

  // validate
  tests.forEach(function(aTest) {
    aTest.validate();
  });

  // clean up
  jsonFile.remove(false);
}
