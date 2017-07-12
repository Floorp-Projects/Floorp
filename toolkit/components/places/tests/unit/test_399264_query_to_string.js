/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Obtains the id of the folder obtained from the query.
 *
 * @param aFolderID
 *        The id of the folder we want to generate a query for.
 * @returns the string representation of the query for the given folder.
 */
function query_string(aFolderID) {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);

  var query = hs.getNewQuery();
  query.setFolders([aFolderID], 1);
  var options = hs.getNewQueryOptions();
  return hs.queriesToQueryString([query], 1, options);
}

function run_test() {
  const QUERIES = [
      "folder=PLACES_ROOT",
      "folder=BOOKMARKS_MENU",
      "folder=TAGS",
      "folder=UNFILED_BOOKMARKS",
      "folder=TOOLBAR"
  ];
  const FOLDER_IDS = [
    PlacesUtils.placesRootId,
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.tagsFolderId,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.toolbarFolderId,
  ];

  for (var i = 0; i < QUERIES.length; i++) {
    var result = query_string(FOLDER_IDS[i]);
    dump("Looking for '" + QUERIES[i] + "' in '" + result + "'\n");
    do_check_neq(-1, result.indexOf(QUERIES[i]));
  }
}
