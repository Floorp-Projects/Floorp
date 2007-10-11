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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com>
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

// Get the history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
}
catch(ex) {
  do_throw("Could not get the history service\n");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
}
catch(ex) {
  do_throw("Could not get the nav-bookmarks-service\n");
}

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
}

// adds a test URI visit to the database
function add_visit(aURI, aTime) {
  histsvc.addVisit(aURI,
                    aTime,
                    0, // no referrer
                    histsvc.TRANSITION_TYPED, // user typed in URL bar
                    false, // not redirect
                    0);
}

// main
function run_test() {
  var testRoot = bmsvc.createFolder(bmsvc.placesRoot,
                                    "Result-sort functionality tests root",
                                    bmsvc.DEFAULT_INDEX);
  var uri1 = uri("http://foo.tld/a");
  var uri2 = uri("http://foo.tld/b");
  var id1 = bmsvc.insertBookmark(testRoot, uri1, bmsvc.DEFAULT_INDEX, "b");
  var id2 = bmsvc.insertBookmark(testRoot, uri2, bmsvc.DEFAULT_INDEX, "a");
  // url of id1, title of id2
  var id3 = bmsvc.insertBookmark(testRoot, uri1, bmsvc.DEFAULT_INDEX, "a");

  // query with natural order
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([testRoot], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  do_check_eq(root.childCount, 3);

  const NHQO = Ci.nsINavHistoryQueryOptions;

  function checkOrder(a, b, c) {
    do_check_eq(root.getChild(0).itemId, a);
    do_check_eq(root.getChild(1).itemId, b);
    do_check_eq(root.getChild(2).itemId, c);
  }

  // natural order
  checkOrder(id1, id2, id3);

  // title: id3 should precede id2 since we fall-back to URI-based sorting
  result.sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
  checkOrder(id3, id2, id1);

  // In reverse
  result.sortingMode = NHQO.SORT_BY_TITLE_DESCENDING;
  checkOrder(id1, id2, id3);

  // uri sort: id1 should precede id3 since we fall-back to natural order
  result.sortingMode = NHQO.SORT_BY_URI_ASCENDING;
  checkOrder(id1, id3, id2);

  // test live update
  bmsvc.changeBookmarkURI(id1, uri2);
  checkOrder(id3, id1, id2);
  bmsvc.changeBookmarkURI(id1, uri1);
  checkOrder(id1, id3, id2);

  // keyword sort
  result.sortingMode = NHQO.SORT_BY_KEYWORD_ASCENDING;
  checkOrder(id3, id2, id1);  // no keywords set - falling back to title sort
  bmsvc.setKeywordForBookmark(id1, "a");
  bmsvc.setKeywordForBookmark(id2, "z");
  checkOrder(id3, id1, id2);

  // XXXtodo: test history sortings (visit count, visit date)
  // XXXtodo: test different item types once folderId and bookmarkId are merged.
  // XXXtodo: test sortingAnnotation functionality with non-bookmark nodes

  annosvc.setItemAnnotation(id1, "testAnno", "a", 0, 0);
  annosvc.setItemAnnotation(id3, "testAnno", "b", 0, 0);
  result.sortingAnnotation = "testAnno";
  result.sortingMode = NHQO.SORT_BY_ANNOTATION_DESCENDING;

  // id1 precedes id2 per title-descending fallback
  checkOrder(id3, id1, id2);

  // XXXtodo:  test dateAdded sort
  // XXXtodo:  test lastModified sort
  
  // test live update
  annosvc.setItemAnnotation(id1, "testAnno", "c", 0, 0);
  checkOrder(id1, id3, id2);
}
