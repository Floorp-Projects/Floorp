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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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
