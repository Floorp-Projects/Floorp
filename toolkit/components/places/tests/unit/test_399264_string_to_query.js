/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Obtains the id of the folder obtained from the query.
 *
 * @param aQuery
 *        The query to obtain the folder id from.
 * @returns the folder id of the folder of the root node of the query.
 */
function folder_id(aQuery)
{
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);

  dump("Checking query '" + aQuery + "'\n");
  var options = { };
  var queries = { };
  var size = { };
  hs.queryStringToQueries(aQuery, queries, size, options);
  var result = hs.executeQueries(queries.value, size.value, options.value);
  var root = result.root;
  root.containerOpen = true;
  do_check_true(root.hasChildren);
  var folderID = root.getChild(0).parent.itemId;
  root.containerOpen = false;
  return folderID;
}

function run_test()
{
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);

  const QUERIES = [
      "place:folder=PLACES_ROOT"
    , "place:folder=BOOKMARKS_MENU"
    , "place:folder=TAGS"
    , "place:folder=UNFILED_BOOKMARKS"
    , "place:folder=TOOLBAR"
  ];
  const FOLDER_IDS = [
      bs.placesRoot
    , bs.bookmarksMenuFolder
    , bs.tagsFolder
    , bs.unfiledBookmarksFolder
    , bs.toolbarFolder
  ];

  // add something in the bookmarks menu folder so a query to it returns results
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://example.com/bmf/"),
                    Ci.nsINavBookmarksService.DEFAULT_INDEX, "bmf");

  // add something to the tags folder
  var ts = Cc["@mozilla.org/browser/tagging-service;1"].
           getService(Ci.nsITaggingService);
  ts.tagURI(uri("http://www.example.com/"), ["tag"]);

  // add something to the unfiled bookmarks folder
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri("http://example.com/ubf/"),
                    Ci.nsINavBookmarksService.DEFAULT_INDEX, "ubf");

  // add something to the toolbar folder
  bs.insertBookmark(bs.toolbarFolder, uri("http://example.com/tf/"),
                    Ci.nsINavBookmarksService.DEFAULT_INDEX, "tf");

  for (var i = 0; i < QUERIES.length; i++) {
    var result = folder_id(QUERIES[i]);
    dump("expected " + FOLDER_IDS[i] + ", got " + result + "\n");
    do_check_eq(FOLDER_IDS[i], result);
  }
}
