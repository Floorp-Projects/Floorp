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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Clint Talbert <ctalbert@mozilla.com>
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

// The test data for our database, note that the ordering of the results that
// will be returned by the query (the isInQuery: true objects) is IMPORTANT.
// see compareArrayToResult in head_queries.js for more info.
var testData = [
  // Normal folder
  { isInQuery: true, isFolder: true, title: "Folder 1",
    parentFolder: PlacesUtils.toolbarFolderId },

  // Read only folder
  { isInQuery: false, isFolder: true, title: "Folder 2 RO",
    parentFolder: PlacesUtils.toolbarFolderId, readOnly: true }
];

function run_test() {
  populateDB(testData);

  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.excludeQueries = true;
  options.excludeReadOnlyFolders = true;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  displayResultSet(root);
  // The readonly folder should not be in our result set.
  do_check_eq(1, root.childCount);
  do_check_eq("Folder 1", root.getChild(0).title);

  root.containerOpen = false;
}
