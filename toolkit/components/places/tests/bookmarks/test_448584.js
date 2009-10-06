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
 * The Original Code is Bug 448584 code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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

// Get database connection
try {
  var mDBConn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                   .DBConnection;
}
catch(ex) {
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
    aTest.validate(2);
    // Something in the code went wrong and we finish up losing the place, so
    // the bookmark uri becomes null.
    var sql = "UPDATE moz_bookmarks SET fk = 1337 WHERE id = ?1";
    var stmt = mDBConn.createStatement(sql);
    stmt.bindUTF8StringParameter(0, aTest._itemId);
    try {
      stmt.execute();
    } finally {
      stmt.finalize();
    }
  });

  // export json to file
  try {
    PlacesUtils.backups.saveBookmarksToJSONFile(jsonFile);
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
    aTest.validate(1);
  });

  // clean up
  jsonFile.remove(false);
}
